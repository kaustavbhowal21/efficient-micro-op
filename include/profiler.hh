#ifndef PROFILER_HH
#define PROFILER_HH

#include "uop_types.hh"
#include <vector>
#include <unordered_map>

class Profiler {
public:
    // Performs Steps 2-6 of FURBYS:
    // 1. Simulates the trace with FLACK to compute exact per-PW micro-op hit rates
    // 2. Uses Jenks Natural Breaks to cluster hit rates per cache set into 2^weight_bits buckets
    // 3. Returns the trace annotated with 3-bit (or N-bit) weight hints for each PW
    static std::vector<PredictionWindow> profile_and_tag_trace(
        const std::vector<PredictionWindow>& raw_trace,
        const CacheConfig& config);

    // Cross-input profiling: profiles on train_trace, and tags test_trace using the learned breaks
    static std::vector<PredictionWindow> profile_cross_input(
        const std::vector<PredictionWindow>& train_trace,
        const std::vector<PredictionWindow>& test_trace,
        const CacheConfig& config);
};

#endif // PROFILER_HH

