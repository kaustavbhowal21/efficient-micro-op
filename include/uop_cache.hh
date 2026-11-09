#ifndef UOP_CACHE_HH
#define UOP_CACHE_HH

#include "uop_types.hh"
#include "policies/base_policy.hh"
#include <vector>
#include <memory>
#include <deque>

struct PendingInsertion {
    uint32_t set_idx;
    PredictionWindow pw;
    uint64_t ready_cycle;
};

class MicroOpCache {
private:
    CacheConfig config_;
    std::vector<std::vector<CacheEntry>> sets_;
    std::shared_ptr<BasePolicy> policy_;
    SimulationStats stats_;
    uint64_t current_cycle_ = 0;

    // Asynchronous non-blocking accumulation buffer / decode queue
    std::deque<PendingInsertion> pending_insertions_;

public:
    MicroOpCache(const CacheConfig& config, std::shared_ptr<BasePolicy> policy);

    void set_policy(std::shared_ptr<BasePolicy> policy);
    std::shared_ptr<BasePolicy> get_policy() const { return policy_; }

    uint32_t get_set_index(uint64_t pc) const;
    uint64_t get_tag(uint64_t pc) const;

    // Access the cache for a prediction window
    LookupResult access(const PredictionWindow& pw);

    // Process completed insertions from the asynchronous decode pipeline
    void process_pending_insertions(uint64_t current_cycle);

    // Drain all remaining pending insertions at end of simulation
    void drain();

    void reset();

    const SimulationStats& get_stats() const { return stats_; }
};

#endif // UOP_CACHE_HH

