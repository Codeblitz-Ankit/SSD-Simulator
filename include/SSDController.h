#ifndef SSDCONTROLLER_H
#define SSDCONTROLLER_H

#include <string>
#include "FlashMemory.h"
#include "FTLMapper.h"
#include "StatsTracker.h"
#include "GarbageCollector.h"

class SSDController {

private:
    FlashMemory      flashMemory;
    FTLMapper        mapper;
    StatsTracker     stats;
    GarbageCollector gc;

    // GC fires when free-page ratio drops below this fraction (0.0 – 1.0)
    double lowWatermarkPercent;

public:
    // numBlocks        : total number of flash blocks
    // pagesPerBlock    : pages per block
    // lowWatermark     : free-page fraction below which GC triggers (default 20%)
    SSDController(
        int    numBlocks,
        int    pagesPerBlock,
        double lowWatermark = 0.20
    );

    // Out-of-place write. Triggers GC automatically if free space is low.
    void write(int logicalAddress, const std::string& data);

    std::string read(int logicalAddress);

    // Manually run one GC cycle (also available internally)
    void garbageCollect();

    void displayStatus();

    void printStats() const;
};

#endif