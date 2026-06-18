#ifndef BLOCK_H
#define BLOCK_H

#include <vector>
#include "Page.h"

class Block {
private:
    std::vector<Page> pages;
    int eraseCount;

public:
    Block(int numPages);

    Page& getPage(int index);

    int getFreePageIndex() const;

    int getEraseCount() const;

    void eraseBlock();

    int getTotalPages() const;

    int getInvalidPageCount() const;
};

#endif
