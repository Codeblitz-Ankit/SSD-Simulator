#ifndef SSDCONTROLLER_H
#define SSDCONTROLLER_H

#include <string>
#include "FlashMemory.h"
#include "FTLMapper.h"
#include "StatsTracker.h"
#include "LatencyModel.h"
#include "PageCache.h"
#include "GarbageCollector.h"

// ─────────────────────────────────────────────────────────────────────────────
// SSDController — Top-level SSD firmware simulation
//
// Subsystem ownership and wire-up:
//   FlashMemory      — physical NAND array         (Phase 1)
//   FTLMapper        — LBA ↔ PBA address mapping   (Phase 1)
//   StatsTracker     — WAF / IOPS / latency metrics (Phase 1 + 2)
//   LatencyModel     — NAND + DRAM timing constants (Phase 2)
//   PageCache        — DRAM write buffer / LRU cache(Phase 4)
//   GarbageCollector — greedy GC + static WL       (Phase 1 + 3)
//
// ── Declaration order = initialisation order ────────────────────────────────
// GarbageCollector holds const-refs to flashMemory, mapper, stats, and latency,
// so all four must be declared (and constructed) before gc.
// PageCache is independent but placed before gc for grouping clarity.
// ─────────────────────────────────────────────────────────────────────────────

class SSDController {

private:
    FlashMemory      flashMemory;
    FTLMapper        mapper;
    StatsTracker     stats;
    LatencyModel     latency;
    PageCache        cache;
    GarbageCollector gc;

    double lowWatermarkPercent;

    // Write `data` for `lba` directly to NAND (no cache interaction).
    // Handles GC trigger, free-slot selection, page program, FTL update,
    // invalidation of old page, and physical-write + latency accounting.
    // Does NOT record a logical write.
    void nandWrite(int lba, const std::string& data);

public:
    // numBlocks     — total flash blocks
    // pagesPerBlock — pages per block
    // lowWatermark  — free-page fraction below which GC fires (default 20%)
    // model         — NAND + DRAM timing constants (default: SLC)
    // cacheSize     — DRAM cache capacity in pages (0 = disabled, default 8)
    // peLimit       — P/E cycle endurance per block (default 3000 = MLC NAND)
    //                 Use a low value (e.g., 50) in demos to make wear visible.
    SSDController(
        int          numBlocks,
        int          pagesPerBlock,
        double       lowWatermark = 0.20,
        LatencyModel model        = LatencyModel(),
        int          cacheSize    = 8,
        int          peLimit      = 3000
    );

    // ── I/O ───────────────────────────────────────────────────────────────
    void        write(int logicalAddress, const std::string& data);
    std::string read (int logicalAddress);

    // Drain all dirty cache entries to NAND (end-of-simulation flush)
    void flushCache();

    // Manually run one regular GC cycle (static WL may follow automatically)
    void garbageCollect();

    // ── Configuration ─────────────────────────────────────────────────────
    void setVerbose(bool v);

    // Phase 3: configure static wear leveling interval
    // 0 = disable, default = 5 (every 5 GC cycles)
    void setWearLevelInterval(int n);

    // ── Display ───────────────────────────────────────────────────────────
    void displayStatus()     ;   // per-block free/valid/invalid/erases
    void displayWearAnalysis() const;  // Phase 3: erase bar chart + lifespan
    void printStats()        const;    // full stats report

    // ── Raw accessors (for comparison runs) ──────────────────────────────
    const StatsTracker& getStats()       const { return stats; }
    const PageCache&    getCache()       const { return cache; }
    const FlashMemory&  getFlashMemory() const { return flashMemory; }
};

#endif