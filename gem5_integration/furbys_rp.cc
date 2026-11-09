/**
 * @file
 * Implementation of FURBYS replacement policy for gem5.
 */

#include "mem/cache/replacement_policies/furbys_rp.hh"
#include "params/FURBYSRP.hh"
#include "sim/cur_tick.hh"

namespace gem5
{
namespace replacement_policy
{

FURBYSRP::FURBYSRP(const Params &p)
    : Base(p),
      bypassThresholdK(p.bypass_threshold_k),
      pitfallDepth(p.pitfall_depth),
      setPitfallSlots(1024) // Default capacity for typical set counts
{
}

void
FURBYSRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto data = std::static_pointer_cast<FURBYSReplData>(replacement_data);
    data->rrpv = 2; // Initialized to 2 on insertion as per HPCA 2025 paper
    data->lastTouchTick = curTick();
}

void
FURBYSRP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto data = std::static_pointer_cast<FURBYSReplData>(replacement_data);
    data->rrpv = 0; // Promoted to 0 on hit
    data->lastTouchTick = curTick();
}

ReplaceableEntry*
FURBYSRP::findSRRIPVictim(const ReplacementCandidates& candidates) const
{
    // Search candidates for RRPV == 3. If none, increment until found.
    while (true) {
        for (const auto& candidate : candidates) {
            auto data = std::static_pointer_cast<FURBYSReplData>(
                candidate->replacementData);
            if (data->rrpv == 3) {
                return candidate;
            }
        }
        for (const auto& candidate : candidates) {
            auto data = std::static_pointer_cast<FURBYSReplData>(
                candidate->replacementData);
            if (data->rrpv < 3) {
                data->rrpv++;
            }
        }
    }
}

ReplaceableEntry*
FURBYSRP::getVictim(const ReplacementCandidates& candidates) const
{
    assert(!candidates.empty());

    // 1. Min Module: scan all ways to find minimum weight
    uint8_t min_weight = 255;
    ReplaceableEntry* min_victim = candidates[0];
    uint8_t max_rrpv = 0;
    int min_way_idx = 0;

    for (size_t i = 0; i < candidates.size(); ++i) {
        auto data = std::static_pointer_cast<FURBYSReplData>(
            candidates[i]->replacementData);

        if (data->weight < min_weight) {
            min_weight = data->weight;
            min_victim = candidates[i];
            max_rrpv = data->rrpv;
            min_way_idx = static_cast<int>(i);
        } else if (data->weight == min_weight) {
            // Tie break with highest RRPV (most dead line)
            if (data->rrpv > max_rrpv) {
                min_victim = candidates[i];
                max_rrpv = data->rrpv;
                min_way_idx = static_cast<int>(i);
            }
        }
    }

    // 2. Local Pitfall Detector: detect repetitive evictions in target set
    uint32_t set_idx = candidates[0]->getSet();
    if (set_idx >= setPitfallSlots.size()) {
        setPitfallSlots.resize(set_idx + 1);
    }
    auto& p_slots = setPitfallSlots[set_idx];

    bool pitfall_triggered = false;
    if (p_slots.size() >= pitfallDepth && pitfallDepth > 0) {
        pitfall_triggered = true;
        for (int way : p_slots) {
            if (way != min_way_idx) {
                pitfall_triggered = false;
                break;
            }
        }
    }

    ReplaceableEntry* final_victim = min_victim;

    if (pitfall_triggered) {
        // Degrade to SRRIP max module
        final_victim = findSRRIPVictim(candidates);
        p_slots.clear();
    } else {
        // FURBYS selects min_victim
        p_slots.push_back(min_way_idx);
        if (p_slots.size() > pitfallDepth) {
            p_slots.pop_front();
        }
    }

    return final_victim;
}

std::shared_ptr<ReplacementData>
FURBYSRP::instantiateEntry()
{
    return std::make_shared<FURBYSReplData>();
}

} // namespace replacement_policy
} // namespace gem5

