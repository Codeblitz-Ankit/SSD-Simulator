#include <cmath>
#include <climits>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "LatencyModel.h"
#include "SSDController.h"

// ============================================================
//  SSD Simulator — Phase 3: Wear Leveling & Lifespan Demo
// ============================================================
//
//  Compares the same no-cache NAND workload under:
//    1. Dynamic-only wear leveling       (static WL disabled)
//    2. Dynamic + static wear leveling   (static WL every N GC cycles)
//
//  Geometry is intentionally tiny and endurance is intentionally low so the
//  erase distribution and lifespan estimate are visible in one short run.

static const int    BLOCKS             = 8;
static const int    PAGES_PER_BLOCK    = 8;
static const int    TOTAL_PAGES        = BLOCKS * PAGES_PER_BLOCK;
static const int    PE_LIMIT           = 50;
static const double WATERMARK          = 0.25;
static const int    CACHE_SIZE         = 0;

static const int    COLD_LBAS          = 16;
static const int    HOT_LBAS           = 4;
static const int    HOT_ROUNDS         = 50;  // 4 LBAs x 50 = 200 hot writes
static const int    STATIC_WL_INTERVAL = 4;

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
};

static std::string formatDouble(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

static std::string formatRemaining(long long value) {
    if (value == LLONG_MAX) return "N/A";
    return std::to_string(value);
}

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

    return result;
}

static ScenarioResult runScenario(const std::string& label,
                                  int wearLevelInterval) {
    std::cout << "============================================================\n";
    std::cout << "  Scenario: " << label << "\n";
    std::cout << "============================================================\n";
    std::cout << "  Static WL interval : "
              << (wearLevelInterval == 0
                  ? "disabled"
                  : "every " + std::to_string(wearLevelInterval) + " GC cycles")
              << "\n";
    std::cout << "  Cache size         : " << CACHE_SIZE
              << " pages (disabled)\n";
    std::cout << "  Workload           : " << COLD_LBAS
              << " cold writes, then "
              << (HOT_LBAS * HOT_ROUNDS) << " hot overwrites\n\n";

    SSDController ssd(BLOCKS,
                      PAGES_PER_BLOCK,
                      WATERMARK,
                      LatencyModel::SLC(),
                      CACHE_SIZE,
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
    std::cout << "  Wear-Leveling Summary\n";
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
    std::cout << "\n";
}

static void printTakeaways(const std::vector<ScenarioResult>& results) {
    if (results.size() < 2) return;

    const ScenarioResult& dynamicOnly = results[0];
    const ScenarioResult& staticWL    = results[1];

    std::cout << "Key Takeaways\n";
    std::cout << "-------------\n";
    std::cout << "  Static wear leveling ran "
              << staticWL.staticWearLevelPasses
              << " pass(es), compared with "
              << dynamicOnly.staticWearLevelPasses
              << " in dynamic-only mode.\n";
    std::cout << "  The static-WL run moves long-lived cold pages from low-erase "
              << "blocks onto high-erase free blocks, then frees the fresher "
              << "source blocks for hot writes.\n";
    std::cout << "  That prevents a small hot-write block set from racing ahead "
              << "toward the P/E limit: variance changed from "
              << formatDouble(dynamicOnly.eraseVariance, 2)
              << " to " << formatDouble(staticWL.eraseVariance, 2)
              << ", and spread changed from "
              << (dynamicOnly.maxErase - dynamicOnly.minErase)
              << " to " << (staticWL.maxErase - staticWL.minErase)
              << " erase cycles.\n";
    std::cout << "  Lifespan is conservative and tied to the most-worn block: "
              << formatRemaining(dynamicOnly.remainingHostWrites)
              << " remaining host writes versus "
              << formatRemaining(staticWL.remainingHostWrites)
              << " with static WL enabled. Because this short demo includes "
              << "the migration cost in WAF, the estimate may trade some "
              << "remaining-host-write count for a healthier erase distribution.\n\n";
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "  SSD Simulator — Phase 3 Wear Leveling Comparison\n";
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
    std::cout << "  Cache          : disabled in both scenarios\n";
    std::cout << "============================================================\n\n";

    std::vector<ScenarioResult> results;
    results.push_back(runScenario("Dynamic Only", 0));
    results.push_back(runScenario("Dynamic + Static WL", STATIC_WL_INTERVAL));

    printSummaryTable(results);
    printTakeaways(results);

    return 0;
}
