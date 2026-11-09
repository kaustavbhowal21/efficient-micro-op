#ifndef FLACK_POLICY_HH
#define FLACK_POLICY_HH

#include "base_policy.hh"
#include <vector>
#include <unordered_map>
#include <deque>
#include <limits>

class FLACKPolicy : public BasePolicy {
private:
    const std::vector<PredictionWindow>* trace_ = nullptr;
    size_t current_idx_ = 0;
    // Map start_pc to a list of future occurrences (index, size, cost)
    struct FutureAccess {
        size_t idx;
        uint32_t size;
        uint32_t cost;
    };
    std::unordered_map<uint64_t, std::deque<FutureAccess>> future_occurrences_;

public:
    FLACKPolicy() = default;

    std::string name() const override { return "FLACK-Offline"; }

    void set_trace(const std::vector<PredictionWindow>& trace) {
        trace_ = &trace;
        future_occurrences_.clear();
        for (size_t i = 0; i < trace.size(); ++i) {
            future_occurrences_[trace[i].start_pc].push_back({i, trace[i].size, trace[i].cost});
        }
        current_idx_ = 0;
    }

    void advance_trace(size_t idx) {
        current_idx_ = idx;
        if (trace_ && idx < trace_->size()) {
            uint64_t pc = (*trace_)[idx].start_pc;
            auto it = future_occurrences_.find(pc);
            if (it != future_occurrences_.end() && !it->second.empty() && it->second.front().idx == idx) {
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
        (void)set_idx; (void)cycle;
        // Check for empty way
        for (size_t w = 0; w < set_entries.size(); ++w) {
            if (!set_entries[w].valid) {
                return static_cast<int>(w);
            }
        }

        // FLACK selects victim by weighing future reuse distance by cost/size:
        // Score = Distance * size / cost. Higher score = better candidate to evict.
        // A block with high micro-op density (cost/size) is retained longer.
        // If incoming PW has worse score than all entries in set, bypass it!
        double max_score = -1.0;
        int victim_way = 0;

        for (size_t w = 0; w < set_entries.size(); ++w) {
            uint64_t pc = set_entries[w].start_pc;
            auto it = future_occurrences_.find(pc);
            
            if (it == future_occurrences_.end() || it->second.empty()) {
                // Never referenced again -> infinite score, evict immediately
                return static_cast<int>(w);
            } else {
                const auto& next_acc = it->second.front();
                double dist = static_cast<double>(next_acc.idx > current_idx_ ? (next_acc.idx - current_idx_) : 1);
                
                // Check if this cached line can provide partial hit to future access
                if (set_entries[w].size >= next_acc.size) {
                    // Larger or equal: boosts retention value (lowers eviction score)
                    dist *= 0.8;
                }

                double cost_density = static_cast<double>(set_entries[w].cost) / 
                                      std::max(1u, set_entries[w].size);
                if (cost_density < 0.1) cost_density = 0.1;

                double score = dist / cost_density;
                if (score > max_score) {
                    max_score = score;
                    victim_way = static_cast<int>(w);
                }
            }
        }

        // Selective Bypassing in FLACK:
        // If incoming PW's next reuse is very far or has low cost density compared to victims
        auto in_it = future_occurrences_.find(incoming_pw.start_pc);
        if (in_it == future_occurrences_.end() || in_it->second.empty()) {
            // Incoming is never used again! Bypass it to avoid polluting cache
            return -1;
        } else {
            double in_dist = static_cast<double>(in_it->second.front().idx > current_idx_ ? 
                                                 (in_it->second.front().idx - current_idx_) : 1);
            double in_density = static_cast<double>(incoming_pw.cost) / std::max(1u, incoming_pw.size);
            double in_score = in_dist / in_density;
            if (in_score > max_score * 1.5) {
                // Incoming line is significantly worse than existing cache lines: bypass
                return -1;
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

#endif // FLACK_POLICY_HH

