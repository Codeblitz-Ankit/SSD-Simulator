#include "FlashMemory.h"

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
    int lowestEraseCount = 999999;

    for (int i = 0; i < blocks.size(); i++) {

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