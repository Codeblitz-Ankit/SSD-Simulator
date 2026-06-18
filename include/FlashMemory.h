#ifndef FLASHMEMORY_H
#define FLASHMEMORY_H

#include <vector>
#include "Block.h"

class FlashMemory {
private:
    std::vector<Block> blocks;

public:
    FlashMemory(int numBlocks, int pagesPerBlock);

    Block& getBlock(int index);

    int getTotalBlocks() const;

    int findFreeBlock() const;

    int findLeastUsedFreeBlock() const;
};

#endif