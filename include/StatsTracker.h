#ifndef STATSTRACKER_H
#define STATSTRACKER_H

class StatsTracker {

private:
    int logicalWriteCount;
    int physicalWriteCount;
    int gcInvocationCount;
    int pagesMigrated;

public:
    StatsTracker();

    // Called once per host-level logical write
    void recordLogicalWrite();

    // Called for every NAND page programmed (host write)
    void recordPhysicalWrite();

    // Called once each time the GC cycle is invoked
    void recordGCInvocation();

    // Called for each valid page moved during GC
    // (also bumps physicalWriteCount — migration IS a physical write)
    void recordPageMigrated();

    double getWAF()            const;
    int    getGCInvocations()  const;
    int    getPagesMigrated()  const;
    int    getLogicalWrites()  const;
    int    getPhysicalWrites() const;

    void printReport() const;
};

#endif
