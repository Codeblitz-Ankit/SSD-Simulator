// ============================================================
//  SSD Simulator — Phase 2: Latency Model + IOPS
// ============================================================
//
//  What Phase 2 adds
//  -----------------
//  Every NAND operation is now time-charged using realistic latency
//  constants drawn from Micron MT29F flash datasheets:
//
//    Operation      SLC       MLC       TLC
//    ─────────────  ───────   ───────   ───────
//    Page read      50 µs     70 µs     100 µs
//    Page program   500 µs    1800 µs   3000 µs
//    Block erase    2000 µs   3500 µs   5000 µs
//
//  The demo runs the SAME workload against all three NAND grades and
//  prints a side-by-side comparison showing how grade choice affects
//  WAF, average latency, and IOPS.
//
//  Workload
//  --------
//  SSD geometry : 8 blocks × 8 pages = 64 pages total
//  GC watermark : 25% free pages (= 16 pages)
//
//  Phase A: Write 40 unique LBAs once  (fills 62.5% of flash)
//  Phase B: Overwrite 6 hot LBAs × 50 times each (triggers GC repeatedly)
//  Phase C: Read all 40 LBAs sequentially (adds read latency mass)
//
//  Why this workload
//  -----------------
//  Phase A creates a mix of cold (long-lived) data spread across blocks.
//  Phase B hammers a small hot set — each overwrite invalidates an old page.
//  When GC fires, it must migrate the cold pages it finds alongside the
//  hot invalids → WAF > 1.  Phase C adds a realistic read-dominant tail
//  to model a database "write-then-read" access pattern.

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "SSDController.h"
#include "LatencyModel.h"

// ─── Workload constants ───────────────────────────────────────────────────────

static const int BLOCKS        = 8;
static const int PAGES         = 8;   // 64 pages total
static const double WATERMARK  = 0.25;

static const int TOTAL_LBA     = 40;  // unique LBAs written in Phase A
static const int HOT_LBA       = 6;   // first 6 LBAs are "hot"
static const int HOT_WRITES    = 50;  // rewrites per hot LBA in Phase B

// ─── Result struct ────────────────────────────────────────────────────────────

struct WorkloadResult {
    std::string gradeName;
    double      waf;
    double      avgReadLatUs;
    double      avgWriteLatUs;   // amortized (host write + proportional GC)
    double      totalSimMs;
    double      iops;
    int         gcCycles;
    int         pagesMigrated;
};

// ─── runWorkload ─────────────────────────────────────────────────────────────
// Runs the full Phase A + B + C workload on a fresh SSDController configured
// with the given LatencyModel.  Returns a filled WorkloadResult.

static WorkloadResult runWorkload(
    const std::string& gradeName,
    LatencyModel        model
) {
    std::cout << "  Running " << gradeName << " workload... ";
    std::cout.flush();

    // Fresh SSD for each grade — GC output suppressed for clean comparison
    SSDController ssd(BLOCKS, PAGES, WATERMARK, model);
    ssd.setVerbose(false);

    // ── Phase A: write all TOTAL_LBA addresses once ───────────────────────
    for (int i = 0; i < TOTAL_LBA; i++) {
        ssd.write(i * 10, "cold_LBA" + std::to_string(i * 10));
    }

    // ── Phase B: hammer the HOT_LBA hot addresses ─────────────────────────
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

    std::cout << "done.\n";

    // ── Extract results ───────────────────────────────────────────────────
    const StatsTracker& st = ssd.getStats();

    WorkloadResult r;
    r.gradeName     = gradeName;
    r.waf           = st.getWAF();
    r.avgReadLatUs  = st.getAvgReadLatencyUs();
    r.avgWriteLatUs = st.getAvgWriteLatencyUs();
    r.totalSimMs    = st.getTotalSimulatedTimeUs() / 1000.0;
    r.iops          = st.getIOPS();
    r.gcCycles      = st.getGCInvocations();
    r.pagesMigrated = st.getPagesMigrated();

    return r;
}

// ─── printTable ──────────────────────────────────────────────────────────────

static void printTable(const std::vector<WorkloadResult>& results) {

    // Column widths
    const int W0 = 26;   // metric label
    const int W  = 16;   // data column

    auto hline = [&]() {
        std::cout << "  +" << std::string(W0, '-');
        for (size_t i = 0; i < results.size(); i++)
            std::cout << "+" << std::string(W, '-');
        std::cout << "+\n";
    };

    auto row = [&](const std::string& label,
                   const std::vector<std::string>& vals) {
        std::cout << "  | " << std::left << std::setw(W0 - 1) << label;
        for (const auto& v : vals)
            std::cout << "| " << std::right << std::setw(W - 2) << v << " ";
        std::cout << "|\n";
    };

    std::cout << "\n";
    hline();

    // Header
    std::vector<std::string> headers;
    for (const auto& r : results) headers.push_back(r.gradeName);
    row("Metric", headers);
    hline();

    // WAF
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << r.waf << "x";
            vals.push_back(oss.str());
        }
        row("WAF", vals);
    }

    // Avg Read Latency
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << r.avgReadLatUs << " µs";
            vals.push_back(oss.str());
        }
        row("Avg Read Latency", vals);
    }

    // Avg Write Latency (amortized)
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << r.avgWriteLatUs << " µs";
            vals.push_back(oss.str());
        }
        row("Avg Write Lat (w/GC)", vals);
    }

    // Total Simulated Time
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << r.totalSimMs << " ms";
            vals.push_back(oss.str());
        }
        row("Total Simulated Time", vals);
    }

    // IOPS
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << r.iops;
            vals.push_back(oss.str());
        }
        row("Estimated IOPS", vals);
    }

    hline();

    // GC Cycles
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            vals.push_back(std::to_string(r.gcCycles));
        }
        row("GC Cycles", vals);
    }

    // Pages Migrated
    {
        std::vector<std::string> vals;
        for (const auto& r : results) {
            vals.push_back(std::to_string(r.pagesMigrated));
        }
        row("Pages Migrated", vals);
    }

    hline();
    std::cout << "\n";
}

// ─── printInsights ───────────────────────────────────────────────────────────

static void printInsights(const std::vector<WorkloadResult>& results) {

    if (results.size() < 2) return;

    const WorkloadResult& slc = results[0];   // baseline
    const WorkloadResult& tlc = results[2];   // worst

    std::cout << "  Key Takeaways\n";
    std::cout << "  ─────────────────────────────────────────────────────\n";

    // WAF is grade-independent
    std::cout << "  • WAF is identical across all grades (" 
              << std::fixed << std::setprecision(2) << slc.waf
              << "x) — WAF is determined by\n"
              << "    workload access pattern + GC algorithm, NOT NAND timing.\n\n";

    // Write latency amplification
    double wlatRatio = tlc.avgWriteLatUs / slc.avgWriteLatUs;
    std::cout << "  • Write latency: TLC is "
              << std::fixed << std::setprecision(1) << wlatRatio
              << "x slower than SLC (per host write, amortized GC).\n"
              << "    SLC: " << std::setprecision(0) << slc.avgWriteLatUs
              << " µs  →  TLC: " << tlc.avgWriteLatUs << " µs\n\n";

    // IOPS ratio
    double iopsRatio = slc.iops / tlc.iops;
    std::cout << "  • IOPS: SLC delivers "
              << std::fixed << std::setprecision(1) << iopsRatio
              << "x more IOPS than TLC under the same workload.\n"
              << "    SLC: " << std::setprecision(0) << slc.iops
              << " IOPS  →  TLC: " << tlc.iops << " IOPS\n\n";

    // GC overhead contribution
    std::cout << "  • The GC write amplification factor (" 
              << std::fixed << std::setprecision(2) << slc.waf
              << "x) means every host write\n"
              << "    causes " << std::setprecision(2) << (slc.waf - 1.0)
              << " extra NAND programs on average (migration overhead).\n";
    std::cout << "  ─────────────────────────────────────────────────────\n\n";
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {

    std::cout << "============================================================\n";
    std::cout << "  SSD Simulator — Phase 2: Latency Model + IOPS\n";
    std::cout << "============================================================\n";
    std::cout << "  Geometry : " << BLOCKS << " blocks x " << PAGES
              << " pages = " << (BLOCKS * PAGES) << " pages total\n";
    std::cout << "  Watermark: " << (WATERMARK * 100) << "% free\n";
    std::cout << "  Phase A  : " << TOTAL_LBA << " unique LBA writes\n";
    std::cout << "  Phase B  : " << HOT_LBA << " hot LBAs x "
              << HOT_WRITES << " rewrites\n";
    std::cout << "  Phase C  : " << TOTAL_LBA << " sequential reads\n";
    std::cout << "============================================================\n\n";

    std::cout << "Running workload on all NAND grades:\n";

    std::vector<WorkloadResult> results;
    results.push_back(runWorkload("SLC", LatencyModel::SLC()));
    results.push_back(runWorkload("MLC", LatencyModel::MLC()));
    results.push_back(runWorkload("TLC", LatencyModel::TLC()));

    std::cout << "\n============================================================\n";
    std::cout << "  NAND Grade Comparison\n";
    std::cout << "============================================================\n";

    printTable(results);
    printInsights(results);

    // ── Detailed stats for SLC (verbose Phase 1 + 2 combined report) ──────
    std::cout << "============================================================\n";
    std::cout << "  Detailed Report — SLC (verbose GC enabled)\n";
    std::cout << "============================================================\n\n";

    SSDController ssdVerbose(BLOCKS, PAGES, WATERMARK, LatencyModel::SLC());
    ssdVerbose.setVerbose(true);

    std::cout << "--- Phase A: Initial writes ---\n";
    for (int i = 0; i < TOTAL_LBA; i++)
        ssdVerbose.write(i * 10, "cold_LBA" + std::to_string(i * 10));

    std::cout << "\n--- Phase B: Hot LBA rewrites ---\n";
    for (int w = 0; w < HOT_WRITES; w++)
        for (int i = 0; i < HOT_LBA; i++)
            ssdVerbose.write(i * 10,
                             "hot_v" + std::to_string(w)
                             + "_LBA" + std::to_string(i * 10));

    std::cout << "\n--- Phase C: Read all LBAs ---\n";
    for (int i = 0; i < TOTAL_LBA; i++) {
        std::string val = ssdVerbose.read(i * 10);
        std::cout << "  LBA " << std::setw(3) << (i * 10)
                  << " -> \"" << val << "\"\n";
    }

    ssdVerbose.displayStatus();
    ssdVerbose.printStats();

    return 0;
}