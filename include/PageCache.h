#ifndef PAGECACHE_H
#define PAGECACHE_H

// ─────────────────────────────────────────────────────────────────────────────
// PageCache — DRAM Write Buffer / Read Cache (LRU eviction policy)
//
// Models the DRAM buffer found in every real SSD firmware:
//
//   Write path  :  data → cache (dirty).  If LBA already in cache, the write
//                  is coalesced (old dirty entry updated in-place, NO NAND
//                  write happens).  When a dirty entry is evicted, caller must
//                  flush it to NAND.
//
//   Read path   :  cache hit  → return data at DRAM latency (~0.1 µs).
//                  cache miss → fall through to NAND, then insert clean entry.
//
// Implementation: doubly-linked list (O(1) LRU reorder) + unordered_map
//   (O(1) lookup by LBA) — the classic O(1) LRU design.
//
//   lruList_ : front = MRU (most recently used), back = LRU (victim)
//   index_   : LBA → iterator into lruList_
//
// Metrics tracked (independent of StatsTracker):
//   cacheHits_       — reads + writes that found the LBA in cache
//   cacheMisses_     — reads + writes that did not find the LBA
//   writesCoalesced_ — writes that hit a dirty entry (NAND write avoided)
//   dirtyEvictions_  — LRU evictions of dirty entries (caller flushes to NAND)
// ─────────────────────────────────────────────────────────────────────────────

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

class PageCache {
public:
    // ── Cache entry ───────────────────────────────────────────────────────
    struct Entry {
        int         lba;
        std::string data;
        bool        dirty;  // true = must flush to NAND before discarding
    };

private:
    int capacity_;

    // LRU list: front = MRU, back = LRU victim
    std::list<Entry>                                     lruList_;
    // Fast O(1) lookup by LBA
    std::unordered_map<int, std::list<Entry>::iterator>  index_;

    // ── Stats ─────────────────────────────────────────────────────────────
    int cacheHits_;
    int cacheMisses_;
    int writesCoalesced_;  // writes that updated an existing dirty entry
    int dirtyEvictions_;   // LRU evictions where caller must flush to NAND

    // ── Private helpers ───────────────────────────────────────────────────

    // Evicts the LRU (back) entry if the cache is at capacity.
    // Returns true + fills outEvicted if the evicted entry was dirty.
    // Returns false for clean evictions (caller needs no NAND action).
    bool evictIfFull(Entry& outEvicted);

public:
    // capacity = 0 disables the cache entirely (isEnabled() returns false).
    explicit PageCache(int capacity);

    // ── Read side ─────────────────────────────────────────────────────────

    // Attempt a cache read for `lba`.
    //   Hit  → fills outData, promotes entry to MRU, returns true.
    //   Miss → returns false; caller must read from NAND, then call insert().
    bool read(int lba, std::string& outData);

    // Insert a clean page after a NAND read miss.
    //   If cache is full and the evicted entry is dirty → returns true and
    //   fills outEvicted (caller must flush outEvicted to NAND).
    bool insert(int lba, const std::string& data, Entry& outEvicted);

    // ── Write side ────────────────────────────────────────────────────────

    // Write `data` to the cache for `lba` (always marks dirty).
    //   Hit (LBA already in cache)  → coalesced update, no eviction, returns false.
    //   Miss (LBA not in cache)     → inserts dirty entry; if eviction needed and
    //                                  the evicted entry is dirty → returns true
    //                                  and fills outEvicted (caller flushes to NAND).
    bool write(int lba, const std::string& data, Entry& outEvicted);

    // ── Flush ─────────────────────────────────────────────────────────────

    // Drain ALL dirty entries from the cache (end-of-simulation or power-down).
    // Caller must write each returned entry to NAND.
    // Clean entries are silently discarded.
    std::vector<Entry> flush();

    // ── Queries ───────────────────────────────────────────────────────────

    bool isEnabled()  const { return capacity_ > 0; }
    int  getCapacity()const { return capacity_; }
    int  getSize()    const { return static_cast<int>(lruList_.size()); }

    int  getCacheHits()       const { return cacheHits_; }
    int  getCacheMisses()     const { return cacheMisses_; }
    int  getWritesCoalesced() const { return writesCoalesced_; }
    int  getDirtyEvictions()  const { return dirtyEvictions_; }

    // Hit ratio over all cache accesses (reads + writes)
    double getHitRatio() const;

    void printCacheStats() const;
};

#endif
