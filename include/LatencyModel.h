#ifndef LATENCYMODEL_H
#define LATENCYMODEL_H

// ─────────────────────────────────────────────────────────────────────────────
// LatencyModel — NAND Flash Timing Constants
//
// Holds per-operation latency values (in microseconds) that reflect real NAND
// flash datasheets:
//
//   tR    (page read)    : time from read command to data-ready on I/O bus
//   tPROG (page program) : time to write one page from cache register to cell
//   tBERS (block erase)  : time to erase all cells in one block
//
// Reference values (Micron MT29F series):
//   Grade  | tR (µs) | tPROG (µs) | tBERS (µs) | Endurance
//   -----  | ------- | ---------- | ---------- | ---------
//   SLC    |   25–50 |    200–500 |   1500–3000| 100 000 P/E
//   MLC    |   50–70 |   800–1800 |   2000–5000|   3 000 P/E
//   TLC    |  70–100 |  2000–3000 |   5000–8000|   1 000 P/E
//
// This class also provides named constructors for each grade so callers can
// say LatencyModel::MLC() instead of remembering exact numbers.
// ─────────────────────────────────────────────────────────────────────────────

struct LatencyModel {

    double pageReadUs;    // µs — tR:    page read latency
    double pageWriteUs;   // µs — tPROG: page program latency
    double blockEraseUs;  // µs — tBERS: block erase latency

    // Default constructor — SLC timings (fastest, most conservative)
    LatencyModel();

    // Explicit constructor for custom timings
    LatencyModel(double readUs, double writeUs, double eraseUs);

    // ── Named constructors for common NAND grades ─────────────────────────
    static LatencyModel SLC();  //  50 µs /   500 µs /  2 000 µs
    static LatencyModel MLC();  //  70 µs / 1 800 µs /  3 500 µs
    static LatencyModel TLC();  // 100 µs / 3 000 µs /  5 000 µs
};

#endif
