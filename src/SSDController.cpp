#include "SSDController.h"
#include <iostream>

// ─── Constructor ──────────────────────────────────────────────────────────────
//
// Initialisation order MUST match declaration order in the header:
//   flashMemory → mapper → stats → latency → cache → gc → lowWatermarkPercent
//
// GarbageCollector holds const-refs to flashMemory, mapper, stats, latency,
// so all four must be fully constructed before gc is initialised.

SSDController::SSDController(
    int          numBlocks,
    int          pagesPerBlock,
    double       lowWatermark,
    LatencyModel model,
    int          cacheSize
)
    : flashMemory(numBlocks, pagesPerBlock),
      mapper(),
      stats(),
      latency(model),
      cache(cacheSize),
      gc(flashMemory, mapper, stats, latency),
      lowWatermarkPercent(lowWatermark)
{}

// ─── setVerbose ───────────────────────────────────────────────────────────────

void SSDController::setVerbose(bool v) {
    gc.setVerbose(v);
}

// ─── nandWrite (private) ──────────────────────────────────────────────────────
//
// Writes `data` to a free NAND page for `lba`.  Handles:
//   • GC cycle if no free block available
//   • Free page selection (least-used block for wear leveling)
//   • NAND page program + physical-write + latency accounting
//   • Old physical page invalidation (out-of-place write semantics)
//   • FTL mapping update
//
// Does NOT call stats.recordLogicalWrite() — the public write() does that.
//
// Called from:
//   SSDController::write()  — when cache disabled, or dirty eviction from write
//   SSDController::read()   — dirty eviction during read-miss cache insertion
//   SSDController::flushCache() — end-of-simulation drain

void SSDController::nandWrite(int lba, const std::string& data) {

    // Trigger GC if free-page ratio is below the watermark
    if (gc.shouldTrigger(lowWatermarkPercent)) {
        gc.runOneCycle();
    }

    int freeBlock = flashMemory.findLeastUsedFreeBlock();

    if (freeBlock == -1) {
        // Watermark GC wasn't enough — force one more cycle
        std::cout << "[SSD] No free block found, forcing extra GC cycle...\n";
        gc.runOneCycle();
        freeBlock = flashMemory.findLeastUsedFreeBlock();

        if (freeBlock == -1) {
            std::cout << "ERROR: SSD FULL — write of LBA "
                      << lba << " dropped.\n";
            return;
        }
    }

    int freePage = flashMemory.getBlock(freeBlock).getFreePageIndex();

    // Program the NAND page
    flashMemory.getBlock(freeBlock)
               .getPage(freePage)
               .writeData(data);

    stats.recordPhysicalWrite();
    stats.chargeWriteLatency(latency.pageWriteUs);

    // Invalidate the previously-mapped physical page (out-of-place semantics)
    PhysicalAddress newAddr;
    newAddr.blockIndex = freeBlock;
    newAddr.pageIndex  = freePage;

    if (mapper.hasMapping(lba)) {
        PhysicalAddress oldAddr = mapper.removeMapping(lba);
        flashMemory.getBlock(oldAddr.blockIndex)
                   .getPage(oldAddr.pageIndex)
                   .invalidate();
    }

    // Update FTL: LBA → new physical address
    mapper.mapLogicalToPhysical(lba, newAddr);
}

// ─── Write ────────────────────────────────────────────────────────────────────

void SSDController::write(
    int logicalAddress,
    const std::string& data
) {
    stats.recordLogicalWrite();

    if (!cache.isEnabled()) {
        // ── No cache: direct NAND write (Phase 2 behaviour) ──────────────
        nandWrite(logicalAddress, data);
        return;
    }

    // ── Cache enabled ─────────────────────────────────────────────────────
    // Try to write into cache.  Two outcomes:
    //   (a) Cache hit (LBA already dirty in cache) → coalesced update, done.
    //   (b) Cache miss → new dirty entry inserted; if cache was full and the
    //       evicted entry was dirty → flush that evicted entry to NAND.

    PageCache::Entry evicted;
    bool mustFlush = cache.write(logicalAddress, data, evicted);

    if (mustFlush) {
        // Evicted dirty entry must be persisted to NAND
        nandWrite(evicted.lba, evicted.data);
    }
    // Coalesced writes (case a) and non-dirty evictions: no NAND action needed.
}

// ─── Read ─────────────────────────────────────────────────────────────────────

std::string SSDController::read(int logicalAddress) {

    stats.recordLogicalRead();

    if (cache.isEnabled()) {
        std::string cachedData;
        if (cache.read(logicalAddress, cachedData)) {
            // ── Cache hit: DRAM latency only ─────────────────────────────
            stats.chargeReadLatency(latency.dramReadUs);
            return cachedData;
        }
        // Cache miss: fall through to NAND
    }

    if (!mapper.hasMapping(logicalAddress)) {
        return "[ERROR: LBA " + std::to_string(logicalAddress) + " not mapped]";
    }

    PhysicalAddress addr = mapper.getPhysicalAddress(logicalAddress);

    // NAND read: charge full page-read latency
    stats.chargeReadLatency(latency.pageReadUs);

    std::string data = flashMemory.getBlock(addr.blockIndex)
                                  .getPage(addr.pageIndex)
                                  .readData();

    if (cache.isEnabled()) {
        // Insert clean entry; a dirty eviction here (unusual but possible if
        // cache is 100% dirty) must be flushed to NAND.
        PageCache::Entry evicted;
        if (cache.insert(logicalAddress, data, evicted)) {
            nandWrite(evicted.lba, evicted.data);
        }
    }

    return data;
}

// ─── flushCache ───────────────────────────────────────────────────────────────
//
// Drains all dirty cache entries to NAND.  Call at end of simulation to ensure
// all host-written data is persisted (mirrors a real SSD power-down flush).

void SSDController::flushCache() {

    if (!cache.isEnabled()) return;

    std::vector<PageCache::Entry> dirty = cache.flush();

    if (!dirty.empty()) {
        std::cout << "[Cache] Flushing " << dirty.size()
                  << " dirty entries to NAND...\n";
    }

    for (const auto& e : dirty) {
        nandWrite(e.lba, e.data);
    }
}

// ─── Manual GC ───────────────────────────────────────────────────────────────

void SSDController::garbageCollect() {
    gc.runOneCycle();
}

// ─── displayStatus ────────────────────────────────────────────────────────────

void SSDController::displayStatus() {

    std::cout << "\n===== SSD BLOCK STATUS =====\n";

    for (int i = 0; i < flashMemory.getTotalBlocks(); i++) {

        Block& block = flashMemory.getBlock(i);

        std::cout << "  Block " << i
                  << " | Free: "    << block.getFreePageCount()
                  << " | Valid: "   << block.getValidPageCount()
                  << " | Invalid: " << block.getInvalidPageCount()
                  << " | Erases: "  << block.getEraseCount()
                  << "\n";
    }

    int total = flashMemory.getTotalPageCount();
    int free  = flashMemory.getFreePageCount();

    std::cout << "  ─────────────────────────────────────\n";
    std::cout << "  Total pages : " << total
              << "  |  Free: "      << free
              << "  |  Used: "      << (total - free) << "\n";
    std::cout << "============================\n";
}

// ─── printStats ───────────────────────────────────────────────────────────────

void SSDController::printStats() const {
    stats.printReport();
    if (cache.isEnabled()) {
        cache.printCacheStats();
    }
}