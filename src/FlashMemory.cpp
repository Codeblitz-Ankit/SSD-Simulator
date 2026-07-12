#include "FlashMemory.h"
#include <climits>
#include <cmath>    // sqrt for variance

// ─── Constructor ──────────────────────────────────────────────────────────────

FlashMemory::FlashMemory(int numBlocks, int pagesPerBlock, int peLimit) {
    for (int i = 0; i < numBlocks; i++) {
        blocks.push_back(Block(pagesPerBlock, peLimit));
    }
}

// ─── Block accessors ─────────────────────────────────────────────────────────

Block& FlashMemory::getBlock(int index) {
    return blocks[index];
}

const Block& FlashMemory::getBlock(int index) const {
    return blocks[index];
}

int FlashMemory::getTotalBlocks() const {
    return static_cast<int>(blocks.size());
}

// ─── Free block search ───────────────────────────────────────────────────────

int FlashMemory::findFreeBlock() const {
    for (int i = 0; i < (int)blocks.size(); i++) {
        if (blocks[i].getFreePageIndex() != -1) {
            return i;
        }
    }
    return -1;
}

int FlashMemory::findLeastUsedFreeBlock() const {
    int bestBlock       = -1;
    int lowestEraseCount = INT_MAX;

    for (int i = 0; i < (int)blocks.size(); i++) {
        if (blocks[i].getFreePageIndex() != -1) {
            if (blocks[i].getEraseCount() < lowestEraseCount) {
                lowestEraseCount = blocks[i].getEraseCount();
                bestBlock        = i;
            }
        }
    }
    return bestBlock;
}

// ─── findFreeSlot ─────────────────────────────────────────────────────────────

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

    PhysicalAddress none;
    none.blockIndex = -1;
    none.pageIndex  = -1;
    return none;
}

// ─── Page count helpers ───────────────────────────────────────────────────────

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

// ─── Phase 3: Erase-count statistics ─────────────────────────────────────────

double FlashMemory::getAvgEraseCount() const {
    if (blocks.empty()) return 0.0;
    long long total = 0;
    for (const auto& b : blocks) total += b.getEraseCount();
    return static_cast<double>(total) / blocks.size();
}

double FlashMemory::getEraseCountVariance() const {
    if (blocks.empty()) return 0.0;
    double mean = getAvgEraseCount();
    double sumSq = 0.0;
    for (const auto& b : blocks) {
        double diff = b.getEraseCount() - mean;
        sumSq += diff * diff;
    }
    return sumSq / blocks.size();   // population variance
}

int FlashMemory::getMinEraseCount() const {
    if (blocks.empty()) return 0;
    int mn = INT_MAX;
    for (const auto& b : blocks) mn = std::min(mn, b.getEraseCount());
    return mn;
}

int FlashMemory::getMaxEraseCount() const {
    if (blocks.empty()) return 0;
    int mx = 0;
    for (const auto& b : blocks) mx = std::max(mx, b.getEraseCount());
    return mx;
}

int FlashMemory::getWornOutCount() const {
    int count = 0;
    for (const auto& b : blocks)
        if (b.isWornOut()) count++;
    return count;
}

int FlashMemory::getPELimit() const {
    if (blocks.empty()) return 0;
    return blocks[0].getPELimit();
}

// ─── Phase 3: Static wear-leveling helpers ────────────────────────────────────

// The fully-free block with the HIGHEST erase count is the wear-leveling
// *destination* — worn-out free blocks store cold data so they're taken out
// of the heavy-write rotation, letting dynamic WL pick fresher blocks next.

int FlashMemory::findMostWornFreeBlock() const {
    int bestBlock   = -1;
    int highestErase = -1;

    for (int i = 0; i < (int)blocks.size(); i++) {
        // Must be completely free (all pages erased, no valid data)
        if (blocks[i].getFreePageCount() != blocks[i].getTotalPages()) continue;

        if (blocks[i].getEraseCount() > highestErase) {
            highestErase = blocks[i].getEraseCount();
            bestBlock    = i;
        }
    }
    return bestBlock;
}

// The block with at least 1 valid page and the LOWEST erase count is the
// wear-leveling *source* — moving its cold data frees a fresh block for
// hot-write traffic and gives that block's low-erase "credit" to the write pool.

int FlashMemory::findLeastWornDataBlock() const {
    int bestBlock  = -1;
    int lowestErase = INT_MAX;

    for (int i = 0; i < (int)blocks.size(); i++) {
        // Must contain at least one valid page (cold data worth migrating)
        if (blocks[i].getValidPageCount() == 0) continue;
        // Exclude fully-free blocks (no valid data to migrate)
        if (blocks[i].getFreePageCount() == blocks[i].getTotalPages()) continue;

        if (blocks[i].getEraseCount() < lowestErase) {
            lowestErase = blocks[i].getEraseCount();
            bestBlock   = i;
        }
    }
    return bestBlock;
}