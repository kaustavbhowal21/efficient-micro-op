#ifndef BASE_POLICY_HH
#define BASE_POLICY_HH

#include "uop_types.hh"
#include <string>
#include <vector>

class BasePolicy {
public:
    virtual ~BasePolicy() = default;

    virtual std::string name() const = 0;

    // Called on hit in the cache
    virtual void on_hit(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) = 0;

    // Called on miss to determine victim or if insertion should be bypassed
    // Returns way_idx to evict (0 .. num_ways-1), or -1 if bypassed
    virtual int select_victim_or_bypass(uint32_t set_idx, 
                                        const std::vector<CacheEntry>& set_entries, 
                                        const PredictionWindow& incoming_pw, 
                                        uint64_t cycle) = 0;

    // Called when a line is inserted into set_idx, way_idx
    virtual void on_insert(uint32_t set_idx, uint32_t way_idx, const PredictionWindow& pw, uint64_t cycle) = 0;

    // Reset internal state for a new simulation run
    virtual void reset() = 0;
};

#endif // BASE_POLICY_HH

