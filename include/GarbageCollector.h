#ifndef GARBAGECOLLECTOR_H
#define GARBAGECOLLECTOR_H

#include "FlashMemory.h"
#include "FTLMapper.h"
#include "StatsTracker.h"

class GarbageCollector {

private:
    FlashMemory&  flash;
    FTLMapper&    mapper;
    StatsTracker& stats;

    // Greedy victim selection: block with the most invalid pages.
    // Returns block index, or -1 if no reclaimable block exists.
    int selectVictimBlock() const;

public:
    GarbageCollector(
        FlashMemory&  flash,
        FTLMapper&    mapper,
        StatsTracker& stats
    );

    // Execute one GC cycle:
    //   1. Pick victim (most invalid pages)
    //   2. Migrate surviving valid pages to free slots elsewhere
    //   3. Erase victim block
    // Returns true if a block was successfully collected.
    bool runOneCycle();

    // Returns true when free-page ratio falls below lowWatermarkPercent.
    // (lowWatermarkPercent is in range 0.0 – 1.0, e.g. 0.20 = 20%)
    bool shouldTrigger(double lowWatermarkPercent) const;
};

#endif
