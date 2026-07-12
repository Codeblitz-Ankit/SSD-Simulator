#include "StatsTracker.h"
#include <iostream>
#include <iomanip>

// ─── Constructor ──────────────────────────────────────────────────────────────

StatsTracker::StatsTracker()
    : logicalWriteCount(0),
      physicalWriteCount(0),
      gcInvocationCount(0),
      pagesMigrated(0),
      logicalReadCount(0),
      totalReadLatencyUs(0.0),
      totalWriteLatencyUs(0.0),
      totalGCLatencyUs(0.0)
{}

// ─── Phase 1 recording ────────────────────────────────────────────────────────

void StatsTracker::recordLogicalWrite() {
    logicalWriteCount++;
}

void StatsTracker::recordPhysicalWrite() {
    physicalWriteCount++;
}

void StatsTracker::recordGCInvocation() {
    gcInvocationCount++;
}

void StatsTracker::recordPageMigrated() {
    pagesMigrated++;
    physicalWriteCount++;   // every GC migration is a real NAND program op
}

// ─── Phase 2 recording ────────────────────────────────────────────────────────

void StatsTracker::recordLogicalRead() {
    logicalReadCount++;
}

void StatsTracker::chargeReadLatency(double us) {
    totalReadLatencyUs += us;
}

void StatsTracker::chargeWriteLatency(double us) {
    totalWriteLatencyUs += us;
}

void StatsTracker::chargeGCLatency(double us) {
    totalGCLatencyUs += us;
}

// ─── Phase 1 getters ─────────────────────────────────────────────────────────

double StatsTracker::getWAF() const {
    if (logicalWriteCount == 0) return 0.0;
    return static_cast<double>(physicalWriteCount) / logicalWriteCount;
}

int StatsTracker::getGCInvocations()  const { return gcInvocationCount; }
int StatsTracker::getPagesMigrated()  const { return pagesMigrated; }
int StatsTracker::getLogicalWrites()  const { return logicalWriteCount; }
int StatsTracker::getPhysicalWrites() const { return physicalWriteCount; }
int StatsTracker::getLogicalReads()   const { return logicalReadCount; }

// ─── Phase 2 getters ─────────────────────────────────────────────────────────

double StatsTracker::getAvgReadLatencyUs() const {
    if (logicalReadCount == 0) return 0.0;
    return totalReadLatencyUs / logicalReadCount;
}

double StatsTracker::getAvgWriteLatencyUs() const {
    // Amortize the total GC cost (erase + migration writes) across all host
    // writes that triggered it.  This gives the realistic end-to-end cost a
    // host write sees on a busy SSD.
    if (logicalWriteCount == 0) return 0.0;
    return (totalWriteLatencyUs + totalGCLatencyUs) / logicalWriteCount;
}

double StatsTracker::getTotalSimulatedTimeUs() const {
    return totalReadLatencyUs + totalWriteLatencyUs + totalGCLatencyUs;
}

double StatsTracker::getIOPS() const {
    double totalTimeUs = getTotalSimulatedTimeUs();
    if (totalTimeUs <= 0.0) return 0.0;
    double totalOps = static_cast<double>(logicalReadCount + logicalWriteCount);
    return (totalOps / totalTimeUs) * 1.0e6;   // ops/µs → ops/s
}

// ─── Report ───────────────────────────────────────────────────────────────────

void StatsTracker::printReport() const {

    double totalUs = getTotalSimulatedTimeUs();

    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "               SIMULATION STATS REPORT                     \n";
    std::cout << "============================================================\n";

    // ── Phase 1: Write amplification ──────────────────────────────────────
    std::cout << "  [Write Amplification]\n";
    std::cout << "  Logical Writes    (host)    : " << logicalWriteCount  << "\n";
    std::cout << "  Physical Writes   (NAND)    : " << physicalWriteCount << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Write Amp. Factor (WAF)     : " << getWAF()           << "x\n";
    std::cout << "  GC Cycles Triggered         : " << gcInvocationCount  << "\n";
    std::cout << "  Pages Migrated by GC        : " << pagesMigrated      << "\n";

    if (getWAF() <= 1.0) {
        std::cout << "  → WAF = 1.0 — no write amplification.\n";
    } else {
        std::cout << "  → GC wrote "
                  << (physicalWriteCount - logicalWriteCount)
                  << " extra pages beyond host requests.\n";
    }

    // ── Phase 2: Latency & IOPS ───────────────────────────────────────────
    std::cout << "\n  [Latency & IOPS]\n";
    std::cout << "  Logical Reads     (host)    : " << logicalReadCount   << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Avg Read  Latency           : "
              << getAvgReadLatencyUs()  << " µs\n";
    std::cout << "  Avg Write Latency (w/ GC)   : "
              << getAvgWriteLatencyUs() << " µs\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Total Simulated Time        : "
              << (totalUs / 1000.0)     << " ms\n";
    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  Estimated IOPS              : "
              << getIOPS()              << "\n";

    std::cout << "\n  [Latency Breakdown]\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Read  ops  total            : "
              << (totalReadLatencyUs  / 1000.0) << " ms\n";
    std::cout << "  Write ops  total (host)     : "
              << (totalWriteLatencyUs / 1000.0) << " ms\n";
    std::cout << "  GC    ops  total            : "
              << (totalGCLatencyUs    / 1000.0) << " ms\n";
    std::cout << "============================================================\n";
    std::cout << "\n";
}
