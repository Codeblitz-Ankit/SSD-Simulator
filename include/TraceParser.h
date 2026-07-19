#ifndef TRACEPARSER_H
#define TRACEPARSER_H

// ─────────────────────────────────────────────────────────────────────────────
// TraceParser — Load and replay workload trace files (Phase 5)
//
// Trace file format (one operation per line):
//
//   W <LBA> <data>        — write <data> to logical block address <LBA>
//   R <LBA>               — read logical block address <LBA>
//   #  ...                — comment line (ignored)
//   (blank lines)         — ignored
//
// Example trace:
//   # Phase A: cold fill
//   W 0 cold_data_0
//   W 10 cold_data_10
//   # Phase B: hot rewrites
//   W 200 hot_v1
//   W 200 hot_v2
//   R 200
//
// Usage:
//   TraceParser parser("traces/workload.trace");
//   parser.loadInto(queue, ssd);
//   while (!queue.isEmpty()) queue.getNextRequest()->execute();
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include "RequestQueue.h"
#include "SSDController.h"

class TraceParser {
private:
    std::string filePath_;

    // Counts of parsed operations (set by loadInto)
    int parsedWrites_;
    int parsedReads_;
    int skippedLines_;

public:
    explicit TraceParser(const std::string& filePath);

    // Parse the trace file and push all operations into `queue`.
    // Each write becomes a WriteRequest; each read becomes a ReadRequest.
    // Returns true on success, false if the file could not be opened.
    bool loadInto(RequestQueue& queue, SSDController& ssd);

    // Post-load statistics
    int getParsedWrites()  const { return parsedWrites_; }
    int getParsedReads()   const { return parsedReads_; }
    int getSkippedLines()  const { return skippedLines_; }
    int getTotalOps()      const { return parsedWrites_ + parsedReads_; }
};

#endif
