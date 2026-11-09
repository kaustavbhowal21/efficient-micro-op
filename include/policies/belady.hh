#ifndef BELADY_POLICY_HH
#define BELADY_POLICY_HH

#include "base_policy.hh"
#include <vector>
#include <unordered_map>
#include <deque>
#include <limits>

class BeladyPolicy : public BasePolicy {
private:
    const std::vector<PredictionWindow>* trace_ = nullptr;
    size_t current_idx_ = 0;
    // Map from start_pc to a queue of upcoming trace indices
    std::unordered_map<uint64_t, std::deque<size_t>> future_occurrences_;

public:
    BeladyPolicy() = default;

    std::string name() const override { return "Belady-MIN"; }

    void set_trace(const std::vector<PredictionWindow>& trace) {
        trace_ = &trace;
        future_occurrences_.clear();
        for (size_t i = 0; i < trace.size(); ++i) {
            future_occurrences_[trace[i].start_pc].push_back(i);
        }
        current_idx_ = 0;
    }

    void advance_trace(size_t idx) {
        current_idx_ = idx;
        if (trace_ && idx < trace_->size()) {
            uint64_t pc = (*trace_)[idx].start_pc;
            auto it = future_occurrences_.find(pc);
            if (it != future_occurrences_.end() && !it->second.empty() && it->second.front() == idx) {
                it->second.pop_front();
            }
        }
    }

    void on_hit(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)set_idx; (void)way_idx; (void)pw; (void)cycle;
    }

    int select_victim_or_bypass(uint32_t set_idx, 
                                const std::vector<CacheEntry>& set_entries, 
                                const PredictionWindow& incoming_pw, 
                                uint64_t cycle) override {
        (void)set_idx; (void)incoming_pw; (void)cycle;
        // Check for empty way
        for (size_t w = 0; w < set_entries.size(); ++w) {
            if (!set_entries[w].valid) {
                return static_cast<int>(w);
            }
        }

        // Find way with furthest next reuse
        size_t furthest_dist = 0;
        int victim_way = 0;

        for (size_t w = 0; w < set_entries.size(); ++w) {
            uint64_t pc = set_entries[w].start_pc;
            auto it = future_occurrences_.find(pc);
            if (it == future_occurrences_.end() || it->second.empty()) {
                // Never used again in future! Best candidate.
                return static_cast<int>(w);
            } else {
                size_t next_use = it->second.front();
                if (next_use > furthest_dist) {
                    furthest_dist = next_use;
                    victim_way = static_cast<int>(w);
                }
            }
        }
        return victim_way;
    }

    void on_insert(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) override {
        (void)set_idx; (void)way_idx; (void)pw; (void)cycle;
    }

    void reset() override {
        current_idx_ = 0;
    }
};

#endif // BELADY_POLICY_HH

