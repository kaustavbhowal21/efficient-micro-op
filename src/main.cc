#include "uop_types.hh"
#include "uop_cache.hh"
#include "profiler.hh"
#include "trace_loader.hh"
#include "policies/lru.hh"
#include "policies/srrip.hh"
#include "policies/belady.hh"
#include "policies/flack.hh"
#include "policies/furbys.hh"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

void run_policy_simulation(
    const std::string& name,
    std::shared_ptr<BasePolicy> policy,
    const std::vector<PredictionWindow>& trace,
    const CacheConfig& config,
    SimulationStats& out_stats) {

    MicroOpCache cache(config, policy);

    // If policy is Belady or FLACK, provide trace
    auto belady = std::dynamic_pointer_cast<BeladyPolicy>(policy);
    auto flack = std::dynamic_pointer_cast<FLACKPolicy>(policy);

    if (belady) belady->set_trace(trace);
    if (flack) flack->set_trace(trace);

    for (size_t i = 0; i < trace.size(); ++i) {
        if (belady) belady->advance_trace(i);
        if (flack) flack->advance_trace(i);
        cache.access(trace[i]);
    }
    cache.drain();

    out_stats = cache.get_stats();
    out_stats.policy_name = name;

    auto furbys = std::dynamic_pointer_cast<FURBYSPolicy>(policy);
    if (furbys) {
        out_stats.selective_bypasses = furbys->get_bypass_count();
        out_stats.pitfall_triggers = furbys->get_pitfall_triggers();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=========================================================================\n";
    std::cout << "  EFFICIENT MICRO-OP CACHE REPLACEMENT POLICIES SIMULATOR (HPCA 2025)\n";
    std::cout << "  Modeling: AMD Zen3 (512 entries, 8-way set-associative, 64 sets)\n";
    std::cout << "=========================================================================\n\n";

    std::string workload = "Clang";
    size_t num_accesses = 100000;
    std::string trace_file = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--workload" && i + 1 < argc) {
            workload = argv[++i];
        } else if (arg == "--accesses" && i + 1 < argc) {
            num_accesses = std::stoul(argv[++i]);
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_file = argv[++i];
        }
    }

    CacheConfig config;
    std::vector<PredictionWindow> raw_trace;

    if (!trace_file.empty()) {
        std::cout << "[*] Loading trace from file: " << trace_file << "...\n";
        raw_trace = TraceLoader::load_from_file(trace_file);
    }

    if (raw_trace.empty()) {
        std::cout << "[*] Generating realistic data center benchmark trace (" << workload 
                  << ", " << num_accesses << " accesses)...\n";
        raw_trace = TraceLoader::generate_benchmark_trace(workload, num_accesses, 42);
    }

    std::cout << "[*] Total accesses in trace: " << raw_trace.size() << "\n\n";

    // 1. Run Baseline LRU
    SimulationStats stats_lru;
    auto lru_policy = std::make_shared<LRUPolicy>();
    run_policy_simulation("LRU (Baseline)", lru_policy, raw_trace, config, stats_lru);

    // 2. Run Online SRRIP (2-bit)
    SimulationStats stats_srrip;
    auto srrip_policy = std::make_shared<SRRIPPolicy>(config.num_sets, config.num_ways);
    run_policy_simulation("SRRIP (2-bit)", srrip_policy, raw_trace, config, stats_srrip);

    // 3. Run Belady's MIN (Lookahead oracle)
    SimulationStats stats_belady;
    auto belady_policy = std::make_shared<BeladyPolicy>();
    run_policy_simulation("Belady-MIN", belady_policy, raw_trace, config, stats_belady);

    // 4. Run FLACK (Offline near-optimal)
    SimulationStats stats_flack;
    auto flack_policy = std::make_shared<FLACKPolicy>();
    run_policy_simulation("FLACK (Offline)", flack_policy, raw_trace, config, stats_flack);

    // 5. Generate Profile & Run FURBYS (Same-Input Profile)
    std::cout << "[*] Running FURBYS 7-step profiling pipeline (FLACK + Jenks 3-bit clustering)...\n";
    std::vector<PredictionWindow> furbys_tagged_trace = 
        Profiler::profile_and_tag_trace(raw_trace, config);

    SimulationStats stats_furbys;
    auto furbys_policy = std::make_shared<FURBYSPolicy>(
        config.num_sets, config.num_ways, config.bypass_threshold_k, config.pitfall_depth);
    run_policy_simulation("FURBYS (Same-Input)", furbys_policy, furbys_tagged_trace, config, stats_furbys);

    // 6. Cross-Validation: Train on Seed 42, Test on Seed 99
    auto cross_train_trace = raw_trace;
    auto cross_test_raw = TraceLoader::generate_benchmark_trace(workload, num_accesses, 99);
    auto cross_test_tagged = Profiler::profile_cross_input(cross_train_trace, cross_test_raw, config);

    SimulationStats stats_cross_lru, stats_cross_furbys;
    run_policy_simulation("LRU (Cross-Test)", std::make_shared<LRUPolicy>(), cross_test_raw, config, stats_cross_lru);
    auto cross_furbys_policy = std::make_shared<FURBYSPolicy>(
        config.num_sets, config.num_ways, config.bypass_threshold_k, config.pitfall_depth);
    run_policy_simulation("FURBYS (Cross-Input)", cross_furbys_policy, cross_test_tagged, config, stats_cross_furbys);

    // Print Comparative Results Table
    std::cout << "\n";
    std::cout << "+-----------------------+-------------+-------------+------------------+-----------------------------+\n";
    std::cout << "| Policy                | Uop Misses  | Miss Rate   | Miss Red. vs LRU | Key Telemetry / Behavior    |\n";
    std::cout << "+-----------------------+-------------+-------------+------------------+-----------------------------+\n";

    auto print_row = [&](const std::string& name, const SimulationStats& st, const SimulationStats& base, const std::string& telem) {
        double red = base.uop_misses > 0 ? 
            (100.0 * (static_cast<double>(base.uop_misses) - st.uop_misses) / base.uop_misses) : 0.0;
        std::cout << "| " << std::left << std::setw(21) << name
                  << " | " << std::right << std::setw(11) << st.uop_misses
                  << " | " << std::fixed << std::setprecision(2) << std::setw(9) << st.uop_miss_rate() << "%"
                  << " | " << std::setw(14) << (red >= 0 ? "+" : "") << red << "%"
                  << " | " << std::left << std::setw(27) << telem << " |\n";
    };

    print_row("LRU (Baseline)", stats_lru, stats_lru, "Baseline cache");
    print_row("SRRIP (2-bit RRPV)", stats_srrip, stats_lru, "Ignores uop cost & size");
    print_row("Belady-MIN (Oracle)", stats_belady, stats_lru, "Object-optimal only");
    print_row("FLACK (Near-Optimal)", stats_flack, stats_lru, "Cost/size + partial-hit");
    
    std::string furbys_telem = std::to_string(stats_furbys.selective_bypasses) + " Bypasses, " + 
                               std::to_string(stats_furbys.pitfall_triggers) + " Pitfalls";
    print_row("FURBYS (Same-Input)", stats_furbys, stats_lru, furbys_telem);

    std::string cross_telem = std::to_string(stats_cross_furbys.selective_bypasses) + " Bypasses, " + 
                              std::to_string(stats_cross_furbys.pitfall_triggers) + " Pitfalls";
    print_row("FURBYS (Cross-Input)", stats_cross_furbys, stats_cross_lru, cross_telem);

    std::cout << "+-----------------------+-------------+-------------+------------------+-----------------------------+\n\n";

    // Write Summary to CSV in results/
    std::ofstream csv("results/main_evaluation.csv");
    csv << "Policy,UopLookups,UopHits,UopMisses,UopMissRate,MissReductionVsLRU,SelectiveBypasses,PitfallTriggers\n";
    auto write_csv = [&](const SimulationStats& st, const SimulationStats& base) {
        double red = base.uop_misses > 0 ? 
            (100.0 * (static_cast<double>(base.uop_misses) - st.uop_misses) / base.uop_misses) : 0.0;
        csv << st.policy_name << ","
            << st.uop_lookups << ","
            << st.uop_hits << ","
            << st.uop_misses << ","
            << st.uop_miss_rate() << ","
            << red << ","
            << st.selective_bypasses << ","
            << st.pitfall_triggers << "\n";
    };
    write_csv(stats_lru, stats_lru);
    write_csv(stats_srrip, stats_lru);
    write_csv(stats_belady, stats_lru);
    write_csv(stats_flack, stats_lru);
    write_csv(stats_furbys, stats_lru);
    write_csv(stats_cross_furbys, stats_cross_lru);
    csv.close();

    std::cout << "[+] Main evaluation exported to results/main_evaluation.csv\n";

    // Run Sensitivity Sweeps (Figures 19, 20, 21 from Paper)
    std::cout << "\n[*] Running Sensitivity Sweep 1: Number of Representative Bits (1 to 8 bits)...\n";
    std::ofstream bits_csv("results/sensitivity_bits.csv");
    bits_csv << "Bits,Groups,UopMisses,MissReductionVsLRU\n";
    for (uint32_t bits = 1; bits <= 8; ++bits) {
        CacheConfig cfg_bits = config;
        cfg_bits.num_weight_bits = bits;
        auto tagged = Profiler::profile_and_tag_trace(raw_trace, cfg_bits);
        SimulationStats st_bits;
        auto p_bits = std::make_shared<FURBYSPolicy>(cfg_bits.num_sets, cfg_bits.num_ways, cfg_bits.bypass_threshold_k, cfg_bits.pitfall_depth);
        run_policy_simulation("FURBYS-Bits-" + std::to_string(bits), p_bits, tagged, cfg_bits, st_bits);
        double red = 100.0 * (static_cast<double>(stats_lru.uop_misses) - st_bits.uop_misses) / stats_lru.uop_misses;
        bits_csv << bits << "," << (1 << bits) << "," << st_bits.uop_misses << "," << red << "\n";
        std::cout << "    - " << bits << " bits (" << (1 << bits) << " groups): Miss Red = " << red << "%\n";
    }
    bits_csv.close();

    std::cout << "\n[*] Running Sensitivity Sweep 2: Depth of Local Pitfall Detector (1 to 5)...\n";
    std::ofstream depth_csv("results/sensitivity_pitfall_depth.csv");
    depth_csv << "Depth,UopMisses,PitfallTriggers,MissReductionVsLRU\n";
    for (uint32_t depth = 1; depth <= 5; ++depth) {
        CacheConfig cfg_depth = config;
        cfg_depth.pitfall_depth = depth;
        SimulationStats st_depth;
        auto p_depth = std::make_shared<FURBYSPolicy>(cfg_depth.num_sets, cfg_depth.num_ways, cfg_depth.bypass_threshold_k, depth);
        run_policy_simulation("FURBYS-Depth-" + std::to_string(depth), p_depth, furbys_tagged_trace, cfg_depth, st_depth);
        double red = 100.0 * (static_cast<double>(stats_lru.uop_misses) - st_depth.uop_misses) / stats_lru.uop_misses;
        depth_csv << depth << "," << st_depth.uop_misses << "," << st_depth.pitfall_triggers << "," << red << "\n";
        std::cout << "    - Depth " << depth << ": Pitfall Triggers = " << st_depth.pitfall_triggers << ", Miss Red = " << red << "%\n";
    }
    depth_csv.close();

    std::cout << "\n[*] Running Sensitivity Sweep 3: Bypass On vs Bypass Off (Figure 21)...\n";
    std::ofstream bypass_csv("results/sensitivity_bypass.csv");
    bypass_csv << "BypassMode,UopMisses,Bypasses,MissReductionVsLRU\n";

    // Bypass On (K = 1)
    double red_on = 100.0 * (static_cast<double>(stats_lru.uop_misses) - stats_furbys.uop_misses) / stats_lru.uop_misses;
    bypass_csv << "BypassOn," << stats_furbys.uop_misses << "," << stats_furbys.selective_bypasses << "," << red_on << "\n";

    // Bypass Off (K = 999 so no block is ever bypassed)
    SimulationStats st_nobypass;
    auto p_nobypass = std::make_shared<FURBYSPolicy>(config.num_sets, config.num_ways, 999, config.pitfall_depth);
    run_policy_simulation("FURBYS-NoBypass", p_nobypass, furbys_tagged_trace, config, st_nobypass);
    double red_off = 100.0 * (static_cast<double>(stats_lru.uop_misses) - st_nobypass.uop_misses) / stats_lru.uop_misses;
    bypass_csv << "BypassOff," << st_nobypass.uop_misses << ",0," << red_off << "\n";
    bypass_csv.close();

    std::cout << "    - Bypass On:  Miss Red = " << red_on << "%, Bypasses = " << stats_furbys.selective_bypasses << "\n";
    std::cout << "    - Bypass Off: Miss Red = " << red_off << "%, Bypasses = 0\n";
    std::cout << "    - Bypass Benefit: +" << (red_on - red_off) << "% additional miss reduction!\n";

    std::cout << "\n[+] All evaluations and sensitivity results successfully generated in results/\n";
    return 0;
}

