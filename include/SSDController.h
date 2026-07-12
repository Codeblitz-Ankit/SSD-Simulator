#ifndef SSDCONTROLLER_H
#define SSDCONTROLLER_H

#include <string>
#include "FlashMemory.h"
#include "FTLMapper.h"
#include "StatsTracker.h"
#include "LatencyModel.h"
#include "GarbageCollector.h"

// ─────────────────────────────────────────────────────────────────────────────
// SSDController — Top-level SSD firmware simulation
//
// Owns all subsystems and wires them together:
//   FlashMemory   — physical NAND storage
//   FTLMapper     — logical ↔ physical address translation
//   StatsTracker  — WAF, IOPS, latency metrics (Phase 1 + 2)
//   LatencyModel  — per-operation NAND timing constants  (Phase 2)
//   GarbageCollector — watermark-triggered, greedy victim selection
//
// Member declaration order matches initialisation order (required because
// GarbageCollector holds const-refs to flashMemory, mapper, stats, latency).
// ─────────────────────────────────────────────────────────────────────────────

class SSDController {

private:
    FlashMemory      flashMemory;
    FTLMapper        mapper;
    StatsTracker     stats;
    LatencyModel     latency;       // Phase 2: must be before gc (gc holds ref)
    GarbageCollector gc;

    double lowWatermarkPercent;     // free-page fraction below which GC fires

public:
    // numBlocks     — total flash blocks
    // pagesPerBlock — pages per block
    // lowWatermark  — free-page fraction below which GC fires (default 20%)
    // model         — NAND timing constants (default: SLC)
    SSDController(
        int          numBlocks,
        int          pagesPerBlock,
        double       lowWatermark = 0.20,
        LatencyModel model        = LatencyModel()
    );

    // Out-of-place write. Triggers GC automatically if free space is low.
    // Charges latency.pageWriteUs to StatsTracker.
    void write(int logicalAddress, const std::string& data);

    // Read — charges latency.pageReadUs to StatsTracker.
    std::string read(int logicalAddress);

    // Manually run one GC cycle (also triggered automatically on write)
    void garbageCollect();

    // Toggle [GC] verbose log output. Default: true.
    // Set false for clean output in batch / comparison runs.
    void setVerbose(bool v);

    // Print current free/valid/invalid/erase-count per block
    void displayStatus();

    // Print full stats report (WAF + latency + IOPS)
    void printStats() const;

    // Access raw stats for external result extraction
    const StatsTracker& getStats() const { return stats; }
};

#endif