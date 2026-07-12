#include "LatencyModel.h"

// ─── Default constructor (SLC timings) ───────────────────────────────────────
LatencyModel::LatencyModel()
    : pageReadUs(50.0),
      pageWriteUs(500.0),
      blockEraseUs(2000.0),
      dramReadUs(0.1)
{}

// ─── Custom constructor ───────────────────────────────────────────────────────
LatencyModel::LatencyModel(double readUs, double writeUs, double eraseUs)
    : pageReadUs(readUs),
      pageWriteUs(writeUs),
      blockEraseUs(eraseUs),
      dramReadUs(0.1)   // DRAM latency is grade-independent
{}

// ─── Named constructors ───────────────────────────────────────────────────────

// SLC: Single-Level Cell
// Fastest grade — used in enterprise/industrial SSDs
// tR=50µs, tPROG=500µs, tBERS=2000µs
LatencyModel LatencyModel::SLC() {
    return LatencyModel(50.0, 500.0, 2000.0);
}

// MLC: Multi-Level Cell (2 bits/cell)
// Mid-range — used in most consumer SSDs (e.g. Samsung 860 EVO)
// tR=70µs, tPROG=1800µs, tBERS=3500µs
LatencyModel LatencyModel::MLC() {
    return LatencyModel(70.0, 1800.0, 3500.0);
}

// TLC: Triple-Level Cell (3 bits/cell)
// Highest density, lowest cost — used in QLC consumer SSDs
// tR=100µs, tPROG=3000µs, tBERS=5000µs
LatencyModel LatencyModel::TLC() {
    return LatencyModel(100.0, 3000.0, 5000.0);
}
