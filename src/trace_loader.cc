#include "trace_loader.hh"
#include <fstream>
#include <sstream>
#include <random>
#include <iostream>

std::vector<PredictionWindow> TraceLoader::load_from_file(const std::string& filepath) {
    std::vector<PredictionWindow> trace;
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::cerr << "Warning: Could not open trace file: " << filepath << "\n";
        return trace;
    }

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string val;

        uint64_t pc = 0;
        uint32_t size = 1;
        uint32_t cost = 1;
        uint64_t end = 0;
        uint8_t weight = 0;

        if (std::getline(ss, val, ',')) pc = std::stoull(val, nullptr, 0);
        if (std::getline(ss, val, ',')) size = std::stoul(val);
        if (std::getline(ss, val, ',')) cost = std::stoul(val);
        if (std::getline(ss, val, ',')) end = std::stoull(val, nullptr, 0);
        if (std::getline(ss, val, ',')) weight = static_cast<uint8_t>(std::stoul(val));

        trace.emplace_back(pc, size, cost, end, weight);
    }
    return trace;
}

void TraceLoader::save_to_file(const std::string& filepath, const std::vector<PredictionWindow>& trace) {
    std::ofstream outfile(filepath);
    if (!outfile.is_open()) return;
    outfile << "# start_pc,size,cost,end_pc,weight_hint\n";
    for (const auto& pw : trace) {
        outfile << "0x" << std::hex << pw.start_pc << std::dec << ","
                << pw.size << ","
                << pw.cost << ","
                << "0x" << std::hex << pw.end_pc << std::dec << ","
                << static_cast<uint32_t>(pw.weight_hint) << "\n";
    }
}

std::vector<PredictionWindow> TraceLoader::generate_benchmark_trace(
    const std::string& workload_name, 
    size_t num_accesses, 
    uint32_t seed) {

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist_01(0.0, 1.0);
    std::uniform_int_distribution<uint32_t> dist_size(1, 2);
    std::uniform_int_distribution<uint32_t> dist_cost_s1(1, 8);
    std::uniform_int_distribution<uint32_t> dist_cost_s2(9, 15);

    // Set footprint sizes based on application characteristics
    size_t total_pws = 4096; // Massive footprint exceeding 512 entries
    if (workload_name == "PostgreSQL" || workload_name == "MySQL") {
        total_pws = 6144;
    } else if (workload_name == "Clang" || workload_name == "Mediawiki") {
        total_pws = 8192;
    }

    // Allocate base addresses aligned to 64 bytes
    std::vector<uint64_t> base_pcs(total_pws);
    for (size_t i = 0; i < total_pws; ++i) {
        base_pcs[i] = 0x400000ULL + (i * 64ULL);
    }

    // Assign categories: Hot (5%), Warm (25%), Cold (70%)
    size_t hot_count = total_pws * 5 / 100;
    size_t warm_count = total_pws * 25 / 100;

    std::vector<PredictionWindow> trace;
    trace.reserve(num_accesses);

    size_t phase_length = num_accesses / 5;
    size_t phase_shift = 0;

    for (size_t i = 0; i < num_accesses; ++i) {
        // Trigger periodic phase transitions (testing pitfall detector)
        if (i > 0 && (i % phase_length == 0)) {
            phase_shift += (total_pws / 8);
        }

        double r = dist_01(rng);
        size_t chosen_pw_idx = 0;

        if (r < 0.50) {
            // 50% accesses go to Hot set (shifted by phase)
            size_t h_idx = (phase_shift + (rng() % hot_count)) % total_pws;
            chosen_pw_idx = h_idx;
        } else if (r < 0.85) {
            // 35% accesses go to Warm set
            size_t w_idx = (phase_shift + hot_count + (rng() % warm_count)) % total_pws;
            chosen_pw_idx = w_idx;
        } else {
            // 15% accesses go to Cold tail (streaming code)
            size_t c_idx = (hot_count + warm_count + (rng() % (total_pws - hot_count - warm_count))) % total_pws;
            chosen_pw_idx = c_idx;
        }

        uint64_t pc = base_pcs[chosen_pw_idx];

        // Model overlapping prediction windows (partial hits)
        // 20% of branches alternate between short (size=1) and long (size=2)
        uint32_t size = 1;
        uint32_t cost = 4;
        if ((chosen_pw_idx % 5) == 0) {
            // Overlapping candidate: sometimes size 1, sometimes size 2
            if (dist_01(rng) < 0.6) {
                size = 2;
                cost = dist_cost_s2(rng);
            } else {
                size = 1;
                cost = dist_cost_s1(rng);
            }
        } else {
            size = (dist_01(rng) < 0.8) ? 1 : 2;
            cost = (size == 1) ? dist_cost_s1(rng) : dist_cost_s2(rng);
        }

        trace.emplace_back(pc, size, cost, pc + (size * 32), 0);
    }

    return trace;
}

