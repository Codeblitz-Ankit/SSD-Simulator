#include "StatsTracker.h"
#include <iostream>
#include <iomanip>

StatsTracker::StatsTracker()
    : logicalWriteCount(0),
      physicalWriteCount(0),
      gcInvocationCount(0),
      pagesMigrated(0) {}

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
    physicalWriteCount++;   // every migration is a real NAND program operation
}

double StatsTracker::getWAF() const {
    if (logicalWriteCount == 0) return 0.0;
    return static_cast<double>(physicalWriteCount) / logicalWriteCount;
}

int StatsTracker::getGCInvocations()  const { return gcInvocationCount; }
int StatsTracker::getPagesMigrated()  const { return pagesMigrated; }
int StatsTracker::getLogicalWrites()  const { return logicalWriteCount; }
int StatsTracker::getPhysicalWrites() const { return physicalWriteCount; }

void StatsTracker::printReport() const {

    std::cout << "\n";
    std::cout << "============================================\n";
    std::cout << "            SIMULATION STATS REPORT         \n";
    std::cout << "============================================\n";
    std::cout << "  Logical Writes   (host)     : "
              << logicalWriteCount  << "\n";
    std::cout << "  Physical Writes  (NAND)     : "
              << physicalWriteCount << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Write Amplification (WAF)   : "
              << getWAF()           << "x\n";
    std::cout << "  GC Cycles Triggered         : "
              << gcInvocationCount  << "\n";
    std::cout << "  Pages Migrated by GC        : "
              << pagesMigrated      << "\n";
    std::cout << "============================================\n";

    if (getWAF() <= 1.0) {
        std::cout << "  [WAF = 1.0 — no amplification yet]\n";
    } else {
        std::cout << "  [WAF > 1.0 — GC wrote "
                  << (physicalWriteCount - logicalWriteCount)
                  << " extra pages beyond host requests]\n";
    }

    std::cout << "\n";
}
