/**
 * @file
 * Declaration of FURBYS (FLACK-based groUping-by-hit-Rate BYpassing-coldness 
 * detecting-miSses) replacement policy for gem5, modeled from HPCA 2025.
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_FURBYS_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_FURBYS_RP_HH__

#include <vector>
#include <deque>
#include "mem/cache/replacement_policies/base.hh"

namespace gem5
{

struct FURBYSRPParams;

namespace replacement_policy
{

class FURBYSRP : public Base
{
  protected:
    /** Sub-class for FURBYS-specific entry metadata. */
    struct FURBYSReplData : public ReplacementData
    {
        /** 3-bit profiled weight (0..7) computed from Jenks Natural Breaks */
        uint8_t weight;

        /** 2-bit SRRIP Re-reference Prediction Value (0..3) */
        uint8_t rrpv;

        /** Tick for LRU tie breaking */
        Tick lastTouchTick;

        FURBYSReplData(uint8_t w = 0, uint8_t r = 2)
            : weight(w), rrpv(r), lastTouchTick(0) {}
    };

    /** Hyperparameter K: bypass when incoming_weight < min_weight - K */
    const int32_t bypassThresholdK;

    /** Depth of local miss-pitfall detector (default: 2) */
    const unsigned pitfallDepth;

    /** Map or table of per-set pitfall detector slots */
    mutable std::vector<std::deque<int>> setPitfallSlots;

    /** Find SRRIP victim candidate (max RRPV = 3) */
    ReplaceableEntry* findSRRIPVictim(const ReplacementCandidates& candidates) const;

  public:
    typedef FURBYSRPParams Params;
    FURBYSRP(const Params &p);
    ~FURBYSRP() = default;

    /**
     * Invalidate and reset replacement data for a line (e.g. on insertion).
     * Initializes RRPV = 2 as per paper.
     */
    void reset(const std::shared_ptr<ReplacementData>& replacement_data) const override;

    /**
     * Touch an entry on a cache hit. Promotes RRPV to 0.
     */
    void touch(const std::shared_ptr<ReplacementData>& replacement_data) const override;

    /**
     * Find replacement victim using the Min Module and Local Pitfall Detector.
     */
    ReplaceableEntry* getVictim(const ReplacementCandidates& candidates) const override;

    /**
     * Instantiate entry-specific replacement data.
     */
    std::shared_ptr<ReplacementData> instantiateEntry() override;
};

} // namespace replacement_policy
} // namespace gem5

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_FURBYS_RP_HH__

