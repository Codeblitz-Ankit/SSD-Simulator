#ifndef GARBAGECOLLECTOR_H
#define GARBAGECOLLECTOR_H

#include "FlashMemory.h"
#include "FTLMapper.h"
#include "StatsTracker.h"
#include "LatencyModel.h"

// ─────────────────────────────────────────────────────────────────────────────
// GarbageCollector — NAND space reclamation + wear leveling
//
// ── Regular GC (runOneCycle) ─────────────────────────────────────────────────
//   1. Greedy victim selection: block with the most invalid pages
//   2. Migrate every valid page out → free slot elsewhere (FTL updated)
//   3. Erase victim block
//   Charges NAND program + erase latency to StatsTracker.
//
// ── Static Wear Leveling (staticWearLevel) ───────────────────────────────────
//   Triggered automatically every `wearLevelInterval` successful GC cycles.
//   Goal: reduce erase-count variance across blocks by moving cold data from
//   low-erase (fresh) blocks to high-erase (worn) free blocks, freeing fresh
//   blocks for the heavy write rotation managed by dynamic wear leveling.
//
//   Algorithm:
//     src = block with valid data + LOWEST erase count  (cold data block)
//     dst = fully-free block with HIGHEST erase count   (worn free block)
//     If erase gap < WEAR_LEVEL_THRESHOLD → skip (not worth it)
//     Move all valid pages from src → dst, updating FTL mappings
//     Erase src block → now it's a fresh free block for write traffic
//
//   Why this works:
//     After the swap, dynamic WL (findLeastUsedFreeBlock) will now prefer the
//     freshly-erased src block (low erase count) for new writes instead of the
//     worn dst block (high erase count). The worn block now holds cold data
//     that rarely changes, so it accumulates few future erases.
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int WEAR_LEVEL_THRESHOLD = 3;  // min erase gap to trigger WL

class GarbageCollector {

private:
    FlashMemory&        flash;
    FTLMapper&          mapper;
    StatsTracker&       stats;
    const LatencyModel& latency;

    bool verbose;

    // Phase 3: static wear leveling state
    int gcCyclesRun;        // incremented after each successful GC cycle
    int wearLevelInterval;  // run static WL every N GC cycles (0 = disabled)

    // Greedy victim: block with most invalid pages.  Returns -1 if none found.
    int selectVictimBlock() const;

public:
    // wearLevelInterval: how many GC cycles between static WL passes (default 5)
    GarbageCollector(
        FlashMemory&        flash,
        FTLMapper&          mapper,
        StatsTracker&       stats,
        const LatencyModel& latency,
        int                 wearLevelInterval = 5
    );

    // Execute one regular GC cycle (greedy victim → migrate → erase).
    // If static WL is due (gcCyclesRun % wearLevelInterval == 0), it is run
    // automatically at the end of the GC cycle.
    // Returns true if a block was collected.
    bool runOneCycle();

    // Execute one static wear-leveling pass manually (also called automatically
    // inside runOneCycle at the configured interval).
    // Returns true if a cold-data swap was performed.
    bool staticWearLevel();

    // Returns true when free-page ratio falls below the watermark.
    bool shouldTrigger(double lowWatermarkPercent) const;

    // Configure static WL interval (0 = disable, default = 5)
    void setWearLevelInterval(int n) { wearLevelInterval = n; }
    int  getWearLevelInterval()const { return wearLevelInterval; }

    void setVerbose(bool v)  { verbose = v; }
    bool isVerbose()   const { return verbose; }
};

#endif
