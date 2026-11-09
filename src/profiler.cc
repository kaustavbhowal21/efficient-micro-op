#include "profiler.hh"
#include "uop_cache.hh"
#include "policies/flack.hh"
#include "jenks.hh"
#include <unordered_map>
#include <iostream>

std::vector<PredictionWindow> Profiler::profile_and_tag_trace(
    const std::vector<PredictionWindow>& raw_trace,
    const CacheConfig& config) {

    if (raw_trace.empty()) return {};

    // 1. Run FLACK simulation on the raw trace to record micro-op level hit rates
    auto flack_policy = std::make_shared<FLACKPolicy>();
    flack_policy->set_trace(raw_trace);

    MicroOpCache flack_sim(config, flack_policy);

    // Track per-PC: {uops_hit, uops_total}
    std::unordered_map<uint64_t, std::pair<uint64_t, uint64_t>> pc_stats;

    for (size_t i = 0; i < raw_trace.size(); ++i) {
        flack_policy->advance_trace(i);
        LookupResult res = flack_sim.access(raw_trace[i]);
        pc_stats[raw_trace[i].start_pc].first += res.uops_served;
        pc_stats[raw_trace[i].start_pc].second += raw_trace[i].cost;
    }
    flack_sim.drain();

    // 2. Group PCs by cache set
    size_t num_classes = 1 << config.num_weight_bits; // e.g. 2^3 = 8
    std::vector<std::vector<uint64_t>> set_pcs(config.num_sets);
    std::vector<std::vector<double>> set_hit_rates(config.num_sets);

    for (const auto& pair : pc_stats) {
        uint64_t pc = pair.first;
        uint32_t set_idx = static_cast<uint32_t>((pc >> 6) % config.num_sets);
        double hr = pair.second.second > 0 ? 
            (static_cast<double>(pair.second.first) / pair.second.second) : 0.0;
        
        set_pcs[set_idx].push_back(pc);
        set_hit_rates[set_idx].push_back(hr);
    }

    // 3. For each set, run Jenks Natural Breaks clustering
    std::unordered_map<uint64_t, uint8_t> pc_to_weight;

    for (uint32_t s = 0; s < config.num_sets; ++s) {
        if (set_hit_rates[s].empty()) continue;

        std::vector<double> breaks = JenksNaturalBreaks::compute_breaks(set_hit_rates[s], num_classes);

        for (size_t i = 0; i < set_pcs[s].size(); ++i) {
            uint64_t pc = set_pcs[s][i];
            double hr = set_hit_rates[s][i];
            uint8_t w = JenksNaturalBreaks::get_class(hr, breaks);
            pc_to_weight[pc] = w;
        }
    }

    // 4. Tag the trace with 3-bit hints
    std::vector<PredictionWindow> tagged_trace = raw_trace;
    for (auto& pw : tagged_trace) {
        auto it = pc_to_weight.find(pw.start_pc);
        if (it != pc_to_weight.end()) {
            pw.weight_hint = it->second;
        } else {
            pw.weight_hint = 0;
        }
    }

    return tagged_trace;
}

std::vector<PredictionWindow> Profiler::profile_cross_input(
    const std::vector<PredictionWindow>& train_trace,
    const std::vector<PredictionWindow>& test_trace,
    const CacheConfig& config) {

    if (train_trace.empty() || test_trace.empty()) return test_trace;

    // 1. Profile on train_trace
    auto flack_policy = std::make_shared<FLACKPolicy>();
    flack_policy->set_trace(train_trace);
    MicroOpCache flack_sim(config, flack_policy);

    std::unordered_map<uint64_t, std::pair<uint64_t, uint64_t>> pc_stats;
    for (size_t i = 0; i < train_trace.size(); ++i) {
        flack_policy->advance_trace(i);
        LookupResult res = flack_sim.access(train_trace[i]);
        pc_stats[train_trace[i].start_pc].first += res.uops_served;
        pc_stats[train_trace[i].start_pc].second += train_trace[i].cost;
    }
    flack_sim.drain();

    size_t num_classes = 1 << config.num_weight_bits;
    std::vector<std::vector<uint64_t>> set_pcs(config.num_sets);
    std::vector<std::vector<double>> set_hit_rates(config.num_sets);

    for (const auto& pair : pc_stats) {
        uint64_t pc = pair.first;
        uint32_t set_idx = static_cast<uint32_t>((pc >> 6) % config.num_sets);
        double hr = pair.second.second > 0 ? 
            (static_cast<double>(pair.second.first) / pair.second.second) : 0.0;
        set_pcs[set_idx].push_back(pc);
        set_hit_rates[set_idx].push_back(hr);
    }

    std::unordered_map<uint64_t, uint8_t> pc_to_weight;
    for (uint32_t s = 0; s < config.num_sets; ++s) {
        if (set_hit_rates[s].empty()) continue;
        std::vector<double> breaks = JenksNaturalBreaks::compute_breaks(set_hit_rates[s], num_classes);
        for (size_t i = 0; i < set_pcs[s].size(); ++i) {
            uint64_t pc = set_pcs[s][i];
            double hr = set_hit_rates[s][i];
            pc_to_weight[pc] = JenksNaturalBreaks::get_class(hr, breaks);
        }
    }

    // 2. Apply learned weights to test_trace
    std::vector<PredictionWindow> tagged_test = test_trace;
    for (auto& pw : tagged_test) {
        auto it = pc_to_weight.find(pw.start_pc);
        if (it != pc_to_weight.end()) {
            pw.weight_hint = it->second;
        } else {
            // Unseen PC in training: assign default medium/low weight (e.g. 1)
            pw.weight_hint = 1;
        }
    }

    return tagged_test;
}

