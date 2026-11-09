#ifndef TRACE_LOADER_HH
#define TRACE_LOADER_HH

#include "uop_types.hh"
#include <string>
#include <vector>

class TraceLoader {
public:
    // Load trace from a file (CSV format: start_pc,size,cost,end_pc)
    static std::vector<PredictionWindow> load_from_file(const std::string& filepath);

    // Save trace to a file
    static void save_to_file(const std::string& filepath, const std::vector<PredictionWindow>& trace);

    // Generates a realistic benchmark trace mimicking data center workloads:
    // - Massive instruction footprint (capacity pressure on 512 entries)
    // - Overlapping Prediction Windows (same start PC, different lengths due to branches)
    // - Variable micro-op counts (1 to 8 uops per entry)
    // - Phase transitions (triggering the Local Pitfall Detector)
    // - Cold streaming blocks (exercising Selective Bypassing)
    static std::vector<PredictionWindow> generate_benchmark_trace(
        const std::string& workload_name, 
        size_t num_accesses = 100000, 
        uint32_t seed = 42);
};

#endif // TRACE_LOADER_HH

