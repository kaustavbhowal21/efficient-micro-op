#ifndef SRRIP_POLICY_HH
#define SRRIP_POLICY_HH

#include "base_policy.hh"
#include <vector>

class SRRIPPolicy : public BasePolicy {
private:
    uint32_t num_sets_;
    uint32_t num_ways_;
    std::vector<std::vector<uint8_t>> rrpv_;

public:
    SRRIPPolicy(uint32_t sets = 64, uint32_t ways = 8)
        : num_sets_(sets), num_ways_(ways),
          rrpv_(sets, std::vector<uint8_t>(ways, 3)) {}

    std::string name() const override { return "SRRIP-2bit"; }

    void on_hit(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)pw; (void)cycle;
        rrpv_[set_idx][way_idx] = 0; // Immediate re-reference
    }

    int select_victim_or_bypass(uint32_t set_idx, 
                                const std::vector<CacheEntry>& set_entries, 
                                const PredictionWindow& incoming_pw, 
                                uint64_t cycle) override {
        (void)incoming_pw; (void)cycle;
        for (size_t w = 0; w < num_ways_; ++w) {
            if (!set_entries[w].valid) {
                return static_cast<int>(w);
            }
        }

        // Search for RRPV == 3
        while (true) {
            for (size_t w = 0; w < num_ways_; ++w) {
                if (rrpv_[set_idx][w] == 3) {
                    return static_cast<int>(w);
                }
            }
            // Increment all RRPVs
            for (size_t w = 0; w < num_ways_; ++w) {
                if (rrpv_[set_idx][w] < 3) {
                    rrpv_[set_idx][w]++;
                }
            }
        }
    }

    void on_insert(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)pw; (void)cycle;
        rrpv_[set_idx][way_idx] = 2; // Long re-reference interval
    }

    void reset() override {
        for (auto& set : rrpv_) {
            std::fill(set.begin(), set.end(), 3);
        }
    }
};

#endif // SRRIP_POLICY_HH

