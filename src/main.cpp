#include <cmath>
#include <climits>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "LatencyModel.h"
#include "SSDController.h"
#include "TraceParser.h"
#include "RequestQueue.h"

// ============================================================
//  SSD Simulator — Phase 3 / 4 / 5 Combined Demo
// ============================================================
//
//  Phase 3: Compares the same no-cache NAND workload under:
//    1. Dynamic-only wear leveling       (static WL disabled)
//    2. Dynamic + static wear leveling   (static WL every N GC cycles)
//
//  Phase 4: Demonstrates the DRAM write-buffer / LRU read cache:
//    3. Wear-leveling OFF, cache ENABLED (shows IOPS improvement)
//
//  Phase 5: Trace file parser + workload replay:
//    Loads traces/sample.trace, replays via RequestQueue, prints stats.
//
//  Geometry is intentionally tiny and endurance intentionally low so the
//  erase distribution and lifespan estimate are visible in one short run.

// ─── Simulation constants ─────────────────────────────────────────────────────
static const int    BLOCKS             = 8;
static const int    PAGES_PER_BLOCK    = 8;
static const int    TOTAL_PAGES        = BLOCKS * PAGES_PER_BLOCK;
static const int    PE_LIMIT           = 50;
static const double WATERMARK          = 0.25;
static const int    CACHE_DISABLED     = 0;
static const int    CACHE_ENABLED      = 16;   // Phase 4: 16-page DRAM buffer

static const int    COLD_LBAS          = 16;
static const int    HOT_LBAS           = 4;
static const int    HOT_ROUNDS         = 50;  // 4 LBAs x 50 = 200 hot writes
static const int    STATIC_WL_INTERVAL = 4;

// ─── Result types ─────────────────────────────────────────────────────────────

struct ScenarioResult {
    std::string label;
    int         wearLevelInterval;
    double      waf;
    int         logicalWrites;
    int         physicalWrites;
    int         gcCycles;
    int         staticWearLevelPasses;
    int         pagesMigrated;
    double      eraseVariance;
    double      eraseStdDev;
    int         minErase;
    int         maxErase;
    long long   remainingHostWrites;
    // Phase 2 metrics
    double      avgReadLatencyUs;
    double      avgWriteLatencyUs;
    double      iops;
    // Phase 4 cache metrics (0 when cache disabled)
    int         cacheHits;
    int         cacheMisses;
    int         writesCoalesced;
    double      cacheHitRatio;
};

// ─── Helpers ──────────────────────────────────────────────────────────────────

static std::string formatDouble(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

static std::string formatRemaining(long long value) {
    if (value == LLONG_MAX) return "N/A";
    return std::to_string(value);
}

// ─── Phase 3 workload helpers ─────────────────────────────────────────────────

static void runPhaseAColdSetup(SSDController& ssd) {
    for (int i = 0; i < COLD_LBAS; i++) {
        int lba = i * 10;  // LBA 0, 10, ..., 150
        ssd.write(lba, "cold_LBA_" + std::to_string(lba));
    }
}

static void runPhaseBHotOverwrites(SSDController& ssd) {
    const int hotSet[HOT_LBAS] = {200, 210, 220, 230};

    for (int round = 0; round < HOT_ROUNDS; round++) {
        for (int i = 0; i < HOT_LBAS; i++) {
            int lba = hotSet[i];
            ssd.write(lba,
                      "hot_round_" + std::to_string(round)
                      + "_LBA_" + std::to_string(lba));
        }
    }
}

static ScenarioResult collectResult(const std::string& label,
                                    int wearLevelInterval,
                                    const SSDController& ssd) {
    const StatsTracker& stats = ssd.getStats();
    const FlashMemory&  flash = ssd.getFlashMemory();
    const PageCache&    cache = ssd.getCache();

    ScenarioResult result;
    result.label                 = label;
    result.wearLevelInterval     = wearLevelInterval;
    result.waf                   = stats.getWAF();
    result.logicalWrites         = stats.getLogicalWrites();
    result.physicalWrites        = stats.getPhysicalWrites();
    result.gcCycles              = stats.getGCInvocations();
    result.staticWearLevelPasses = stats.getStaticWearLevelCount();
    result.pagesMigrated         = stats.getPagesMigrated();
    result.eraseVariance         = flash.getEraseCountVariance();
    result.eraseStdDev           = std::sqrt(result.eraseVariance);
    result.minErase              = flash.getMinEraseCount();
    result.maxErase              = flash.getMaxEraseCount();
    result.remainingHostWrites   = stats.estimateRemainingWrites(
        flash.getPELimit(),
        result.maxErase,
        flash.getTotalPageCount()
    );
    result.avgReadLatencyUs  = stats.getAvgReadLatencyUs();
    result.avgWriteLatencyUs = stats.getAvgWriteLatencyUs();
    result.iops              = stats.getIOPS();
    result.cacheHits         = cache.getCacheHits();
    result.cacheMisses       = cache.getCacheMisses();
    result.writesCoalesced   = cache.getWritesCoalesced();
    result.cacheHitRatio     = cache.getHitRatio();

    return result;
}

// ─── Scenario runner ─────────────────────────────────────────────────────────

static ScenarioResult runScenario(const std::string& label,
                                  int wearLevelInterval,
                                  int cacheSize) {
    std::cout << "============================================================\n";
    std::cout << "  Scenario: " << label << "\n";
    std::cout << "============================================================\n";
    std::cout << "  Static WL interval : "
              << (wearLevelInterval == 0
                  ? "disabled"
                  : "every " + std::to_string(wearLevelInterval) + " GC cycles")
              << "\n";
    std::cout << "  Cache size         : " << cacheSize
              << (cacheSize == 0 ? " pages (disabled)" : " pages (enabled)")
              << "\n";
    std::cout << "  Workload           : " << COLD_LBAS
              << " cold writes, then "
              << (HOT_LBAS * HOT_ROUNDS) << " hot overwrites\n\n";

    SSDController ssd(BLOCKS,
                      PAGES_PER_BLOCK,
                      WATERMARK,
                      LatencyModel::SLC(),
                      cacheSize,
                      PE_LIMIT);
    ssd.setVerbose(false);
    ssd.setWearLevelInterval(wearLevelInterval);

    runPhaseAColdSetup(ssd);
    runPhaseBHotOverwrites(ssd);
    ssd.flushCache();

    std::cout << "--- Final Block Status ---\n";
    ssd.displayStatus();

    std::cout << "--- Wear Analysis ---\n";
    ssd.displayWearAnalysis();

    return collectResult(label, wearLevelInterval, ssd);
}

// ─── Summary table ────────────────────────────────────────────────────────────

static void printSummaryTable(const std::vector<ScenarioResult>& results) {
    const int L = 35;
    const int C = 26;

    auto hline = [&]() {
        std::cout << "  +" << std::string(L, '-');
        for (size_t i = 0; i < results.size(); i++) {
            std::cout << "+" << std::string(C, '-');
        }
        std::cout << "+\n";
    };

    auto row = [&](const std::string& label,
                   const std::vector<std::string>& values) {
        std::cout << "  | " << std::left << std::setw(L - 1) << label;
        for (const auto& value : values) {
            std::cout << "| " << std::right << std::setw(C - 2) << value << " ";
        }
        std::cout << "|\n";
    };

    std::cout << "============================================================\n";
    std::cout << "  Wear-Leveling & Cache Summary\n";
    std::cout << "============================================================\n";

    hline();
    {
        std::vector<std::string> values;
        for (const auto& r : results) values.push_back(r.label);
        row("Metric", values);
    }
    hline();

    {
        std::vector<std::string> values;
        for (const auto& r : results) values.push_back(formatDouble(r.waf, 2) + "x");
        row("WAF", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(std::to_string(r.physicalWrites)
                             + " / " + std::to_string(r.logicalWrites));
        }
        row("Physical / Logical Writes", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) values.push_back(std::to_string(r.gcCycles));
        row("GC Cycles", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(std::to_string(r.staticWearLevelPasses));
        }
        row("Static WL Passes", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(formatDouble(r.eraseVariance, 2)
                             + " / " + formatDouble(r.eraseStdDev, 2));
        }
        row("Erase Variance / Std Dev", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(std::to_string(r.minErase) + " / "
                             + std::to_string(r.maxErase)
                             + " (spread "
                             + std::to_string(r.maxErase - r.minErase) + ")");
        }
        row("Min / Max Erase Count", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(formatRemaining(r.remainingHostWrites));
        }
        row("Est. Remaining Host Writes", values);
    }

    hline();

    // ── Phase 2 / 4 metrics ───────────────────────────────────────────────────
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(formatDouble(r.avgWriteLatencyUs, 0) + " us");
        }
        row("Avg Write Latency (w/ GC)", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            values.push_back(formatDouble(r.iops, 0));
        }
        row("Estimated IOPS", values);
    }
    {
        std::vector<std::string> values;
        for (const auto& r : results) {
            if (r.cacheHits + r.cacheMisses == 0) {
                values.push_back("disabled");
            } else {
                values.push_back(formatDouble(r.cacheHitRatio, 1) + "% ("
                                 + std::to_string(r.writesCoalesced)
                                 + " coalesced)");
            }
        }
        row("Cache Hit Ratio (coalesced)", values);
    }

    hline();
    std::cout << "\n";
}

// ─── Phase 3/4 takeaways ─────────────────────────────────────────────────────

static void printTakeaways(const std::vector<ScenarioResult>& results) {
    if (results.size() < 3) return;

    const ScenarioResult& dynamicOnly = results[0];
    const ScenarioResult& staticWL    = results[1];
    const ScenarioResult& cached      = results[2];

    std::cout << "Key Takeaways\n";
    std::cout << "-------------\n";

    // Phase 3: wear leveling
    std::cout << "  [Phase 3] Static wear leveling ran "
              << staticWL.staticWearLevelPasses
              << " pass(es) vs " << dynamicOnly.staticWearLevelPasses
              << " in dynamic-only mode.\n";
    std::cout << "  Erase variance reduced from "
              << formatDouble(dynamicOnly.eraseVariance, 2)
              << " → " << formatDouble(staticWL.eraseVariance, 2)
              << " (spread " << (dynamicOnly.maxErase - dynamicOnly.minErase)
              << " → " << (staticWL.maxErase - staticWL.minErase) << " cycles).\n";

    // Phase 4: cache
    std::cout << "\n  [Phase 4] DRAM cache (" << CACHE_ENABLED << " pages) enabled:\n";
    std::cout << "  Cache hit ratio   : "
              << formatDouble(cached.cacheHitRatio, 1) << "%\n";
    std::cout << "  Writes coalesced  : " << cached.writesCoalesced
              << "  (NAND programs avoided by write buffering)\n";
    std::cout << "  IOPS (no cache)   : "
              << formatDouble(dynamicOnly.iops, 0) << "\n";
    std::cout << "  IOPS (w/ cache)   : "
              << formatDouble(cached.iops, 0)
              << "  (" << formatDouble(
                    cached.iops > 0 && dynamicOnly.iops > 0
                        ? (cached.iops / dynamicOnly.iops)
                        : 1.0, 2)
              << "x faster)\n\n";
}

// ─── Phase 5: Trace replay ───────────────────────────────────────────────────

static void runTraceScenario(const std::string& tracePath) {

    std::cout << "============================================================\n";
    std::cout << "  Phase 5: Trace File Replay\n";
    std::cout << "============================================================\n";

    SSDController ssd(BLOCKS,
                      PAGES_PER_BLOCK,
                      WATERMARK,
                      LatencyModel::SLC(),
                      CACHE_ENABLED,    // enable cache for trace replay
                      PE_LIMIT);
    ssd.setVerbose(false);
    ssd.setWearLevelInterval(STATIC_WL_INTERVAL);

    // Try the given path first, then a fallback for running from build/
    std::vector<std::string> candidates = {
        tracePath,
        "../" + tracePath,
        "../../" + tracePath
    };

    RequestQueue queue;
    bool loaded = false;
    std::string usedPath;

    for (const auto& candidate : candidates) {
        TraceParser parser(candidate);
        if (parser.loadInto(queue, ssd)) {
            std::cout << "  Trace: " << candidate << "\n";
            std::cout << "  Parsed  : " << parser.getParsedWrites() << " writes, "
                      << parser.getParsedReads()  << " reads, "
                      << parser.getSkippedLines() << " comments/blanks skipped\n";
            std::cout << "  Replaying " << parser.getTotalOps() << " operations...\n\n";
            loaded   = true;
            usedPath = candidate;
            break;
        }
    }

    if (!loaded) {
        std::cout << "  ERROR: Could not open trace file '" << tracePath
                  << "' — skipping phase.\n\n";
        return;
    }

    // Drain the request queue
    while (!queue.isEmpty()) {
        auto req = queue.getNextRequest();
        if (req) req->execute();
    }

    ssd.flushCache();

    std::cout << "--- Final Block Status ---\n";
    ssd.displayStatus();

    ssd.printStats();

    std::cout << "\n  [Trace Replay Complete]\n\n";
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {

    // ── Phase 3 + 4 header ────────────────────────────────────────────────────
    std::cout << "============================================================\n";
    std::cout << "  SSD Simulator — Phases 3/4/5 Demo\n";
    std::cout << "============================================================\n";
    std::cout << "  Geometry       : " << BLOCKS << " blocks x "
              << PAGES_PER_BLOCK << " pages = " << TOTAL_PAGES
              << " pages total\n";
    std::cout << "  P/E limit      : " << PE_LIMIT << " cycles per block\n";
    std::cout << "  GC watermark   : " << (WATERMARK * 100.0) << "% free pages\n";
    std::cout << "  Phase A        : write LBAs 0..150 by 10 once, then leave cold\n";
    std::cout << "  Phase B        : overwrite LBAs 200, 210, 220, 230 "
              << HOT_ROUNDS << " rounds (" << (HOT_LBAS * HOT_ROUNDS)
              << " writes total)\n";
    std::cout << "============================================================\n\n";

    // ── Phase 3 scenarios ─────────────────────────────────────────────────────
    std::vector<ScenarioResult> results;
    results.push_back(runScenario("Dynamic Only",         0,                  CACHE_DISABLED));
    results.push_back(runScenario("Dynamic + Static WL",  STATIC_WL_INTERVAL, CACHE_DISABLED));
    results.push_back(runScenario("Dynamic + Cache",      0,                  CACHE_ENABLED));

    printSummaryTable(results);
    printTakeaways(results);

    // ── Phase 5: trace replay ─────────────────────────────────────────────────
    // Run from project root: ./build/ssd_simulator
    // or from build dir: ./ssd_simulator (trace path relative to project root)
    // Try project-root-relative path first, then build-relative fallback.
    runTraceScenario("../traces/sample.trace");

    return 0;
}
