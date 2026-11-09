#ifndef FURBYS_POLICY_HH
#define FURBYS_POLICY_HH

#include "base_policy.hh"
#include <vector>
#include <deque>
#include <iostream>

class FURBYSPolicy : public BasePolicy {
private:
    uint32_t num_sets_;
    uint32_t num_ways_;
    int32_t bypass_threshold_k_;
    uint32_t pitfall_depth_;

    // Per-way metadata
    std::vector<std::vector<uint8_t>> weights_;  // 3-bit weight (0..7)
    std::vector<std::vector<uint8_t>> rrpv_;     // 2-bit RRPV (0..3)

    // Per-set local miss-pitfall detector
    // Defaults to depth 2: pitfall_slots_[set_idx] stores up to pitfall_depth_ recent evictions
    std::vector<std::deque<int8_t>> pitfall_slots_;

    // Telemetry stats
    uint64_t bypass_count_ = 0;
    uint64_t pitfall_triggers_ = 0;
    uint64_t srrip_evictions_ = 0;
    uint64_t furbys_evictions_ = 0;

public:
    FURBYSPolicy(uint32_t sets = 64, 
                 uint32_t ways = 8, 
                 int32_t k = 1, 
                 uint32_t pitfall_depth = 2)
        : num_sets_(sets), 
          num_ways_(ways), 
          bypass_threshold_k_(k), 
          pitfall_depth_(pitfall_depth),
          weights_(sets, std::vector<uint8_t>(ways, 0)),
          rrpv_(sets, std::vector<uint8_t>(ways, 2)),
          pitfall_slots_(sets) {}

    std::string name() const override { return "FURBYS-Online"; }

    uint64_t get_bypass_count() const { return bypass_count_; }
    uint64_t get_pitfall_triggers() const { return pitfall_triggers_; }

    void on_hit(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)pw; (void)cycle;
        // Hit promotion: set RRPV to 0 (immediate re-reference)
        rrpv_[set_idx][way_idx] = 0;
    }

    int select_victim_or_bypass(uint32_t set_idx, 
                                const std::vector<CacheEntry>& set_entries, 
                                const PredictionWindow& incoming_pw, 
                                uint64_t cycle) override {
        (void)cycle;
        // 1. If there is an invalid line, fill it first without eviction
        for (size_t w = 0; w < num_ways_; ++w) {
            if (!set_entries[w].valid) {
                return static_cast<int>(w);
            }
        }

        // 2. Step 2 in paper (Figure 7b): Min Module
        // Find minimum weight in the set. Break ties using highest RRPV (or oldest entry).
        uint8_t min_grp = 255;
        int min_way = -1;
        uint8_t max_rrpv_for_min = 0;

        for (size_t w = 0; w < num_ways_; ++w) {
            uint8_t w_val = weights_[set_idx][w];
            uint8_t r_val = rrpv_[set_idx][w];

            if (w_val < min_grp) {
                min_grp = w_val;
                min_way = static_cast<int>(w);
                max_rrpv_for_min = r_val;
            } else if (w_val == min_grp) {
                // Tie breaker: choose the line with larger RRPV (more dead)
                if (r_val > max_rrpv_for_min) {
                    min_way = static_cast<int>(w);
                    max_rrpv_for_min = r_val;
                }
            }
        }

        // 3. Step 3 in paper: Compare groups for Selective Bypassing
        // If W_new < W_min - K, bypass insertion!
        int32_t incoming_weight = static_cast<int32_t>(incoming_pw.weight_hint);
        int32_t threshold = static_cast<int32_t>(min_grp) - bypass_threshold_k_;
        if (incoming_weight < threshold) {
            bypass_count_++;
            return -1; // Bypass signal
        }

        // 4. Step 3 & 4 in paper: Local Miss-Pitfall Detector
        // Check if the same way has been repeatedly evicted
        auto& p_slots = pitfall_slots_[set_idx];
        bool pitfall_triggered = false;

        if (p_slots.size() >= pitfall_depth_ && pitfall_depth_ > 0) {
            pitfall_triggered = true;
            for (size_t i = 0; i < p_slots.size(); ++i) {
                if (p_slots[i] != min_way) {
                    pitfall_triggered = false;
                    break;
                }
            }
        }

        int final_victim = min_way;

        if (pitfall_triggered) {
            // Degrade to SRRIP max module to evict stale high-weight line
            pitfall_triggers_++;
            srrip_evictions_++;
            final_victim = find_srrip_victim(set_idx);
            p_slots.clear(); // Reset pitfall detector
        } else {
            // FURBYS selects min_way
            furbys_evictions_++;
            // Record this eviction in pitfall detector
            p_slots.push_back(static_cast<int8_t>(min_way));
            if (p_slots.size() > pitfall_depth_) {
                p_slots.pop_front();
            }
        }

        return final_victim;
    }

    void on_insert(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)cycle;
        weights_[set_idx][way_idx] = pw.weight_hint;
        rrpv_[set_idx][way_idx] = 2; // Initialized to 2 on insert as per paper
    }

    void reset() override {
        for (auto& set : weights_) {
            std::fill(set.begin(), set.end(), 0);
        }
        for (auto& set : rrpv_) {
            std::fill(set.begin(), set.end(), 2);
        }
        for (auto& p : pitfall_slots_) {
            p.clear();
        }
        bypass_count_ = 0;
        pitfall_triggers_ = 0;
        srrip_evictions_ = 0;
        furbys_evictions_ = 0;
    }

private:
    int find_srrip_victim(uint32_t set_idx) {
        // Standard SRRIP: search for RRPV == 3. If none, increment until found.
        while (true) {
            for (size_t w = 0; w < num_ways_; ++w) {
                if (rrpv_[set_idx][w] == 3) {
                    return static_cast<int>(w);
                }
            }
            for (size_t w = 0; w < num_ways_; ++w) {
                if (rrpv_[set_idx][w] < 3) {
                    rrpv_[set_idx][w]++;
                }
            }
        }
    }
};

#endif // FURBYS_POLICY_HH

