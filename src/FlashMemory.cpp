#include "FlashMemory.h"
#include <climits>

FlashMemory::FlashMemory(int numBlocks, int pagesPerBlock) {

    for (int i = 0; i < numBlocks; i++) {
        blocks.push_back(Block(pagesPerBlock));
    }
}

Block& FlashMemory::getBlock(int index) {
    return blocks[index];
}

int FlashMemory::getTotalBlocks() const {
    return blocks.size();
}

int FlashMemory::findFreeBlock() const {

    for (int i = 0; i < blocks.size(); i++) {

        if (blocks[i].getFreePageIndex() != -1) {
            return i;
        }
    }

    return -1;
}

int FlashMemory::findLeastUsedFreeBlock() const {

    int bestBlock = -1;
    int lowestEraseCount = INT_MAX;

    for (int i = 0; i < (int)blocks.size(); i++) {

        if (blocks[i].getFreePageIndex() != -1) {

            if (blocks[i].getEraseCount()
                < lowestEraseCount) {

                lowestEraseCount =
                    blocks[i].getEraseCount();

                bestBlock = i;
            }
        }
    }

    return bestBlock;
}

// Finds the first free page slot in any block except excludedBlockIndex.
// Used by GarbageCollector to migrate valid pages out of the victim block.
PhysicalAddress FlashMemory::findFreeSlot(int excludedBlockIndex) const {

    for (int i = 0; i < (int)blocks.size(); i++) {

        if (i == excludedBlockIndex) continue;

        int fp = blocks[i].getFreePageIndex();

        if (fp != -1) {
            PhysicalAddress addr;
            addr.blockIndex = i;
            addr.pageIndex  = fp;
            return addr;
        }
    }

    // Sentinel: no free slot found
    PhysicalAddress none;
    none.blockIndex = -1;
    none.pageIndex  = -1;
    return none;
}

int FlashMemory::getFreePageCount() const {

    int count = 0;

    for (int i = 0; i < (int)blocks.size(); i++) {
        count += blocks[i].getFreePageCount();
    }

    return count;
}

int FlashMemory::getTotalPageCount() const {

    int count = 0;

    for (int i = 0; i < (int)blocks.size(); i++) {
        count += blocks[i].getTotalPages();
    }

    return count;
}