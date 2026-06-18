// ============================================================
//  SSD Simulator — Phase 1 Demo
//  Realistic Garbage Collection + Write Amplification Factor
// ============================================================
//
//  Scenario
//  --------
//  SSD geometry : 4 blocks × 4 pages = 16 pages total
//  GC watermark : 25% free pages  (= 4 pages)
//
//  Workload
//  --------
//  Phase A: Write to 12 unique LBAs once each (fills 12/16 pages)
//  Phase B: Repeatedly overwrite only 3 "hot" LBAs (40 times each)
//
//  Why WAF > 1 happens
//  -------------------
//  After Phase A, each block holds a mix of cold (valid) pages.
//  In Phase B, the 3 hot LBAs get overwritten → their old physical pages
//  become invalid.  Eventually a block has some invalid + some valid pages.
//  GC must migrate the surviving valid (cold) pages before erasing the block.
//  Those migration writes are EXTRA physical writes → WAF > 1.

#include <iostream>
#include <string>
#include "SSDController.h"

int main() {

    // 4 blocks × 4 pages = 16 pages total; 25% watermark = 4 pages
    SSDController ssd(4, 4, 0.25);

    const int TOTAL_LBA = 12;   // 12 unique logical addresses
    const int HOT_LBA   = 3;    // first 3 are "hot" (frequently rewritten)
    const int HOT_WRITES = 40;  // times each hot LBA is overwritten in Phase B

    std::cout << "============================================================\n";
    std::cout << "  SSD Simulator — Phase 1: Realistic GC + WAF\n";
    std::cout << "============================================================\n";
    std::cout << "  Geometry  : 4 blocks x 4 pages = 16 pages total\n";
    std::cout << "  Watermark : 25%  (GC fires when free pages < 4)\n";
    std::cout << "  Phase A   : write " << TOTAL_LBA << " LBAs once\n";
    std::cout << "  Phase B   : overwrite " << HOT_LBA
              << " hot LBAs x " << HOT_WRITES << " times\n";
    std::cout << "============================================================\n\n";

    // ── Phase A: write all 12 LBAs once ──────────────────────────────────────
    std::cout << "--- Phase A: Initial write to all " << TOTAL_LBA
              << " LBAs ---\n";

    for (int i = 0; i < TOTAL_LBA; i++) {
        ssd.write(i * 10, "init_LBA" + std::to_string(i * 10));
    }

    // ── Phase B: hammer the 3 hot LBAs ───────────────────────────────────────
    std::cout << "\n--- Phase B: Overwrite hot LBAs "
              << HOT_WRITES << " times each ---\n";

    for (int w = 0; w < HOT_WRITES; w++) {
        for (int i = 0; i < HOT_LBA; i++) {
            ssd.write(i * 10,
                      "hot_v" + std::to_string(w)
                      + "_LBA" + std::to_string(i * 10));
        }
    }

    int totalWrites = TOTAL_LBA + (HOT_LBA * HOT_WRITES);
    std::cout << "\n  (Total host writes: " << totalWrites << ")\n";

    // ── Verification reads ────────────────────────────────────────────────────
    std::cout << "\n--- Final Read Verification ---\n";

    for (int i = 0; i < TOTAL_LBA; i++) {
        int lba = i * 10;
        std::cout << "  LBA " << lba << " -> \""
                  << ssd.read(lba) << "\"\n";
    }

    // ── Block map ─────────────────────────────────────────────────────────────
    ssd.displayStatus();

    // ── WAF & GC summary ─────────────────────────────────────────────────────
    ssd.printStats();

    return 0;
}