# SSD Simulator

A **realistic, parameterizable NAND Flash SSD Simulator** written in modern C++17. This project models the core internals of an SSD controller — including out-of-place writes, Flash Translation Layer (FTL) mapping, garbage collection with valid-page migration, Write Amplification Factor (WAF) measurement, latency modelling, wear leveling, DRAM write buffering, trace-file workload replay, and an interactive web visualizer.

---

## Architecture

```
Host Application (main.cpp)
        │
        ▼
┌─────────────────────────────────────┐
│         SSDController               │
│  ┌──────────────┐ ┌──────────────┐  │
│  │  FTLMapper   │ │ StatsTracker │  │
│  │  LBA → PBA   │ │ WAF/GC/IOPS │  │
│  │  PBA → LBA   │ │  Latency    │  │
│  └──────────────┘ └──────────────┘  │
│  ┌──────────────┐ ┌──────────────┐  │
│  │  PageCache   │ │LatencyModel  │  │
│  │  DRAM LRU   │ │SLC/MLC/TLC  │  │
│  └──────────────┘ └──────────────┘  │
│  ┌──────────────────────────────┐   │
│  │       GarbageCollector       │   │
│  │  Victim select → Migrate →   │   │
│  │  Erase + Static Wear Level   │   │
│  └──────────────────────────────┘   │
│  ┌──────────────────────────────┐   │
│  │        FlashMemory           │   │
│  │  Block[0] Block[1] Block[N]  │   │
│  │   Page[]   Page[]   Page[]   │   │
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────┐
│   Trace Parser + Request Queue      │
│   (Phase 5: workload replay)        │
└─────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────┐
│   Web Visualizer Dashboard          │
│   visualizer/index.html (Phase 6)  │
└─────────────────────────────────────┘
```

---

## Features

### Phase 1 — Core Storage Model ✅
- **Page → Block → FlashMemory** hierarchy modelling NAND flash storage
- **Out-of-place writes**: pages cannot be overwritten in-place — new writes always go to a free page; old page is marked invalid (exact NAND constraint)
- **Flash Translation Layer (FTL)**: O(1) forward lookup (`LBA → PBA`) + O(n) reverse lookup (`PBA → LBA`) via `FTLMapper`
- **Garbage Collector**: watermark-triggered, greedy victim selection, valid-page migration
- **Write Amplification Factor (WAF)**: `StatsTracker` tracks every logical write and physical NAND page program

### Phase 2 — Latency Model + IOPS ✅
- **LatencyModel**: Real NAND flash timing constants for SLC / MLC / TLC grades
  - SLC: 50 µs read / 500 µs write / 2 ms erase
  - MLC: 70 µs read / 1.8 ms write / 3.5 ms erase
  - TLC: 100 µs read / 3 ms write / 5 ms erase
- **Latency accounting**: `StatsTracker` accumulates host-read, host-write, and GC latency separately
- **IOPS estimation**: `getIOPS()` = total ops / total simulated time
- **Amortized write latency**: GC cost spread across all host writes that triggered it

### Phase 3 — Wear Leveling + Lifespan Estimation ✅
- **Dynamic wear leveling**: `findLeastUsedFreeBlock()` always writes to the lowest-erase-count block
- **Static wear leveling**: automatically triggered every N GC cycles; moves cold data from fresh blocks onto worn free blocks, then erases the fresh blocks for hot-write traffic — reduces erase variance
- **Lifespan estimate**: `estimateRemainingWrites(peLimit, maxErase, totalPages)` predicts host writes remaining before the most-worn block hits its P/E limit
- **Wear analysis display**: ASCII erase-count bar chart + statistical summary (mean, variance, std dev, spread)

### Phase 4 — DRAM Page Cache (LRU) ✅
- **O(1) LRU cache**: doubly-linked list + `unordered_map` — classic optimal LRU design
- **Write coalescing**: writes to an LBA already cached avoid a NAND program entirely
- **Read-through**: cache hit returns at DRAM latency (~0.1 µs); miss falls through to NAND, then inserts clean entry
- **Dirty-eviction flush**: LRU evictions of dirty entries trigger a background NAND write
- **Cache metrics**: hit ratio, writes coalesced, dirty evictions — demonstrated in `main.cpp` Phase 4 scenario
- **15× IOPS improvement** demonstrated (90.7% hit rate on a hot-write workload)

### Phase 5 — Trace File Parser + Workload Replay ✅
- **Trace file format**: simple text format with `W <LBA> <data>` and `R <LBA>` operations (comments with `#`)
- **TraceParser**: reads trace files and populates a `RequestQueue` with `WriteRequest` / `ReadRequest` objects
- **RequestQueue**: thread-safe queue of polymorphic `IORequest` objects
- **Sample trace**: `traces/sample.trace` — cold fill, hot rewrites, and read verification

### Phase 6 — Web Visualizer Dashboard ✅
- **Live animated NAND heatmap**: per-block page state (free/valid/invalid) with flash animation on write
- **WAF over time chart**: tracks write amplification factor across all operations
- **Erase count bar chart**: per-block wear distribution (worn-out blocks shown in red)
- **IOPS over time chart**: estimated throughput as simulation runs
- **LRU cache donut chart**: hits vs misses with coalesced write count
- **GC event log**: real-time event stream with color-coded categories (GC, WL, errors)
- **Configurable parameters**: geometry, NAND grade, watermark, cache size, wear-level interval, workload shape
- **Playback speed control**: 1×, 5×, 20×, or max speed
- Open `visualizer/index.html` in any modern browser — **no server needed**

---

## Build & Run

**Requirements**: CMake ≥ 3.10, C++17-compatible compiler (GCC ≥ 7 / Clang ≥ 5)

```bash
git clone https://github.com/Codeblitz-Ankit/SSD-Simulator.git
cd SSD-Simulator
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./ssd_simulator
```

The simulator binary should be run from the `build/` directory (default). The trace file is resolved relative to the project root automatically.

---

## Trace File Format (Phase 5)

Create a `.trace` file with one operation per line:

```
# This is a comment
W 0 some_data_here         # write "some_data_here" to LBA 0
W 10 more_data             # write to LBA 10
R 0                        # read from LBA 0
R 10                       # read from LBA 10
```

| Token | Meaning |
|---|---|
| `W <LBA> <data>` | Write `data` (rest of line, may contain spaces) to LBA |
| `R <LBA>` | Read from LBA |
| `# ...` | Comment — ignored |
| (blank line) | Ignored |

---

## Web Visualizer (Phase 6)

Open `visualizer/index.html` in any modern browser (Chrome, Firefox, Safari, Edge).

1. Configure geometry and workload in the left sidebar
2. Click **▶ Run Simulation**
3. Watch the NAND block map, WAF chart, erase distribution, IOPS and cache charts update live
4. Hover over individual pages in the block map to see their state and LBA mapping
5. Use speed controls (1×, 5×, 20×, Max) to adjust playback rate

---

## Project Structure

```
ssd_simulator/
├── include/
│   ├── Page.h               # NAND page (free/valid/invalid states)
│   ├── Block.h              # Block of pages + erase count + PE limit
│   ├── FlashMemory.h        # Array of blocks + wear-level helpers
│   ├── FTLMapper.h          # LBA↔PBA mapping table
│   ├── PhysicalAddress.h    # {blockIndex, pageIndex} struct
│   ├── GarbageCollector.h   # Greedy GC + static wear leveling
│   ├── StatsTracker.h       # WAF + GC + latency + IOPS metrics
│   ├── LatencyModel.h       # NAND timing constants (SLC/MLC/TLC)
│   ├── PageCache.h          # DRAM LRU write buffer / read cache
│   ├── SSDController.h      # Top-level controller (write/read/GC)
│   ├── IORequest.h          # Abstract IO operation interface
│   ├── WriteRequest.h       # Concrete write IO request
│   ├── ReadRequest.h        # Concrete read IO request  [Phase 5]
│   ├── RequestQueue.h       # Thread-safe IO request queue
│   └── TraceParser.h        # Trace file parser          [Phase 5]
├── src/
│   ├── main.cpp             # Phases 3/4/5 demo workloads
│   ├── Page.cpp
│   ├── Block.cpp
│   ├── FlashMemory.cpp
│   ├── FTLMapper.cpp
│   ├── GarbageCollector.cpp
│   ├── StatsTracker.cpp
│   ├── LatencyModel.cpp
│   ├── PageCache.cpp
│   ├── SSDController.cpp
│   ├── WriteRequest.cpp
│   ├── ReadRequest.cpp      # [Phase 5]
│   ├── RequestQueue.cpp
│   └── TraceParser.cpp      # [Phase 5]
├── traces/
│   └── sample.trace         # Mixed cold/hot/read workload trace
├── visualizer/
│   └── index.html           # Interactive web dashboard [Phase 6]
├── tests/                   # Unit tests (future)
├── docs/
├── CMakeLists.txt
└── README.md
```

---

## Key Concepts Demonstrated

| Concept | Where |
|---|---|
| **NAND erase-before-write constraint** | `Page::writeData()` only works on free pages; `Block::eraseBlock()` resets all |
| **Out-of-place update (copy-on-write)** | `SSDController::write()` — always programs a fresh page, then invalidates old |
| **Flash Translation Layer (FTL)** | `FTLMapper` — hides physical addressing from the host OS |
| **Garbage Collection** | `GarbageCollector` — victim pick, live-page migration, block erase |
| **Write Amplification Factor** | `StatsTracker::getWAF()` — physical writes ÷ logical writes |
| **Dynamic Wear Leveling** | `FlashMemory::findLeastUsedFreeBlock()` — always writes to lowest-erase-count block |
| **Static Wear Leveling** | `GarbageCollector::staticWearLevel()` — cold-data migration to reduce erase variance |
| **Lifespan Estimation** | `StatsTracker::estimateRemainingWrites()` — remaining host writes before first block wears out |
| **NAND Timing Model** | `LatencyModel` — SLC/MLC/TLC tR / tPROG / tBERS constants |
| **DRAM Write Buffer / LRU Cache** | `PageCache` — write coalescing + read-through with O(1) LRU eviction |
| **Trace-driven Workloads** | `TraceParser` + `RequestQueue` — replay `W`/`R` operations from text files |
| **Live Web Visualizer** | `visualizer/index.html` — interactive JS simulation with canvas charts |

---

## Phase Roadmap

| Phase | Feature | Status |
|---|---|---|
| 1 | **Realistic GC + WAF tracking** | ✅ Complete |
| 2 | **Latency model + IOPS** | ✅ Complete |
| 3 | **Wear leveling + lifespan estimation** | ✅ Complete |
| 4 | **DRAM page cache (LRU)** | ✅ Complete — 15× IOPS gain demonstrated |
| 5 | **Trace file parser + workload replay** | ✅ Complete |
| 6 | **Web visualizer dashboard** | ✅ Complete |

---

## License

MIT License
