#include "Block.h"

Block::Block(int numPages, int peLimit)
    : eraseCount(0),
      peLimit(peLimit)
{
    for (int i = 0; i < numPages; i++) {
        pages.push_back(Page());
    }
}

Page& Block::getPage(int index) {
    return pages[index];
}

const Page& Block::getPage(int index) const {
    return pages[index];
}

int Block::getFreePageIndex() const {

    for (int i = 0; i < pages.size(); i++) {

        if (pages[i].getIsFree()) {
            return i;
        }
    }

    return -1;
}

int Block::getEraseCount() const {
    return eraseCount;
}

int Block::getPELimit() const {
    return peLimit;
}

bool Block::isWornOut() const {
    return eraseCount >= peLimit;
}

double Block::getWearRatio() const {
    if (peLimit <= 0) return 1.0;
    return static_cast<double>(eraseCount) / peLimit;
}

void Block::eraseBlock() {

    for (int i = 0; i < pages.size(); i++) {
        pages[i] = Page();
    }

    eraseCount++;
}

int Block::getTotalPages() const {
    return pages.size();
}

int Block::getInvalidPageCount() const {

    int count = 0;

    for (int i = 0; i < pages.size(); i++) {

        if (!pages[i].getIsFree() &&
            !pages[i].getIsValid()) {

            count++;
        }
    }

    return count;
}

int Block::getFreePageCount() const {

    int count = 0;

    for (int i = 0; i < (int)pages.size(); i++) {
        if (pages[i].getIsFree()) count++;
    }

    return count;
}

int Block::getValidPageCount() const {

    int count = 0;

    for (int i = 0; i < (int)pages.size(); i++) {
        if (!pages[i].getIsFree() && pages[i].getIsValid()) count++;
    }

    return count;
}