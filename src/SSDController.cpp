#include "SSDController.h"
#include <iostream>
#include <iomanip>
#include <cmath>    // sqrt for std-dev display
#include <string>
#include <climits>

// ─── Constructor ──────────────────────────────────────────────────────────────

SSDController::SSDController(
    int          numBlocks,
    int          pagesPerBlock,
    double       lowWatermark,
    LatencyModel model,
    int          cacheSize,
    int          peLimit
)
    : flashMemory(numBlocks, pagesPerBlock, peLimit),
      mapper(),
      stats(),
      latency(model),
      cache(cacheSize),
      gc(flashMemory, mapper, stats, latency),
      lowWatermarkPercent(lowWatermark)
{}

// ─── Configuration ────────────────────────────────────────────────────────────

void SSDController::setVerbose(bool v) {
    gc.setVerbose(v);
}

void SSDController::setWearLevelInterval(int n) {
    gc.setWearLevelInterval(n);
}

// ─── nandWrite (private) ──────────────────────────────────────────────────────

void SSDController::nandWrite(int lba, const std::string& data) {

    if (gc.shouldTrigger(lowWatermarkPercent)) {
        gc.runOneCycle();
    }

    int freeBlock = flashMemory.findLeastUsedFreeBlock();

    if (freeBlock == -1) {
        std::cout << "[SSD] No free block — forcing extra GC cycle...\n";
        gc.runOneCycle();
        freeBlock = flashMemory.findLeastUsedFreeBlock();

        if (freeBlock == -1) {
            std::cout << "ERROR: SSD FULL — write of LBA " << lba << " dropped.\n";
            return;
        }
    }

    int freePage = flashMemory.getBlock(freeBlock).getFreePageIndex();

    flashMemory.getBlock(freeBlock)
               .getPage(freePage)
               .writeData(data);

    stats.recordPhysicalWrite();
    stats.chargeWriteLatency(latency.pageWriteUs);

    PhysicalAddress newAddr;
    newAddr.blockIndex = freeBlock;
    newAddr.pageIndex  = freePage;

    if (mapper.hasMapping(lba)) {
        PhysicalAddress oldAddr = mapper.removeMapping(lba);
        flashMemory.getBlock(oldAddr.blockIndex)
                   .getPage(oldAddr.pageIndex)
                   .invalidate();
    }

    mapper.mapLogicalToPhysical(lba, newAddr);
}

// ─── Write ────────────────────────────────────────────────────────────────────

void SSDController::write(int logicalAddress, const std::string& data) {

    stats.recordLogicalWrite();

    if (!cache.isEnabled()) {
        nandWrite(logicalAddress, data);
        return;
    }

    PageCache::Entry evicted;
    if (cache.write(logicalAddress, data, evicted)) {
        nandWrite(evicted.lba, evicted.data);
    }
}

// ─── Read ─────────────────────────────────────────────────────────────────────

std::string SSDController::read(int logicalAddress) {

    stats.recordLogicalRead();

    if (cache.isEnabled()) {
        std::string cachedData;
        if (cache.read(logicalAddress, cachedData)) {
            stats.chargeReadLatency(latency.dramReadUs);
            return cachedData;
        }
    }

    if (!mapper.hasMapping(logicalAddress)) {
        return "[ERROR: LBA " + std::to_string(logicalAddress) + " not mapped]";
    }

    PhysicalAddress addr = mapper.getPhysicalAddress(logicalAddress);
    stats.chargeReadLatency(latency.pageReadUs);

    std::string data = flashMemory.getBlock(addr.blockIndex)
                                  .getPage(addr.pageIndex)
                                  .readData();

    if (cache.isEnabled()) {
        PageCache::Entry evicted;
        if (cache.insert(logicalAddress, data, evicted)) {
            nandWrite(evicted.lba, evicted.data);
        }
    }

    return data;
}

// ─── flushCache ───────────────────────────────────────────────────────────────

void SSDController::flushCache() {

    if (!cache.isEnabled()) return;

    auto dirty = cache.flush();
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
        std::cout << "  Block " << std::setw(2) << i
                  << " | Free: "    << block.getFreePageCount()
                  << " | Valid: "   << block.getValidPageCount()
                  << " | Invalid: " << block.getInvalidPageCount()
                  << " | Erases: "  << block.getEraseCount()
                  << (block.isWornOut() ? "  *** WORN OUT ***" : "")
                  << "\n";
    }

    int total = flashMemory.getTotalPageCount();
    int free  = flashMemory.getFreePageCount();
    std::cout << "  ─────────────────────────────────────────\n";
    std::cout << "  Total pages: " << total
              << "  |  Free: "     << free
              << "  |  Used: "     << (total - free) << "\n";
    std::cout << "============================\n";
}

// ─── displayWearAnalysis ──────────────────────────────────────────────────────
//
// Phase 3: Prints an ASCII erase-count bar chart for every block, plus
// statistical summary (mean, variance, min, max) and a lifespan estimate.

void SSDController::displayWearAnalysis() const {

    const int peLimit  = flashMemory.getPELimit();
    const int nBlocks  = flashMemory.getTotalBlocks();
    const int maxErase = flashMemory.getMaxEraseCount();
    const int minErase = flashMemory.getMinEraseCount();
    const int wornOut  = flashMemory.getWornOutCount();
    const double avgE  = flashMemory.getAvgEraseCount();
    const double varE  = flashMemory.getEraseCountVariance();
    const double stdE  = std::sqrt(varE);

    const int BAR_WIDTH = 40;  // max bar length in characters

    std::cout << "\n";
    std::cout << "========================================================\n";
    std::cout << "               WEAR LEVEL ANALYSIS\n";
    std::cout << "========================================================\n";
    std::cout << "  P/E Limit : " << peLimit << " cycles"
              << "   |  Worn Out: " << wornOut << " / " << nBlocks << " blocks\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Mean      : " << avgE
              << "   |  Std Dev : " << stdE
              << "   |  Variance: " << std::setprecision(2) << varE << "\n";
    std::cout << "  Min       : " << minErase
              << "   |  Max     : " << maxErase
              << "   |  Spread  : " << (maxErase - minErase) << " cycles\n";
    std::cout << "  ──────────────────────────────────────────────────────\n";
    std::cout << "  Block | Erases |  Wear% |  Bar\n";
    std::cout << "  ──────+--------+--------+"
              << std::string(BAR_WIDTH + 1, '-') << "\n";

    for (int i = 0; i < nBlocks; i++) {
        const Block& b  = flashMemory.getBlock(i);
        int          ec = b.getEraseCount();
        double       wr = b.getWearRatio() * 100.0;

        // Normalise bar length relative to peLimit (not maxErase) so the
        // full bar width always represents "fully worn out".
        int barLen = (peLimit > 0)
                     ? static_cast<int>((double)ec / peLimit * BAR_WIDTH)
                     : 0;
        barLen = std::min(barLen, BAR_WIDTH);

        char barChar = b.isWornOut() ? 'X' : '|';

        std::cout << "  " << std::setw(5) << i
                  << " | " << std::setw(6) << ec
                  << " | " << std::setw(5) << std::fixed
                  << std::setprecision(1) << wr << "% |"
                  << std::string(barLen, barChar)
                  << (b.isWornOut() ? " WORN" : "")
                  << "\n";
    }

    std::cout << "  ──────────────────────────────────────────────────────\n";

    // ── Lifespan estimate ─────────────────────────────────────────────────

    long long rem = stats.estimateRemainingWrites(
        peLimit, maxErase, flashMemory.getTotalPageCount());

    std::cout << "  Lifespan estimate (conservative — worst block):\n";
    if (rem == LLONG_MAX) {
        std::cout << "  → No writes recorded yet — cannot estimate.\n";
    } else if (rem == 0) {
        std::cout << "  → *** WORN OUT — most-worn block at P/E limit! ***\n";
    } else {
        std::cout << "  → ~" << rem << " more host writes before first block"
                  << " reaches P/E limit (" << peLimit << ").\n";
    }
    std::cout << "========================================================\n\n";
}

// ─── printStats ───────────────────────────────────────────────────────────────

void SSDController::printStats() const {
    stats.printReport();
    if (cache.isEnabled()) {
        cache.printCacheStats();
    }
}