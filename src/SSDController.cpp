#include "SSDController.h"
#include <iostream>

// ─── Constructor ─────────────────────────────────────────────────────────────
//
// Member initialisation order must match declaration order in the header:
//   flashMemory → mapper → stats → gc → lowWatermarkPercent
//
// GarbageCollector holds references to flashMemory, mapper, and stats, so those
// three must be fully constructed first — which is guaranteed by declaration order.

SSDController::SSDController(
    int    numBlocks,
    int    pagesPerBlock,
    double lowWatermark
)
    : flashMemory(numBlocks, pagesPerBlock),
      mapper(),
      stats(),
      gc(flashMemory, mapper, stats),
      lowWatermarkPercent(lowWatermark)
{}

// ─── Write ────────────────────────────────────────────────────────────────────

void SSDController::write(
    int logicalAddress,
    const std::string& data
) {
    // 1. Count this as one logical write from the host
    stats.recordLogicalWrite();

    // 2. Trigger GC if free-page ratio has dropped below the watermark
    if (gc.shouldTrigger(lowWatermarkPercent)) {
        gc.runOneCycle();
    }

    // 3. Find a free block (prefer lowest erase count → basic wear levelling)
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

    int freePage =
        flashMemory.getBlock(freeBlock).getFreePageIndex();

    // 4. Program the page
    flashMemory.getBlock(freeBlock)
               .getPage(freePage)
               .writeData(data);

    stats.recordPhysicalWrite();   // host write counts as one physical write

    // 5. Invalidate the previously mapped physical page (out-of-place update)
    PhysicalAddress newAddr;
    newAddr.blockIndex = freeBlock;
    newAddr.pageIndex  = freePage;

    if (mapper.hasMapping(logicalAddress)) {

        PhysicalAddress oldAddr = mapper.removeMapping(logicalAddress);

        flashMemory.getBlock(oldAddr.blockIndex)
                   .getPage(oldAddr.pageIndex)
                   .invalidate();
    }

    // 6. Update FTL mapping: LBA → new physical address
    mapper.mapLogicalToPhysical(logicalAddress, newAddr);
}

// ─── Read ─────────────────────────────────────────────────────────────────────

std::string SSDController::read(
    int logicalAddress
) {
    if (!mapper.hasMapping(logicalAddress)) {
        return "[ERROR: LBA " + std::to_string(logicalAddress) + " not mapped]";
    }

    PhysicalAddress addr = mapper.getPhysicalAddress(logicalAddress);

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