# gem5 Integration Guide for FURBYS (HPCA 2025)

This folder contains the complete gem5 replacement policy implementation for **FURBYS**:
- `furbys_rp.hh`: Header declaring `FURBYSRP` and `FURBYSReplData` (3-bit weight, 2-bit SRRIP RRPV, pitfall detector).
- `furbys_rp.cc`: C++ implementation of the Min Module, Selective Bypassing check, and Local Pitfall Detector with fallback to SRRIP.
- `ReplacementPolicies.py`: SimObject definition for gem5 configuration.

---

## 1. Steps to Integrate into gem5

1. **Copy Source Files:**
   Copy `furbys_rp.hh` and `furbys_rp.cc` into your gem5 source tree:
   ```bash
   cp gem5_integration/furbys_rp.hh <gem5_root>/src/mem/cache/replacement_policies/
   cp gem5_integration/furbys_rp.cc <gem5_root>/src/mem/cache/replacement_policies/
   ```

2. **Register in SConscript:**
   Open `<gem5_root>/src/mem/cache/replacement_policies/SConscript` and add:
   ```python
   Source('furbys_rp.cc')
   ```

3. **Register SimObject:**
   Append the definition from `ReplacementPolicies.py` into:
   `<gem5_root>/src/mem/cache/replacement_policies/ReplacementPolicies.py`.

4. **Recompile gem5 (x86):**
   ```bash
   scons build/X86/gem5.opt -j$(nproc)
   ```

---

## 2. Using FURBYSRP in Cache Configurations

In your Python configuration script (e.g. `se.py`, `fs.py`, or your custom AMD Zen3 CPU script):
```python
from m5.objects import *

# Instantiate an 8-way 64-set (512 entries) micro-op cache
class MicroOpCache(Cache):
    size = '32KiB'  # Or 512 entries
    assoc = 8
    tag_latency = 1
    data_latency = 1
    response_latency = 1
    mshrs = 16
    tgts_per_mshr = 8
    replacement_policy = FURBYSRP(bypass_threshold_k=1, pitfall_depth=2)
```

For standalone, lightweight trace simulations with 100% reproducible results and instant compilation, use the root C++ simulator (`uop_sim.exe`).

