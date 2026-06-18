#ifndef SSDCONTROLLER_H
#define SSDCONTROLLER_H

#include <string>

#include "FlashMemory.h"
#include "FTLMapper.h"

class SSDController {
private:
    FlashMemory flashMemory;
    FTLMapper mapper;

public:
    SSDController(
        int numBlocks,
        int pagesPerBlock
    );

    void write(
        int logicalAddress,
        const std::string& data
    );

    std::string read(
        int logicalAddress
    );

    void displayStatus();

    void garbageCollect();
};

#endif