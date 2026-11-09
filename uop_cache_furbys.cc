/**
 * uop_cache_furbys.cc: FURBYS & FLACK Micro-op Cache Replacement Engine
 * 
 * Course: CS2201 Computer Organization & Architecture
 * Paper:  'From Optimal to Practical: Efficient Micro-op Cache Replacement Policies
 *          for Data Center Applications' (HPCA 2025)
 * Group:  GN4 (Coding Assignment Track)
 * 
 * Implements:
 * 1. 8-way set-associative micro-op cache entry metadata (3-bit weight, 2-bit SRRIP)
 * 2. Min Module for finding minimum weight in target set
 * 3. Selective Bypassing (W_new < W_min - K, K=1)
 * 4. 2-slot Local Miss-Pitfall Detector with automatic fallback to 2-bit SRRIP
 */

#include <cstdint>
#include <vector>
#include <deque>
#include <iostream>
#include <algorithm>

namespace uop_cache_furbys {

constexpr uint32_t NUM_SETS = 64;
constexpr uint32_t NUM_WAYS = 8;
constexpr int32_t  BYPASS_THRESHOLD_K = 1;
constexpr uint32_t PITFALL_DEPTH = 2;

struct CacheEntry {
    bool valid = false;
    uint64_t start_pc = 0;
    uint32_t size = 1;       // Number of entries occupied
    uint32_t cost = 1;       // Number of valid micro-ops
    uint8_t weight_grp = 0;  // 3-bit Jenks weight group (0..7)
    uint8_t rrpv = 2;        // 2-bit SRRIP value (0..3)
    uint64_t last_access = 0;
};

struct CacheSet {
    CacheEntry ways[NUM_WAYS];
    int8_t pitfall_slot_0 = -1; // Local Pitfall Detector Slot 0
    int8_t pitfall_slot_1 = -1; // Local Pitfall Detector Slot 1
};

class FurbysEngine {
private:
    CacheSet sets[NUM_SETS];
    uint64_t cycle = 0;

    // Telemetry counters
    uint64_t stat_selective_bypasses = 0;
    uint64_t stat_pitfall_triggers = 0;
    uint64_t stat_srrip_evicts = 0;
    uint64_t stat_furbys_evicts = 0;

public:
    FurbysEngine() = default;

    int find_srrip_victim(CacheSet& set) {
        while (true) {
            for (uint32_t w = 0; w < NUM_WAYS; ++w) {
                if (set.ways[w].rrpv == 3) {
                    return static_cast<int>(w);
                }
            }
            for (uint32_t w = 0; w < NUM_WAYS; ++w) {
                if (set.ways[w].rrpv < 3) {
                    set.ways[w].rrpv++;
                }
            }
        }
    }

    // FURBYS Replacement & Local Pitfall Detector
    int select_victim_or_bypass(uint32_t set_idx, uint8_t decoded_weight_hint) {
        CacheSet& set = sets[set_idx];

        // 1. Look for empty way
        for (uint32_t w = 0; w < NUM_WAYS; ++w) {
            if (!set.ways[w].valid) {
                return static_cast<int>(w);
            }
        }

        // 2. Min Module: scan all 8 ways to locate minimum weight
        uint8_t min_grp = 255;
        uint32_t min_way = 0;
        uint8_t max_rrpv = 0;

        for (uint32_t w = 0; w < NUM_WAYS; ++w) {
            uint8_t wg = set.ways[w].weight_grp;
            uint8_t r = set.ways[w].rrpv;
            if (wg < min_grp) {
                min_grp = wg;
                min_way = w;
                max_rrpv = r;
            } else if (wg == min_grp) {
                if (r > max_rrpv) {
                    min_way = w;
                    max_rrpv = r;
                }
            }
        }

        // 3. Selective Bypassing check
        if (static_cast<int32_t>(decoded_weight_hint) < static_cast<int32_t>(min_grp) - BYPASS_THRESHOLD_K) {
            stat_selective_bypasses++;
            return -1; // Signal Selective Bypass
        }

        // 4. Local Pitfall Detector: check if same way evicted twice consecutively
        bool pitfall_triggered = (set.pitfall_slot_0 == static_cast<int8_t>(min_way) &&
                                  set.pitfall_slot_1 == static_cast<int8_t>(min_way));

        int final_victim = static_cast<int>(min_way);

        if (pitfall_triggered) {
            stat_pitfall_triggers++;
            stat_srrip_evicts++;
            final_victim = find_srrip_victim(set); // Degrade to SRRIP to evict stale block
            set.pitfall_slot_0 = -1;
            set.pitfall_slot_1 = -1;
        } else {
            stat_furbys_evicts++;
            set.pitfall_slot_0 = set.pitfall_slot_1;
            set.pitfall_slot_1 = static_cast<int8_t>(min_way);
        }

        return final_victim;
    }

    void on_hit(uint32_t set_idx, uint32_t way_idx) {
        sets[set_idx].ways[way_idx].rrpv = 0; // Promoted to 0 on hit
        sets[set_idx].ways[way_idx].last_access = ++cycle;
    }

    void on_insert(uint32_t set_idx, uint32_t way_idx, uint64_t pc, uint32_t size, uint32_t cost, uint8_t weight) {
        CacheEntry& entry = sets[set_idx].ways[way_idx];
        entry.valid = true;
        entry.start_pc = pc;
        entry.size = size;
        entry.cost = cost;
        entry.weight_grp = weight;
        entry.rrpv = 2; // Initialized to 2 on insert
        entry.last_access = ++cycle;
    }

    uint64_t get_bypasses() const { return stat_selective_bypasses; }
    uint64_t get_pitfall_triggers() const { return stat_pitfall_triggers; }
};

} // namespace uop_cache_furbys

