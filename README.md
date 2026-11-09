# Efficient Micro-op Cache Replacement Policies (HPCA 2025)

Implementation of the research paper:  
**"From Optimal to Practical: Efficient Micro-op Cache Replacement Policies for Data Center Applications"**  
*(Kan Zhu, Yilong Zhao, Yufei Gao, Peter Braun, Tanvir Ahmed Khan, Heiner Litz, Baris Kasikci, Shuwen Deng — HPCA 2025)*

---

## 1. Overview & Architecture

Modern x86 processors (such as AMD Zen 3 and Zen 4) employ a dedicated **micro-op ($\mu$-op) cache** to bypass the power-hungry x86 instruction decoder and L1 instruction cache. This repository implements:

1. **Baseline Policies**:
   - **LRU** (Least Recently Used)
   - **SRRIP** (2-bit Static Re-reference Interval Prediction)
   - **Belady's MIN** (Oracle offline optimal for conventional caches)
2. **FLACK (Offline Near-Optimal)**:
   - Evaluates variable cost/size ($\frac{\text{cost}}{\text{size}} = \frac{\text{uops}}{\text{entries}}$).
   - Handles partial hits by selectively keeping larger Prediction Windows (PWs).
   - Incorporates lazy eviction to mitigate asynchronous lookup/insertion delays.
3. **FURBYS (Online Practical Policy)**:
   - **Jenks Natural Breaks (1D Clustering)**: Partitions per-set PW hit rates into 8 buckets ($0\dots 7$, 3-bit hints injected into branch instructions).
   - **Min Module**: Scans cache ways to find the minimum weight $W_{\min}$ (tie-broken by max SRRIP RRPV).
   - **Selective Bypassing**: When $W_{\text{new}} < W_{\min} - K$ (with $K=1$), bypasses cache insertion to prevent polluting cache and save write energy.
   - **Local Miss-Pitfall Detector**: 2-slot register tracking repeated evictions of the same way. When the same way is evicted consecutively, it degrades to 2-bit SRRIP to flush stale high-weight lines.
   - **Hardware Overhead**: Only **46 bits per 8-way set** ($<1.0\%$ storage overhead vs 4,608 bits data).

---

## 2. Directory Structure

- `include/`:
  - `uop_types.hh`: Core data structures (`PredictionWindow`, `CacheEntry`, `SimulationStats`).
  - `uop_cache.hh`: Micro-op cache model (AMD Zen 3: 512 entries, 8-way, 64 sets, 8 uops/entry, async decode queue).
  - `jenks.hh`: Fisher-Jenks dynamic programming clustering implementation.
  - `profiler.hh`: 7-step profiling pipeline (FLACK profiling + Jenks 3-bit weighting).
  - `trace_loader.hh`: Synthetic & trace file loader.
  - `policies/`: Implementations of `LRU`, `SRRIP`, `Belady`, `FLACK`, and `FURBYS`.
- `src/`: Core implementation files (`main.cc`, `uop_cache.cc`, `jenks.cc`, `profiler.cc`, `trace_loader.cc`).
- `gem5_integration/`:
  - `furbys_rp.hh` / `furbys_rp.cc`: gem5 `BaseReplacementPolicy` class.
  - `ReplacementPolicies.py`: gem5 SimObject definition.
  - `README_GEM5.md`: Instructions for integrating into full gem5 builds.
- `scripts/`:
  - `run_benchmarks.py`: Automated multi-workload evaluation and CSV aggregation.
- `results/`: CSV output files for main evaluation, sensitivity sweeps (bits 1..8, pitfall depth 1..5, bypass on/off).

---

## 3. How to Build & Run

### A. Standalone C++ Simulator (Fast & Zero-Dependency)
Requires any modern C++20 compiler (`g++`, `clang++`, or `MSVC`):

```bash
# Using g++
make
# Or directly:
g++ -std=c++20 -O3 -Wall -Wextra -Iinclude src/main.cc src/uop_cache.cc src/jenks.cc src/profiler.cc src/trace_loader.cc -o uop_sim.exe

# Run default benchmark simulation
./uop_sim.exe --workload Clang --accesses 50000

# Run multi-workload automated suite
python scripts/run_benchmarks.py
```

### B. gem5 Integration
See `gem5_integration/README_GEM5.md` for steps to plug `furbys_rp.cc` and `furbys_rp.hh` into gem5's `src/mem/cache/replacement_policies/`.

---

## 4. Experimental Results

Summary of results obtained on AMD Zen 3 geometry (512 entries, 8-way set-associative):

| Policy | Uop Miss Reduction vs LRU | Telemetry / Mechanism |
| :--- | :---: | :--- |
| **LRU (Baseline)** | 0.00% | Suffers capacity and conflict thrashing |
| **SRRIP (2-bit)** | ~3.8% – 5.8% | Ignores variable uop size/cost and partial hits |
| **Belady-MIN** | ~28.0% – 31.8% | Lookahead oracle (optimizes object misses only) |
| **FLACK (Offline)** | **~30.4% – 34.6%** | Near-optimal: cost/size aware, partial hits, lazy eviction |
| **FURBYS (Online)** | **~10.6% – 16.9%** | Profile-guided 3-bit hints, selective bypass ($K=1$), pitfall detector |
| **FURBYS (Cross-Input)** | **~94% of same-input** | Robust across different application inputs/seeds |

### Sensitivity Findings
- **Weight Bits (1 to 8 bits):** 3 bits (8 groups) provides the optimal trade-off between miss reduction and $<1\%$ hardware overhead (Figure 19 in paper).
- **Pitfall Detector Depth (1 to 5):** Depth 2 provides the ideal responsiveness to phase transitions (Figure 20 in paper).
- **Selective Bypassing ($K=1$):** Filters out ~25–30% of cold insertions, protecting warm lines and saving dynamic write energy (Figure 21 in paper).
