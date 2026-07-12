# SSD Simulator

A **realistic, parameterizable NAND Flash SSD Simulator** written in modern C++17. This project models the core internals of an SSD controller — including out-of-place writes, Flash Translation Layer (FTL) mapping, garbage collection with valid-page migration, and Write Amplification Factor (WAF) measurement — and is being progressively upgraded to benchmark-grade fidelity.

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
│  │  LBA → PBA   │ │ WAF/GC stats │  │
│  │  PBA → LBA   │ │              │  │
│  └──────────────┘ └──────────────┘  │
│  ┌──────────────────────────────┐   │
│  │       GarbageCollector       │   │
│  │  Victim select → Migrate →   │   │
│  │  Erase (watermark-triggered) │   │
│  └──────────────────────────────┘   │
│  ┌──────────────────────────────┐   │
│  │        FlashMemory           │   │
│  │  Block[0] Block[1] Block[N]  │   │
│  │   Page[]   Page[]   Page[]   │   │
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
```

---

## Features (Phase 1 Complete ✅)

### Core Storage Model
- **Page → Block → FlashMemory** hierarchy modelling NAND flash storage
- **Out-of-place writes**: pages cannot be overwritten in-place — new writes always go to a free page; old page is marked invalid (exact NAND constraint)
- **Flash Translation Layer (FTL)**: O(1) forward lookup (`LBA → PBA`) + O(n) reverse lookup (`PBA → LBA`) via `FTLMapper`

### Garbage Collector
- **Watermark-triggered**: GC fires automatically when free-page ratio drops below a configurable threshold (default 20%)
- **Greedy victim selection**: always picks the block with the most invalid pages — maximises space reclaimed per erase cycle
- **Valid-page migration**: surviving live pages are copied to a free slot in another block, with FTL remapping, before the victim block is erased
- **Forced fallback**: if watermark GC isn't enough, a second forced cycle runs before declaring the SSD full

### Write Amplification Factor (WAF)
- `StatsTracker` tracks every logical write (host) and every physical NAND page program (host + GC migration)
- `WAF = physicalWrites / logicalWrites` — printed live at end of run
- Also reports: GC invocations, pages migrated, breakdown of amplification source

### Wear-Aware Block Allocation
- `findLeastUsedFreeBlock()` always picks the free block with the lowest erase count — basic dynamic wear leveling out of the box

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

---

## Demo Output (Phase 1)

Workload: 4 blocks × 4 pages SSD, 12 unique LBAs written once, then 3 "hot" LBAs rewritten 40× each.

```
============================================================
  SSD Simulator — Phase 1: Realistic GC + WAF
============================================================
  Geometry  : 4 blocks x 4 pages = 16 pages total
  Watermark : 25%  (GC fires when free pages < 4)
  Phase A   : write 12 LBAs once
  Phase B   : overwrite 3 hot LBAs x 40 times
============================================================

--- Phase A: Initial write to all 12 LBAs ---
--- Phase B: Overwrite hot LBAs 40 times each ---

[GC] >>> Cycle triggered. Victim = Block 0  (invalid=1, valid=3, free=0)
[GC]   Migrated LBA 20 : (0,0) -> (3,1)
[GC]   Migrated LBA 10 : (0,1) -> (3,2)
[GC]   Migrated LBA 30 : (0,3) -> (3,3)
[GC] <<< Block 0 erased. Erase count = 1
...
  (Total host writes: 132)

--- Final Read Verification ---
  LBA 0  -> "hot_v39_LBA0"
  LBA 10 -> "hot_v39_LBA10"
  LBA 20 -> "hot_v39_LBA20"
  LBA 30 -> "init_LBA30"       ← cold data preserved correctly
  ...

===== SSD BLOCK STATUS =====
  Block 0 | Free: 3 | Valid: 1 | Invalid: 0 | Erases: 60
  Block 1 | Free: 0 | Valid: 4 | Invalid: 0 | Erases: 0
  Block 2 | Free: 0 | Valid: 4 | Invalid: 0 | Erases: 0
  Block 3 | Free: 0 | Valid: 3 | Invalid: 1 | Erases: 59
============================

============================================
            SIMULATION STATS REPORT
============================================
  Logical Writes   (host)     : 132
  Physical Writes  (NAND)     : 489
  Write Amplification (WAF)   : 3.70x
  GC Cycles Triggered         : 119
  Pages Migrated by GC        : 357
============================================
  [WAF > 1.0 — GC wrote 357 extra pages beyond host requests]
```

---

## Project Structure

```
ssd_simulator/
├── include/
│   ├── Page.h               # NAND page (free/valid/invalid states)
│   ├── Block.h              # Block of pages + erase count
│   ├── FlashMemory.h        # Array of blocks + free-slot finder
│   ├── FTLMapper.h          # LBA↔PBA mapping table
│   ├── PhysicalAddress.h    # {blockIndex, pageIndex} struct
│   ├── GarbageCollector.h   # Greedy GC with valid-page migration
│   ├── StatsTracker.h       # WAF + GC metrics
│   ├── SSDController.h      # Top-level controller (write/read/GC)
│   ├── IORequest.h          # Abstract IO operation interface
│   ├── WriteRequest.h       # Concrete write IO request
│   └── RequestQueue.h       # Thread-safe IO request queue
├── src/
│   ├── main.cpp             # Phase 1 WAF demo workload
│   ├── Page.cpp
│   ├── Block.cpp
│   ├── FlashMemory.cpp
│   ├── FTLMapper.cpp
│   ├── GarbageCollector.cpp
│   ├── StatsTracker.cpp
│   ├── SSDController.cpp
│   ├── WriteRequest.cpp
│   └── RequestQueue.cpp
├── tests/                   # Unit tests (Phase 2+)
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

---

## Phase Roadmap

| Phase | Feature | Status |
|---|---|---|
| 1 | **Realistic GC + WAF tracking** | ✅ Complete — WAF 3.70x demonstrated |
| 2 | **Latency model + IOPS** | 🔜 Next — 50µs read / 500µs write / 2ms erase |
| 3 | **Wear leveling + lifespan estimation** | 📋 Planned |
| 4 | **DRAM page cache (LRU)** | 📋 Planned |
| 5 | **Trace file parser + workload replay** | 📋 Planned |
| 6 | **Web visualizer dashboard** | 📋 Planned |

---

## License

MIT License
