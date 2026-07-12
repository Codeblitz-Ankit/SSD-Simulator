#include "GarbageCollector.h"
#include <iostream>

// ─── Constructor ──────────────────────────────────────────────────────────────

GarbageCollector::GarbageCollector(
    FlashMemory&        flash,
    FTLMapper&          mapper,
    StatsTracker&       stats,
    const LatencyModel& latency,
    int                 wearLevelInterval
)
    : flash(flash),
      mapper(mapper),
      stats(stats),
      latency(latency),
      verbose(true),
      gcCyclesRun(0),
      wearLevelInterval(wearLevelInterval)
{}

// ─── Victim Selection ─────────────────────────────────────────────────────────

int GarbageCollector::selectVictimBlock() const {
    int bestBlock   = -1;
    int mostInvalid = 0;

    for (int i = 0; i < flash.getTotalBlocks(); i++) {
        int inv = flash.getBlock(i).getInvalidPageCount();
        if (inv > mostInvalid) {
            mostInvalid = inv;
            bestBlock   = i;
        }
    }
    return bestBlock;
}

// ─── Watermark Check ──────────────────────────────────────────────────────────

bool GarbageCollector::shouldTrigger(double lowWatermarkPercent) const {
    int total = flash.getTotalPageCount();
    if (total == 0) return false;
    return (static_cast<double>(flash.getFreePageCount()) / total)
           < lowWatermarkPercent;
}

// ─── Regular GC Cycle ─────────────────────────────────────────────────────────

bool GarbageCollector::runOneCycle() {

    int victimIdx = selectVictimBlock();

    if (victimIdx == -1) {
        if (verbose) std::cout << "[GC] No reclaimable block — skipping.\n";
        return false;
    }

    Block& victim = flash.getBlock(victimIdx);

    if (verbose) {
        std::cout << "[GC] >>> Cycle " << (gcCyclesRun + 1)
                  << "  Victim=Block " << victimIdx
                  << " (invalid=" << victim.getInvalidPageCount()
                  << ", valid="   << victim.getValidPageCount()
                  << ", erases="  << victim.getEraseCount()
                  << ")\n";
    }

    stats.recordGCInvocation();

    // ── Migrate valid pages ───────────────────────────────────────────────

    for (int p = 0; p < victim.getTotalPages(); p++) {

        Page& page = victim.getPage(p);
        if (page.getIsFree() || !page.getIsValid()) continue;

        PhysicalAddress srcAddr;
        srcAddr.blockIndex = victimIdx;
        srcAddr.pageIndex  = p;

        int lba = mapper.findLogicalAddress(srcAddr);
        if (lba == -1) {
            if (verbose) {
                std::cout << "[GC]   Orphaned page at ("
                          << victimIdx << "," << p << ") — skipping.\n";
            }
            continue;
        }

        PhysicalAddress dst = flash.findFreeSlot(victimIdx);
        if (dst.blockIndex == -1) {
            if (verbose) {
                std::cout << "[GC]   No free slot for LBA " << lba
                          << " — aborting.\n";
            }
            return false;
        }

        flash.getBlock(dst.blockIndex)
             .getPage(dst.pageIndex)
             .writeData(page.readData());

        mapper.removeMapping(lba);
        mapper.mapLogicalToPhysical(lba, dst);

        stats.recordPageMigrated();
        stats.chargeGCLatency(latency.pageWriteUs);

        if (verbose) {
            std::cout << "[GC]   Migrated LBA " << lba
                      << " (" << victimIdx << "," << p << ")"
                      << " → (" << dst.blockIndex << "," << dst.pageIndex << ")\n";
        }
    }

    // ── Erase victim ──────────────────────────────────────────────────────

    victim.eraseBlock();
    stats.chargeGCLatency(latency.blockEraseUs);

    if (verbose) {
        std::cout << "[GC] <<< Block " << victimIdx
                  << " erased (erase#" << victim.getEraseCount() << ")\n\n";
    }

    // ── Phase 3: Auto-trigger static wear leveling ────────────────────────

    gcCyclesRun++;

    if (wearLevelInterval > 0 && gcCyclesRun % wearLevelInterval == 0) {
        staticWearLevel();
    }

    return true;
}

// ─── Static Wear Leveling ─────────────────────────────────────────────────────
//
// Moves cold data from the freshest (lowest-erase) data block to the most
// worn free block, then erases the freshest block so dynamic WL can use it
// for hot writes.  Net effect: reduces erase-count variance across blocks.

bool GarbageCollector::staticWearLevel() {

    int srcIdx = flash.findLeastWornDataBlock();   // cold data source
    int dstIdx = flash.findMostWornFreeBlock();    // worn free destination

    if (srcIdx == -1 || dstIdx == -1) {
        if (verbose) {
            std::cout << "[WL] Static WL: no suitable src/dst pair — skipping.\n";
        }
        return false;
    }

    int eraseGap = flash.getBlock(dstIdx).getEraseCount()
                 - flash.getBlock(srcIdx).getEraseCount();

    if (eraseGap < WEAR_LEVEL_THRESHOLD) {
        if (verbose) {
            std::cout << "[WL] Static WL: gap=" << eraseGap
                      << " < threshold=" << WEAR_LEVEL_THRESHOLD
                      << " — skipping.\n";
        }
        return false;
    }

    Block& src = flash.getBlock(srcIdx);
    Block& dst = flash.getBlock(dstIdx);

    if (verbose) {
        std::cout << "[WL] >>> Static WL: src=Block " << srcIdx
                  << " (erases=" << src.getEraseCount() << ", valid="
                  << src.getValidPageCount() << ")"
                  << "  dst=Block " << dstIdx
                  << " (erases=" << dst.getEraseCount() << ")"
                  << "  gap=" << eraseGap << "\n";
    }

    int pagesMoved = 0;

    for (int p = 0; p < src.getTotalPages(); p++) {

        Page& page = src.getPage(p);
        if (page.getIsFree() || !page.getIsValid()) continue;

        // Reverse-lookup: find which LBA maps to this physical page
        PhysicalAddress srcAddr;
        srcAddr.blockIndex = srcIdx;
        srcAddr.pageIndex  = p;

        int lba = mapper.findLogicalAddress(srcAddr);
        if (lba == -1) {
            if (verbose) {
                std::cout << "[WL]   Orphaned page at ("
                          << srcIdx << "," << p << ") — skipping.\n";
            }
            continue;
        }

        // Find a free slot in dst block
        int fp = dst.getFreePageIndex();
        if (fp == -1) {
            if (verbose) {
                std::cout << "[WL]   dst Block " << dstIdx
                          << " full — aborting migration.\n";
            }
            break;
        }

        // Copy data: src → dst
        dst.getPage(fp).writeData(page.readData());

        // Update FTL: LBA → new physical location
        PhysicalAddress dstAddr;
        dstAddr.blockIndex = dstIdx;
        dstAddr.pageIndex  = fp;

        mapper.removeMapping(lba);
        mapper.mapLogicalToPhysical(lba, dstAddr);

        // Charge NAND program latency (migration is a real page program)
        stats.recordPageMigrated();
        stats.chargeGCLatency(latency.pageWriteUs);

        pagesMoved++;

        if (verbose) {
            std::cout << "[WL]   Moved LBA " << lba
                      << " (" << srcIdx << "," << p << ")"
                      << " → (" << dstIdx << "," << fp << ")\n";
        }
    }

    // Erase the freshly-emptied src block → now fresh free for write traffic
    src.eraseBlock();
    stats.chargeGCLatency(latency.blockEraseUs);

    // Record the static WL operation
    stats.recordStaticWearLevel();

    if (verbose) {
        std::cout << "[WL] <<< src Block " << srcIdx
                  << " erased (erase#" << src.getEraseCount()
                  << "). Moved " << pagesMoved << " pages. "
                  << "Erase variance now improving.\n\n";
    }

    return true;
}
