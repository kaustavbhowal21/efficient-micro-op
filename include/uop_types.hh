#ifndef UOP_TYPES_HH
#define UOP_TYPES_HH

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

struct PredictionWindow {
    uint64_t start_pc = 0;
    uint32_t size = 1;       // Number of entries occupied (e.g. 1, 2)
    uint32_t cost = 1;       // Number of valid micro-ops (e.g. 1 to 8*size)
    uint64_t end_pc = 0;     // Boundary/branch PC
    uint8_t weight_hint = 0; // Profiled weight (e.g. 0..7 for 3-bit hints)

    PredictionWindow() = default;
    PredictionWindow(uint64_t pc, uint32_t s, uint32_t c, uint64_t end = 0, uint8_t w = 0)
        : start_pc(pc), size(s), cost(c), end_pc(end), weight_hint(w) {}
};

enum class LookupStatus {
    MISS,
    FULL_HIT,
    PARTIAL_HIT
};

struct LookupResult {
    LookupStatus status = LookupStatus::MISS;
    uint32_t uops_served = 0;
    uint32_t uops_missed = 0;
    int way = -1;
};

struct CacheEntry {
    bool valid = false;
    uint64_t start_pc = 0;
    uint32_t size = 1;
    uint32_t cost = 1;
    uint8_t weight = 0;
    uint8_t rrpv = 2; // Default for SRRIP insertion: RRPV = 2 (distant re-reference)
    uint64_t last_access_time = 0;
    uint64_t insertion_time = 0;
};

struct CacheConfig {
    uint32_t num_sets = 64;            // 64 sets
    uint32_t num_ways = 8;             // 8 ways (512 total entries, AMD Zen3)
    uint32_t max_uops_per_entry = 8;   // 8 uops per entry
    int32_t bypass_threshold_k = 1;    // K = 1 from paper
    uint32_t pitfall_depth = 2;        // 2 slots per set
    uint32_t num_weight_bits = 3;      // 3 bits -> 8 groups (0..7)
    uint32_t decode_latency = 5;       // Asynchronous decode latency in cycles
};

struct SimulationStats {
    std::string policy_name;
    uint64_t pw_lookups = 0;
    uint64_t pw_full_hits = 0;
    uint64_t pw_partial_hits = 0;
    uint64_t pw_misses = 0;
    uint64_t uop_lookups = 0;
    uint64_t uop_hits = 0;
    uint64_t uop_misses = 0;
    uint64_t selective_bypasses = 0;
    uint64_t pitfall_triggers = 0;
    uint64_t total_evictions = 0;

    double uop_miss_rate() const {
        return uop_lookups > 0 ? (100.0 * uop_misses / uop_lookups) : 0.0;
    }

    double pw_miss_rate() const {
        return pw_lookups > 0 ? (100.0 * pw_misses / pw_lookups) : 0.0;
    }

    void print() const {
        std::cout << "========================================================\n";
        std::cout << " Policy: " << policy_name << "\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << " Total PW Lookups:       " << pw_lookups << "\n";
        std::cout << " PW Full Hits:           " << pw_full_hits << "\n";
        std::cout << " PW Partial Hits:        " << pw_partial_hits << "\n";
        std::cout << " PW Misses:              " << pw_misses << " (" << pw_miss_rate() << "%)\n";
        std::cout << " Total Micro-ops:        " << uop_lookups << "\n";
        std::cout << " Micro-ops Hit:          " << uop_hits << "\n";
        std::cout << " Micro-ops Missed:       " << uop_misses << " (" << uop_miss_rate() << "%)\n";
        std::cout << " Selective Bypasses:     " << selective_bypasses << "\n";
        std::cout << " Pitfall SRRIP Triggers: " << pitfall_triggers << "\n";
        std::cout << " Total Evictions:        " << total_evictions << "\n";
        std::cout << "========================================================\n";
    }
};

#endif // UOP_TYPES_HH

