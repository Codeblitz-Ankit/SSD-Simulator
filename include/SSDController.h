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
// Owns all subsystems and wires them together:
//
//   FlashMemory      — physical NAND storage
//   FTLMapper        — logical ↔ physical address translation
//   StatsTracker     — WAF, IOPS, latency metrics    (Phase 1 + 2)
//   LatencyModel     — per-operation NAND/DRAM timing (Phase 2)
//   PageCache        — DRAM write buffer / read cache (Phase 4)
//   GarbageCollector — watermark-triggered, greedy victim selection
//
// ── Declaration order matters ───────────────────────────────────────────────
// GarbageCollector holds const-refs to flashMemory, mapper, stats, and latency.
// All four must be declared (and therefore constructed) before gc.
// PageCache is independent but is placed before gc for grouping clarity.
// ─────────────────────────────────────────────────────────────────────────────

class SSDController {

private:
    // ── Member order = initialisation order ──────────────────────────────
    FlashMemory      flashMemory;
    FTLMapper        mapper;
    StatsTracker     stats;
    LatencyModel     latency;    // Phase 2: must be before gc (gc holds ref)
    PageCache        cache;      // Phase 4: DRAM cache layer
    GarbageCollector gc;         // holds const-refs to the four above

    double lowWatermarkPercent;

    // ── Private helpers ───────────────────────────────────────────────────

    // Write `data` for `lba` directly to NAND (no cache interaction).
    // Handles: GC-if-needed, free-slot lookup, page program, FTL update,
    // old-page invalidation, physical-write + latency accounting.
    // Does NOT record a logical write (caller does that).
    void nandWrite(int lba, const std::string& data);

public:
    // numBlocks     — total flash blocks
    // pagesPerBlock — pages per block
    // lowWatermark  — free-page fraction below which GC fires (default 20%)
    // model         — NAND + DRAM timing constants (default: SLC)
    // cacheSize     — DRAM cache capacity in pages (0 = disabled, default 8)
    SSDController(
        int          numBlocks,
        int          pagesPerBlock,
        double       lowWatermark = 0.20,
        LatencyModel model        = LatencyModel(),
        int          cacheSize    = 8
    );

    // Out-of-place write.
    //   Cache enabled : writes to cache; NAND write deferred until eviction.
    //   Cache disabled: writes directly to NAND (same as Phase 2 behaviour).
    // Triggers GC automatically before any NAND write if free space is low.
    void write(int logicalAddress, const std::string& data);

    // Read.
    //   Cache hit  → returns data at DRAM latency (no NAND access).
    //   Cache miss → reads from NAND, inserts clean entry into cache.
    std::string read(int logicalAddress);

    // Force-write all dirty cache entries to NAND (end-of-simulation drain).
    void flushCache();

    // Manually run one GC cycle (also triggered automatically on write)
    void garbageCollect();

    // Toggle [GC] verbose log output. Default: true.
    void setVerbose(bool v);

    // Print current free/valid/invalid/erase-count per block
    void displayStatus();

    // Print full stats report (WAF + latency + IOPS) and cache stats if enabled
    void printStats() const;

    // Raw accessors for batch/comparison runs
    const StatsTracker& getStats() const { return stats; }
    const PageCache&    getCache() const { return cache; }
};

#endif