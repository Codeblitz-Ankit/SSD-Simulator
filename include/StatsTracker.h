#ifndef STATSTRACKER_H
#define STATSTRACKER_H

// ─────────────────────────────────────────────────────────────────────────────
// StatsTracker — Simulation Metrics Accumulator
//
// Phase 1 metrics:
//   logicalWriteCount   — host writes issued
//   physicalWriteCount  — NAND page programs (host + GC migration)
//   gcInvocationCount   — number of GC cycles that ran
//   pagesMigrated       — valid pages copied during GC
//   WAF                 = physicalWrites / logicalWrites
//
// Phase 2 additions:
//   logicalReadCount    — host reads issued
//   totalReadLatencyUs  — sum of all host read latencies (µs)
//   totalWriteLatencyUs — sum of all host write NAND programs (µs)
//   totalGCLatencyUs    — sum of all GC page-program + block-erase ops (µs)
//   avgReadLatencyUs    = totalReadLatencyUs / logicalReadCount
//   avgWriteLatencyUs   = (totalWriteLatencyUs + totalGCLatencyUs)
//                         / logicalWriteCount   [amortized GC cost]
//   IOPS                = (reads + writes) / totalSimulatedTimeUs * 1e6
// ─────────────────────────────────────────────────────────────────────────────

class StatsTracker {

private:
    // ── Phase 1 ───────────────────────────────────────────────────────────
    int logicalWriteCount;
    int physicalWriteCount;
    int gcInvocationCount;
    int pagesMigrated;

    // ── Phase 2 ───────────────────────────────────────────────────────────
    int    logicalReadCount;

    double totalReadLatencyUs;   // host read NAND accesses only
    double totalWriteLatencyUs;  // host write NAND programs only
    double totalGCLatencyUs;     // GC erase ops + GC migration writes

    // ── Phase 3 ───────────────────────────────────────────────────────────
    int    staticWearLevelCount;

public:
    StatsTracker();

    // ── Phase 1 recording ─────────────────────────────────────────────────
    void recordLogicalWrite();
    void recordPhysicalWrite();
    void recordGCInvocation();
    void recordPageMigrated();   // also bumps physicalWriteCount

    // ── Phase 2 recording ─────────────────────────────────────────────────
    void recordLogicalRead();

    // Latency charging — each call adds `us` microseconds to the accumulator.
    // These are called by SSDController and GarbageCollector after every
    // NAND operation, using values from the active LatencyModel.
    void chargeReadLatency (double us);
    void chargeWriteLatency(double us);   // host writes only
    void chargeGCLatency   (double us);   // GC page-program + block-erase

    // ── Phase 3 recording ─────────────────────────────────────────────────
    void recordStaticWearLevel();

    // ── Phase 1 getters ───────────────────────────────────────────────────
    double getWAF()           const;
    int    getGCInvocations() const;
    int    getPagesMigrated() const;
    int    getLogicalWrites() const;
    int    getPhysicalWrites()const;
    int    getLogicalReads()  const;
    int    getStaticWearLevelCount() const;

    // ── Phase 2 getters ───────────────────────────────────────────────────

    // Average latency seen by the host for a single read request (µs)
    double getAvgReadLatencyUs()  const;

    // Average latency seen by the host for a single write request (µs).
    // Computed as (hostWriteLatency + GCLatency) / logicalWrites so that
    // GC overhead is amortized across every host write that caused it.
    double getAvgWriteLatencyUs() const;

    // Total simulated wall-clock time (µs) = read + write + GC latency sums
    double getTotalSimulatedTimeUs() const;

    // Estimated host IOPS = (reads + writes) / totalSimulatedTime[s]
    double getIOPS() const;

    // ── Phase 3: Lifespan estimation ──────────────────────────────────────
    //
    // Conservative estimate of remaining HOST writes before the most-worn
    // block hits its P/E cycle limit.
    //
    // Formula: hostRemaining = (peLimit - maxEraseNow) × totalPages / WAF
    //   (peLimit - maxEraseNow) = remaining erases for the worst block
    //   totalPages / WAF        = host writes consumed per erase cycle on avg
    //
    // Returns LLONG_MAX when no writes have been made yet (WAF = 0).
    long long estimateRemainingWrites(
        int peLimit,
        int maxEraseNow,
        int totalPages
    ) const;

    // ── Report ────────────────────────────────────────────────────────────
    void printReport() const;
};

#endif
