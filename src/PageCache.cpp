#include "PageCache.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

// ─── Constructor ──────────────────────────────────────────────────────────────

PageCache::PageCache(int capacity)
    : capacity_(capacity),
      cacheHits_(0),
      cacheMisses_(0),
      writesCoalesced_(0),
      dirtyEvictions_(0)
{}

// ─── Private: evictIfFull ─────────────────────────────────────────────────────
//
// Evicts the LRU (back-of-list) entry when the cache is at capacity.
// Returns true if the evicted entry was dirty — caller must flush it to NAND.
// Returns false for a clean eviction (no NAND action needed).
// Returns false immediately if cache still has space.

bool PageCache::evictIfFull(Entry& outEvicted) {

    if ((int)lruList_.size() < capacity_) {
        return false;   // still room — nothing evicted
    }

    Entry& lru = lruList_.back();

    if (lru.dirty) {
        outEvicted = lru;   // caller must flush to NAND
        dirtyEvictions_++;
        index_.erase(lru.lba);
        lruList_.pop_back();
        return true;
    }

    // Clean entry — just discard; no NAND flush needed
    index_.erase(lru.lba);
    lruList_.pop_back();
    return false;
}

// ─── Read ─────────────────────────────────────────────────────────────────────

bool PageCache::read(int lba, std::string& outData) {

    auto it = index_.find(lba);

    if (it == index_.end()) {
        cacheMisses_++;
        return false;   // cache miss — caller reads from NAND
    }

    // Cache hit: promote to MRU (front)
    lruList_.splice(lruList_.begin(), lruList_, it->second);
    outData = it->second->data;
    cacheHits_++;
    return true;
}

// ─── Insert (after NAND read miss) ───────────────────────────────────────────

bool PageCache::insert(int lba, const std::string& data, Entry& outEvicted) {

    // Guard: do not double-insert (should not happen in practice)
    if (index_.count(lba)) {
        // LBA is already in cache (race guard) — just promote & update
        lruList_.splice(lruList_.begin(), lruList_, index_[lba]);
        index_[lba]->data = data;
        return false;
    }

    bool evicted = evictIfFull(outEvicted);

    // Insert as clean (this data just came from authoritative NAND)
    lruList_.push_front({lba, data, /*dirty=*/false});
    index_[lba] = lruList_.begin();

    return evicted;
}

// ─── Write ────────────────────────────────────────────────────────────────────

bool PageCache::write(int lba, const std::string& data, Entry& outEvicted) {

    auto it = index_.find(lba);

    if (it != index_.end()) {
        // ── Write hit: coalesced update ───────────────────────────────────
        // LBA is already in cache — update data in-place, mark dirty, promote.
        // The previous physical page (if any) will be invalidated when this
        // dirty entry is eventually evicted and flushed to NAND.
        it->second->data  = data;
        it->second->dirty = true;
        lruList_.splice(lruList_.begin(), lruList_, it->second);
        writesCoalesced_++;
        cacheHits_++;
        return false;   // no eviction — no NAND action needed
    }

    // ── Write miss: insert new dirty entry ───────────────────────────────
    bool evicted = evictIfFull(outEvicted);

    lruList_.push_front({lba, data, /*dirty=*/true});
    index_[lba] = lruList_.begin();
    cacheMisses_++;

    return evicted;   // true only when a dirty entry was evicted
}

// ─── Flush ────────────────────────────────────────────────────────────────────

std::vector<PageCache::Entry> PageCache::flush() {

    std::vector<Entry> dirty;

    for (auto& entry : lruList_) {
        if (entry.dirty) {
            dirty.push_back(entry);
        }
    }

    // Clear everything — both dirty and clean
    lruList_.clear();
    index_.clear();

    return dirty;
}

// ─── getHitRatio ─────────────────────────────────────────────────────────────

double PageCache::getHitRatio() const {
    int total = cacheHits_ + cacheMisses_;
    if (total == 0) return 0.0;
    return static_cast<double>(cacheHits_) / total * 100.0;
}

// ─── printCacheStats ─────────────────────────────────────────────────────────

void PageCache::printCacheStats() const {

    std::cout << "\n  [DRAM Page Cache — LRU, capacity=" << capacity_ << "]\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Cache Hits              : " << cacheHits_       << "\n";
    std::cout << "  Cache Misses            : " << cacheMisses_     << "\n";
    std::cout << "  Hit Ratio               : " << getHitRatio()    << "%\n";
    std::cout << "  Writes Coalesced        : " << writesCoalesced_
              << "   (NAND writes avoided)\n";
    std::cout << "  Dirty Evictions         : " << dirtyEvictions_
              << "   (NAND writes triggered by eviction)\n";
}
