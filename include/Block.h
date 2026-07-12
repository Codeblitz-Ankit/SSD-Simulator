#ifndef BLOCK_H
#define BLOCK_H

#include <vector>
#include "Page.h"

// ─────────────────────────────────────────────────────────────────────────────
// Block — Models a NAND erase unit
//
// Phase 3 additions:
//   peLimit    — P/E cycle limit (default 3000 for MLC NAND).
//                Set lower in demo/test (e.g., 50) to observe wear effects.
//   isWornOut  — true when eraseCount >= peLimit.
//   getWearRatio — 0.0 (fresh) → 1.0 (at limit) → >1.0 (over-limit, error).
// ─────────────────────────────────────────────────────────────────────────────

class Block {
private:
    std::vector<Page> pages;
    int eraseCount;
    int peLimit;        // Phase 3: maximum P/E cycles before block retirement

public:
    // numPages  — pages per block (e.g., 8, 64, 256)
    // peLimit   — P/E cycle endurance limit (default: 3000 = typical MLC NAND)
    Block(int numPages, int peLimit = 3000);

    Page&       getPage(int index);
    const Page& getPage(int index) const;

    int  getFreePageIndex()    const;

    int  getEraseCount()       const;
    int  getPELimit()          const;   // Phase 3

    // Phase 3: Wear status
    bool   isWornOut()         const;   // eraseCount >= peLimit
    double getWearRatio()      const;   // eraseCount / peLimit (0.0 – 1.0+)

    void eraseBlock();

    int  getTotalPages()       const;

    int  getInvalidPageCount() const;
    int  getFreePageCount()    const;
    int  getValidPageCount()   const;
};

#endif
