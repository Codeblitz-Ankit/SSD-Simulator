#ifndef FLASHMEMORY_H
#define FLASHMEMORY_H

#include <vector>
#include "Block.h"
#include "PhysicalAddress.h"

class FlashMemory {
private:
    std::vector<Block> blocks;

public:
    FlashMemory(int numBlocks, int pagesPerBlock);

    Block& getBlock(int index);

    int getTotalBlocks() const;

    int findFreeBlock() const;
    int findLeastUsedFreeBlock() const;

    // Find any free page NOT in excludedBlockIndex (used by GC for migration).
    // Returns PhysicalAddress with blockIndex = -1 if no slot found.
    PhysicalAddress findFreeSlot(int excludedBlockIndex) const;

    int getFreePageCount()  const;
    int getTotalPageCount() const;
};

#endif