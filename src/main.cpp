// ============================================================
//  SSD Simulator — Phase 4: DRAM LRU Page Cache
// ============================================================
//
//  What Phase 4 adds
//  -----------------
//  A fixed-size DRAM page cache sits in front of NAND, implementing two
//  key SSD firmware techniques:
//
//  1. Write Coalescing:
//     If the same LBA is overwritten multiple times before the cache
//     evicts it, only ONE physical NAND write occurs instead of N.
//     This directly reduces WAF.
//     Example: LBA 5 written 50× while in cache → 1 NAND write.
//
//  2. Read Caching:
//     Subsequent reads to a cached LBA are served from DRAM (0.1 µs)
//     instead of NAND (50 µs). 500× latency improvement per cache hit.
//
//  LRU eviction: least-recently-used page is evicted when cache is full.
//    - Dirty eviction → flush to NAND (still counts as a physical write)
//    - Clean eviction → silently discard (no NAND write)
//
//  Demo
//  ----
//  Runs an IDENTICAL workload three times:
//    1. No cache         (cacheSize = 0)  — Phase 2 baseline
//    2. Cache 8 pages    (cacheSize = 8)  — 12.5% of SSD capacity
//    3. Cache 16 pages   (cacheSize = 16) — 25% of SSD capacity
//
//  Then prints a side-by-side comparison table showing WAF, latency,
//  IOPS, and cache efficiency metrics.
//
//  Workload
//  --------
//  SSD geometry : 8 blocks × 8 pages = 64 pages total
//  GC watermark : 25% free pages
//
//  Phase A : Write 40 unique LBAs once each
//  Phase B : Overwrite 6 "hot" LBAs × 50 times each
//            (300 writes to a 6-LBA hot set — strong coalescing candidate)
//  Phase C : Read all 40 LBAs once (measures cache read hit benefit)
//  Flush   : Drain all dirty cache entries to NAND (simulates power-down)

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "SSDController.h"
#include "LatencyModel.h"

// ─── Workload constants ───────────────────────────────────────────────────────

static const int    BLOCKS      = 8;
static const int    PAGES       = 8;    // 64 pages total
static const double WATERMARK   = 0.25;

static const int    TOTAL_LBA   = 40;
static const int    HOT_LBA     = 6;    // first 6 LBAs are hot
static const int    HOT_WRITES  = 50;   // rewrites per hot LBA

// ─── Result ───────────────────────────────────────────────────────────────────

struct Result {
    std::string label;
    int         cacheSize;
    double      waf;
    int         logicalWrites;
    int         physicalWrites;
    double      avgReadLatUs;
    double      avgWriteLatUs;
    double      totalSimMs;
    double      iops;
    int         gcCycles;
    int         pagesMigrated;
    int         cacheHits;
    int         cacheMisses;
    int         writesCoalesced;
    int         dirtyEvictions;
    double      hitRatio;
};

// ─── runWorkload ──────────────────────────────────────────────────────────────

static Result runWorkload(const std::string& label, int cacheSize) {

    std::cout << "  [" << label << "] Running... ";
    std::cout.flush();

    SSDController ssd(BLOCKS, PAGES, WATERMARK,
                      LatencyModel::SLC(), cacheSize);
    ssd.setVerbose(false);

    // ── Phase A: fill SSD with 40 unique LBAs ────────────────────────────
    for (int i = 0; i < TOTAL_LBA; i++) {
        ssd.write(i * 10, "cold_LBA" + std::to_string(i * 10));
    }

    // ── Phase B: hammer 6 hot LBAs ───────────────────────────────────────
    for (int w = 0; w < HOT_WRITES; w++) {
        for (int i = 0; i < HOT_LBA; i++) {
            ssd.write(i * 10,
                      "hot_v" + std::to_string(w)
                      + "_LBA" + std::to_string(i * 10));
        }
    }

    // ── Phase C: read all LBAs once ──────────────────────────────────────
    for (int i = 0; i < TOTAL_LBA; i++) {
        ssd.read(i * 10);
    }

    // ── Flush: drain dirty cache entries to NAND ──────────────────────────
    ssd.flushCache();

    std::cout << "done.\n";

    // ── Collect results ───────────────────────────────────────────────────
    const StatsTracker& st = ssd.getStats();
    const PageCache&    pc = ssd.getCache();

    Result r;
    r.label           = label;
    r.cacheSize       = cacheSize;
    r.waf             = st.getWAF();
    r.logicalWrites   = st.getLogicalWrites();
    r.physicalWrites  = st.getPhysicalWrites();
    r.avgReadLatUs    = st.getAvgReadLatencyUs();
    r.avgWriteLatUs   = st.getAvgWriteLatencyUs();
    r.totalSimMs      = st.getTotalSimulatedTimeUs() / 1000.0;
    r.iops            = st.getIOPS();
    r.gcCycles        = st.getGCInvocations();
    r.pagesMigrated   = st.getPagesMigrated();
    r.cacheHits       = pc.getCacheHits();
    r.cacheMisses     = pc.getCacheMisses();
    r.writesCoalesced = pc.getWritesCoalesced();
    r.dirtyEvictions  = pc.getDirtyEvictions();
    r.hitRatio        = pc.getHitRatio();

    return r;
}

// ─── printTable ──────────────────────────────────────────────────────────────

static void printTable(const std::vector<Result>& results) {

    const int L = 27;    // label column width
    const int C = 16;    // data column width

    auto hline = [&]() {
        std::cout << "  +" << std::string(L, '-');
        for (size_t i = 0; i < results.size(); i++)
            std::cout << "+" << std::string(C, '-');
        std::cout << "+\n";
    };

    auto row = [&](const std::string& label,
                   const std::vector<std::string>& vals) {
        std::cout << "  | " << std::left << std::setw(L - 1) << label;
        for (const auto& v : vals)
            std::cout << "| " << std::right << std::setw(C - 2) << v << " ";
        std::cout << "|\n";
    };

    // ── Header ───────────────────────────────────────────────────────────
    std::cout << "\n";
    hline();
    {
        std::vector<std::string> hdrs;
        for (const auto& r : results) hdrs.push_back(r.label);
        row("Metric", hdrs);
    }
    hline();

    auto fmt = [](double v, int prec, const std::string& suffix = "") {
        std::ostringstream o;
        o << std::fixed << std::setprecision(prec) << v << suffix;
        return o.str();
    };

    // ── WAF ───────────────────────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(fmt(r.waf, 2, "x"));
        row("WAF", v);
    }

    // ── Physical writes ───────────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results)
            v.push_back(std::to_string(r.physicalWrites)
                        + " / " + std::to_string(r.logicalWrites));
        row("Phys / Logic Writes", v);
    }

    // ── Avg Read Latency ─────────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(fmt(r.avgReadLatUs,  1, " µs"));
        row("Avg Read Latency", v);
    }

    // ── Avg Write Latency ─────────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(fmt(r.avgWriteLatUs, 1, " µs"));
        row("Avg Write Lat (w/GC)", v);
    }

    // ── Total Simulated Time ──────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(fmt(r.totalSimMs, 2, " ms"));
        row("Total Simulated Time", v);
    }

    // ── IOPS ──────────────────────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(fmt(r.iops, 0));
        row("Estimated IOPS", v);
    }

    hline();

    // ── GC / Wear ─────────────────────────────────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(std::to_string(r.gcCycles));
        row("GC Cycles", v);
    }
    {
        std::vector<std::string> v;
        for (const auto& r : results) v.push_back(std::to_string(r.pagesMigrated));
        row("Pages Migrated", v);
    }

    hline();

    // ── Cache stats (only if cache was enabled) ────────────────────────────
    {
        std::vector<std::string> v;
        for (const auto& r : results)
            v.push_back(r.cacheSize > 0 ? fmt(r.hitRatio, 1, "%") : "N/A");
        row("Cache Hit Ratio", v);
    }
    {
        std::vector<std::string> v;
        for (const auto& r : results)
            v.push_back(r.cacheSize > 0
                        ? std::to_string(r.writesCoalesced)
                        : "N/A");
        row("Writes Coalesced", v);
    }
    {
        std::vector<std::string> v;
        for (const auto& r : results)
            v.push_back(r.cacheSize > 0
                        ? std::to_string(r.dirtyEvictions)
                        : "N/A");
        row("Dirty Evictions", v);
    }

    hline();
    std::cout << "\n";
}

// ─── printInsights ────────────────────────────────────────────────────────────

static void printInsights(const std::vector<Result>& results) {

    if (results.size() < 2) return;

    const Result& base  = results[0];  // no-cache baseline
    const Result& cache8 = results[1]; // cache-8

    std::cout << "  Key Takeaways (No-Cache vs Cache-8)\n";
    std::cout << "  ─────────────────────────────────────────────────────\n";

    // WAF reduction
    double wafReduction = (base.waf - cache8.waf) / base.waf * 100.0;
    std::cout << "  • WAF reduced " << std::fixed << std::setprecision(1)
              << wafReduction << "% by write coalescing: "
              << std::setprecision(2) << base.waf << "x → "
              << cache8.waf << "x\n"
              << "    " << cache8.writesCoalesced
              << " writes absorbed by cache before eviction.\n\n";

    // Physical write reduction
    int writeReduction = base.physicalWrites - cache8.physicalWrites;
    std::cout << "  • Physical NAND writes cut from "
              << base.physicalWrites << " → " << cache8.physicalWrites
              << " (" << writeReduction << " writes saved).\n\n";

    // Read latency
    double readSpeedup = base.avgReadLatUs / cache8.avgReadLatUs;
    std::cout << "  • Avg read latency " << std::setprecision(1)
              << readSpeedup << "x faster with cache: "
              << base.avgReadLatUs << " µs → " << cache8.avgReadLatUs << " µs\n"
              << "    (cache hit ratio " << cache8.hitRatio << "% — mostly DRAM "
              << "0.1 µs vs NAND 50 µs)\n\n";

    // IOPS improvement
    double iopsImprovement = cache8.iops / base.iops;
    std::cout << "  • IOPS " << std::setprecision(1) << iopsImprovement
              << "x higher with cache: "
              << std::setprecision(0) << base.iops << " → "
              << cache8.iops << "\n\n";

    // GC reduction
    std::cout << "  • GC cycles: " << base.gcCycles << " (no cache) → "
              << cache8.gcCycles << " (cache-8)\n"
              << "    Fewer NAND writes = less space pressure = fewer GC cycles.\n";
    std::cout << "  ─────────────────────────────────────────────────────\n\n";
}

// ─── verboseRun ──────────────────────────────────────────────────────────────
// Runs one workload with GC verbose ON and prints per-LBA read verification
// + full stats report.  Used to show the detailed SLC + cache-8 picture.

static void verboseRun(int cacheSize) {

    std::cout << "============================================================\n";
    std::cout << "  Detailed Run — SLC + Cache-" << cacheSize << " (verbose GC)\n";
    std::cout << "============================================================\n\n";

    SSDController ssd(BLOCKS, PAGES, WATERMARK,
                      LatencyModel::SLC(), cacheSize);
    ssd.setVerbose(true);

    std::cout << "--- Phase A: Initial writes (GC output suppressed) ---\n";
    ssd.setVerbose(false);
    for (int i = 0; i < TOTAL_LBA; i++)
        ssd.write(i * 10, "cold_LBA" + std::to_string(i * 10));
    ssd.setVerbose(true);

    std::cout << "\n--- Phase B: Hot LBA rewrites ---\n";
    for (int w = 0; w < HOT_WRITES; w++)
        for (int i = 0; i < HOT_LBA; i++)
            ssd.write(i * 10,
                      "hot_v" + std::to_string(w)
                      + "_LBA" + std::to_string(i * 10));

    ssd.flushCache();

    std::cout << "\n--- Phase C: Final reads ---\n";
    for (int i = 0; i < TOTAL_LBA; i++) {
        std::string val = ssd.read(i * 10);
        std::cout << "  LBA " << std::setw(3) << (i * 10)
                  << " -> \"" << val << "\"\n";
    }

    ssd.displayStatus();
    ssd.printStats();
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {

    std::cout << "============================================================\n";
    std::cout << "  SSD Simulator — Phase 4: DRAM LRU Page Cache\n";
    std::cout << "============================================================\n";
    std::cout << "  NAND Grade : SLC  (50 µs read / 500 µs write / 2 ms erase)\n";
    std::cout << "  DRAM Lat.  : 0.1 µs  (cache hit)\n";
    std::cout << "  Geometry   : " << BLOCKS << " blocks x " << PAGES
              << " pages = " << (BLOCKS * PAGES) << " pages total\n";
    std::cout << "  Watermark  : " << (WATERMARK * 100.0) << "% free\n";
    std::cout << "  Phase A    : " << TOTAL_LBA << " unique LBA writes\n";
    std::cout << "  Phase B    : " << HOT_LBA << " hot LBAs x "
              << HOT_WRITES << " rewrites\n";
    std::cout << "  Phase C    : " << TOTAL_LBA << " sequential reads\n";
    std::cout << "  Flush      : drain dirty cache entries to NAND\n";
    std::cout << "============================================================\n\n";

    // ── Comparison runs ───────────────────────────────────────────────────
    std::cout << "Running workload comparison:\n";

    std::vector<Result> results;
    results.push_back(runWorkload("No Cache",  0));
    results.push_back(runWorkload("Cache-8",   8));
    results.push_back(runWorkload("Cache-16", 16));

    // ── Comparison table ──────────────────────────────────────────────────
    std::cout << "\n============================================================\n";
    std::cout << "  Cache Size Comparison\n";
    std::cout << "============================================================\n";

    printTable(results);
    printInsights(results);

    // ── Detailed verbose run: SLC + cache-8 ──────────────────────────────
    verboseRun(8);

    return 0;
}