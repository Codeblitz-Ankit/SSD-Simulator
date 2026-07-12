#ifndef FLASHMEMORY_H
#define FLASHMEMORY_H

#include <vector>
#include "Block.h"
#include "PhysicalAddress.h"

// ─────────────────────────────────────────────────────────────────────────────
// FlashMemory — Physical NAND array
//
// Phase 3 additions:
//   Constructor now accepts peLimit (passed to every Block).
//   Erase-count statistics: avg, variance, min, max, wornOutCount.
//   Wear-leveling helpers for GarbageCollector:
//     findMostWornFreeBlock()  — fully-free block with highest erase count
//                                (destination for cold data migration)
//     findLeastWornDataBlock() — block with valid data + lowest erase count
//                                (source of cold data to move out)
//   const Block& getBlock(int) const — needed for const analysis methods.
// ─────────────────────────────────────────────────────────────────────────────

class FlashMemory {
private:
    std::vector<Block> blocks;

public:
    // peLimit: P/E cycle endurance per block (default 3000 = MLC NAND)
    FlashMemory(int numBlocks, int pagesPerBlock, int peLimit = 3000);

    Block&       getBlock(int index);
    const Block& getBlock(int index) const;  // Phase 3: for const methods

    int getTotalBlocks() const;

    int findFreeBlock()          const;
    int findLeastUsedFreeBlock() const;

    // Find any free page NOT in excludedBlockIndex (used by GC for migration).
    PhysicalAddress findFreeSlot(int excludedBlockIndex) const;

    int getFreePageCount()  const;
    int getTotalPageCount() const;

    // ── Phase 3: Erase-count statistics ───────────────────────────────────

    double getAvgEraseCount()      const;  // arithmetic mean
    double getEraseCountVariance() const;  // population variance
    int    getMinEraseCount()      const;
    int    getMaxEraseCount()      const;
    int    getWornOutCount()       const;  // blocks with eraseCount >= peLimit
    int    getPELimit()            const;  // (all blocks share the same limit)

    // ── Phase 3: Static wear-leveling helpers ─────────────────────────────

    // Returns the index of the fully-free block (all pages free) with the
    // HIGHEST erase count, or -1 if no fully-free block exists.
    // This is the *destination* for cold data: high-wear blocks store cold data
    // so they are taken out of the heavy-write rotation.
    int findMostWornFreeBlock() const;

    // Returns the index of the block with at least 1 valid page and LOWEST
    // erase count, or -1 if no such block exists.
    // This is the *source* of cold data: moving it frees up a fresh block for
    // hot-write traffic, redistributing wear.
    int findLeastWornDataBlock() const;
};

#endif