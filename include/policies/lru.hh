#ifndef LRU_POLICY_HH
#define LRU_POLICY_HH

#include "base_policy.hh"
#include <limits>

class LRUPolicy : public BasePolicy {
public:
    std::string name() const override { return "LRU"; }

    void on_hit(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)set_idx; (void)way_idx; (void)pw; (void)cycle;
        // Last access time is updated in CacheEntry directly
    }

    int select_victim_or_bypass(uint32_t set_idx, 
                                const std::vector<CacheEntry>& set_entries, 
                                const PredictionWindow& incoming_pw, 
                                uint64_t cycle) override {
        (void)set_idx; (void)incoming_pw; (void)cycle;
        // Find empty/invalid way first
        for (size_t w = 0; w < set_entries.size(); ++w) {
            if (!set_entries[w].valid) {
                return static_cast<int>(w);
            }
        }

        // Find oldest accessed way
        int lru_way = 0;
        uint64_t oldest_time = std::numeric_limits<uint64_t>::max();
        for (size_t w = 0; w < set_entries.size(); ++w) {
            if (set_entries[w].last_access_time < oldest_time) {
                oldest_time = set_entries[w].last_access_time;
                lru_way = static_cast<int>(w);
            }
        }
        return lru_way;
    }

    void on_insert(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)set_idx; (void)way_idx; (void)pw; (void)cycle;
    }

    void reset() override {}
};

#endif // LRU_POLICY_HH

