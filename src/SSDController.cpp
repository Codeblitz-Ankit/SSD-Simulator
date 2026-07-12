#include "SSDController.h"
#include <iostream>

// ─── Constructor ──────────────────────────────────────────────────────────────
//
// Initialisation order MUST match declaration order in the header:
//   flashMemory → mapper → stats → latency → gc → lowWatermarkPercent
//
// GarbageCollector holds const-refs to flashMemory, mapper, stats, and latency,
// so all four must be fully constructed before gc is initialised.

SSDController::SSDController(
    int          numBlocks,
    int          pagesPerBlock,
    double       lowWatermark,
    LatencyModel model
)
    : flashMemory(numBlocks, pagesPerBlock),
      mapper(),
      stats(),
      latency(model),
      gc(flashMemory, mapper, stats, latency),
      lowWatermarkPercent(lowWatermark)
{}

// ─── setVerbose ───────────────────────────────────────────────────────────────

void SSDController::setVerbose(bool v) {
    gc.setVerbose(v);
}

// ─── Write ────────────────────────────────────────────────────────────────────

void SSDController::write(
    int logicalAddress,
    const std::string& data
) {
    // 1. Count this as one logical write from the host
    stats.recordLogicalWrite();

    // 2. Trigger GC if free-page ratio has dropped below the watermark.
    //    GC charges its own latency (erase + migration) into stats.
    if (gc.shouldTrigger(lowWatermarkPercent)) {
        gc.runOneCycle();
    }

    // 3. Find a free block (prefer lowest erase count → dynamic wear leveling)
    int freeBlock = flashMemory.findLeastUsedFreeBlock();

    if (freeBlock == -1) {
        // Watermark GC wasn't enough — run one more forced cycle
        std::cout << "[SSD] No free block found, forcing extra GC cycle...\n";
        gc.runOneCycle();
        freeBlock = flashMemory.findLeastUsedFreeBlock();

        if (freeBlock == -1) {
            std::cout << "ERROR: SSD FULL — write of LBA "
                      << logicalAddress << " dropped.\n";
            return;
        }
    }

    int freePage = flashMemory.getBlock(freeBlock).getFreePageIndex();

    // 4. Program the NAND page
    flashMemory.getBlock(freeBlock)
               .getPage(freePage)
               .writeData(data);

    // 5. Charge timing: one page program for this host write
    stats.recordPhysicalWrite();
    stats.chargeWriteLatency(latency.pageWriteUs);   // Phase 2

    // 6. Invalidate the previously mapped physical page (out-of-place update)
    PhysicalAddress newAddr;
    newAddr.blockIndex = freeBlock;
    newAddr.pageIndex  = freePage;

    if (mapper.hasMapping(logicalAddress)) {
        PhysicalAddress oldAddr = mapper.removeMapping(logicalAddress);
        flashMemory.getBlock(oldAddr.blockIndex)
                   .getPage(oldAddr.pageIndex)
                   .invalidate();
    }

    // 7. Update FTL: LBA → new physical address
    mapper.mapLogicalToPhysical(logicalAddress, newAddr);
}

// ─── Read ─────────────────────────────────────────────────────────────────────

std::string SSDController::read(int logicalAddress) {

    // Count host read, charge NAND read latency (Phase 2)
    stats.recordLogicalRead();

    if (!mapper.hasMapping(logicalAddress)) {
        return "[ERROR: LBA " + std::to_string(logicalAddress) + " not mapped]";
    }

    PhysicalAddress addr = mapper.getPhysicalAddress(logicalAddress);

    stats.chargeReadLatency(latency.pageReadUs);   // Phase 2

    return flashMemory.getBlock(addr.blockIndex)
                      .getPage(addr.pageIndex)
                      .readData();
}

// ─── Manual GC trigger ────────────────────────────────────────────────────────

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
}