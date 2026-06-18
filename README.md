# SSD Simulator

A **realistic, parameterizable NAND Flash SSD Simulator** written in modern C++17. This project models the core internals of an SSD controller — including out-of-place writes, Flash Translation Layer (FTL) mapping, and garbage collection — and is being progressively upgraded to benchmark-grade fidelity.

---

## Current Features

- **Page → Block → Flash** hierarchy modelling NAND flash storage
- **Out-of-place writes** (reflecting real NAND constraints — pages cannot be overwritten without a full block erase)
- **Flash Translation Layer (FTL)**: page-level LBA → PBA mapping via `FTLMapper`
- **Basic Garbage Collection**: erases blocks when all pages are invalid
- **Request Queue**: `IORequest` abstraction for dispatching read/write operations
- **Wear-aware block selection**: `findLeastUsedFreeBlock()` prefers lower erase-count blocks

---

## Planned Upgrades (Phase Roadmap)

| Phase | Feature | Key Metric |
|-------|---------|-----------|
| 1 | **Realistic GC + WAF tracking** | Write Amplification Factor |
| 2 | **Latency model + IOPS** | µs per op, IOPS |
| 3 | **Wear leveling + lifespan estimation** | Erase count variance, P/E cycles remaining |
| 4 | **DRAM page cache (LRU)** | Cache hit ratio, WAF reduction |
| 5 | **Trace file parser + workload replay** | Real workload benchmarking |
| 6 | **Web visualizer dashboard** | Block heat-map, live IOPS/WAF charts |

---

## Project Structure

```
ssd_simulator/
├── include/              # Header files
│   ├── Page.h
│   ├── Block.h
│   ├── FlashMemory.h
│   ├── FTLMapper.h
│   ├── PhysicalAddress.h
│   ├── SSDController.h
│   ├── IORequest.h
│   ├── WriteRequest.h
│   └── RequestQueue.h
├── src/                  # Implementation files
│   ├── main.cpp
│   ├── Page.cpp
│   ├── Block.cpp
│   ├── FlashMemory.cpp
│   ├── FTLMapper.cpp
│   ├── SSDController.cpp
│   ├── WriteRequest.cpp
│   └── RequestQueue.cpp
├── tests/                # (Unit tests — coming soon)
├── docs/                 # Documentation
├── CMakeLists.txt
└── README.md
```

---

## Build & Run

**Requirements**: CMake ≥ 3.10, C++17-compatible compiler (GCC ≥ 7, Clang ≥ 5)

```bash
mkdir build && cd build
cmake ..
make
./ssd_simulator
```

**Expected output (current baseline):**

```
Operating Systems
Computer Networks

===== SSD STATUS =====
Block 0 | Free: 2 | Valid: 2 | Invalid: 0 | Erase Count: 0
Block 1 | Free: 4 | Valid: 0 | Invalid: 0 | Erase Count: 0
======================
```

---

## Key Concepts Demonstrated

- **NAND flash constraints**: Why you can't overwrite a page in-place (erase-before-write)
- **FTL (Flash Translation Layer)**: How SSDs hide physical addressing complexity from the OS
- **Garbage Collection**: Why GC is necessary and how it introduces write amplification
- **Wear Leveling**: How SSDs distribute writes evenly to extend NAND lifespan

---

## License

MIT License
