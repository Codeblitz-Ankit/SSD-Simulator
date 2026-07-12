#ifndef GARBAGECOLLECTOR_H
#define GARBAGECOLLECTOR_H

#include "FlashMemory.h"
#include "FTLMapper.h"
#include "StatsTracker.h"
#include "LatencyModel.h"

// ─────────────────────────────────────────────────────────────────────────────
// GarbageCollector
//
// Reclaims NAND space by:
//   1. Selecting a victim block (greedy: most invalid pages wins)
//   2. Migrating every valid page out of victim → free slot elsewhere
//      — updates FTL mapping + charges NAND program latency per page
//   3. Erasing the victim block
//      — charges NAND block-erase latency
//
// Phase 2: now charges LatencyModel timings to StatsTracker for every
//   NAND operation it performs (erase + migration writes).
// ─────────────────────────────────────────────────────────────────────────────

class GarbageCollector {

private:
    FlashMemory&        flash;
    FTLMapper&          mapper;
    StatsTracker&       stats;
    const LatencyModel& latency;    // Phase 2: reference to controller's model

    bool verbose;   // if false, suppress [GC] log lines (useful for batch runs)

    // Greedy victim selection: block with the most invalid pages.
    // Returns block index, or -1 if no reclaimable block found.
    int selectVictimBlock() const;

public:
    GarbageCollector(
        FlashMemory&        flash,
        FTLMapper&          mapper,
        StatsTracker&       stats,
        const LatencyModel& latency   // Phase 2
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

    // Toggle verbose [GC] log output (default: true)
    void setVerbose(bool v) { verbose = v; }
    bool isVerbose()  const { return verbose; }
};

#endif
