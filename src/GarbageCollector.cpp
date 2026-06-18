#include "GarbageCollector.h"
#include <iostream>

GarbageCollector::GarbageCollector(
    FlashMemory&  flash,
    FTLMapper&    mapper,
    StatsTracker& stats
)
    : flash(flash),
      mapper(mapper),
      stats(stats) {}

// ─── Victim Selection ────────────────────────────────────────────────────────

int GarbageCollector::selectVictimBlock() const {

    int bestBlock   = -1;
    int mostInvalid = 0;    // only consider blocks with >= 1 invalid page

    for (int i = 0; i < flash.getTotalBlocks(); i++) {

        int inv = flash.getBlock(i).getInvalidPageCount();

        if (inv > mostInvalid) {
            mostInvalid = inv;
            bestBlock   = i;
        }
    }

    return bestBlock;
}

// ─── Watermark Check ─────────────────────────────────────────────────────────

bool GarbageCollector::shouldTrigger(double lowWatermarkPercent) const {

    int total = flash.getTotalPageCount();
    if (total == 0) return false;

    int free = flash.getFreePageCount();
    return (static_cast<double>(free) / total) < lowWatermarkPercent;
}

// ─── Main GC Cycle ───────────────────────────────────────────────────────────

bool GarbageCollector::runOneCycle() {

    int victimIdx = selectVictimBlock();

    if (victimIdx == -1) {
        std::cout << "[GC] No reclaimable block found — skipping.\n";
        return false;
    }

    Block& victim = flash.getBlock(victimIdx);

    std::cout << "[GC] >>> Cycle triggered. "
              << "Victim = Block " << victimIdx
              << "  (invalid=" << victim.getInvalidPageCount()
              << ", valid="   << victim.getValidPageCount()
              << ", free="    << victim.getFreePageCount()
              << ")\n";

    stats.recordGCInvocation();

    // ── Step 1: Migrate valid pages out of victim block ──────────────────────
    for (int p = 0; p < victim.getTotalPages(); p++) {

        Page& page = victim.getPage(p);

        if (page.getIsFree() || !page.getIsValid()) {
            continue;   // skip free and already-invalid pages
        }

        // Reverse-lookup: which LBA maps to this physical location?
        PhysicalAddress srcAddr;
        srcAddr.blockIndex = victimIdx;
        srcAddr.pageIndex  = p;

        int lba = mapper.findLogicalAddress(srcAddr);

        if (lba == -1) {
            // Orphaned page — no active LBA points here (data-race guard)
            std::cout << "[GC]   WARNING: orphaned valid page at ("
                      << victimIdx << "," << p << ") — skipping.\n";
            continue;
        }

        // Find a free slot in any block OTHER than the victim
        PhysicalAddress dst = flash.findFreeSlot(victimIdx);

        if (dst.blockIndex == -1) {
            std::cout << "[GC]   ERROR: no free slot for migration of LBA "
                      << lba << " — aborting cycle.\n";
            return false;
        }

        // Copy data to the new location
        std::string data = page.readData();
        flash.getBlock(dst.blockIndex)
             .getPage(dst.pageIndex)
             .writeData(data);

        // Update FTL mapping:  LBA → new physical address
        mapper.removeMapping(lba);
        mapper.mapLogicalToPhysical(lba, dst);

        stats.recordPageMigrated();

        std::cout << "[GC]   Migrated LBA " << lba
                  << " : (" << victimIdx << "," << p << ")"
                  << " -> ("  << dst.blockIndex << "," << dst.pageIndex << ")\n";
    }

    // ── Step 2: Erase victim block ────────────────────────────────────────────
    victim.eraseBlock();

    std::cout << "[GC] <<< Block " << victimIdx
              << " erased. Erase count = " << victim.getEraseCount() << "\n\n";

    return true;
}
