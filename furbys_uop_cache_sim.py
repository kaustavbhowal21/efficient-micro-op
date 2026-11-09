#!/usr/bin/env python3
"""
furbys_uop_cache_sim.py: Python Reference Simulator for Micro-op Cache Replacement Policies
Paper: 'From Optimal to Practical: Efficient Micro-op Cache Replacement Policies
        for Data Center Applications' (HPCA 2025)
Author: Group GN4 (CS2201 COA Assignment)
"""

import sys
import math
import random
from collections import deque, defaultdict

# ---------------------------------------------------------------------------
# Data Structures
# ---------------------------------------------------------------------------

class PredictionWindow:
    def __init__(self, start_pc, size, cost, end_pc=0, weight_hint=0):
        self.start_pc = start_pc
        self.size = size          # Number of entries occupied (1 or 2)
        self.cost = cost          # Number of valid micro-ops (1 to 8*size)
        self.end_pc = end_pc
        self.weight_hint = weight_hint

class CacheEntry:
    def __init__(self):
        self.valid = False
        self.start_pc = 0
        self.size = 1
        self.cost = 1
        self.weight = 0
        self.rrpv = 2            # Default SRRIP distant re-reference interval
        self.last_access_cycle = 0

class MicroOpCacheConfig:
    def __init__(self, num_sets=64, num_ways=8, k=1, pitfall_depth=2, weight_bits=3):
        self.num_sets = num_sets          # 64 sets
        self.num_ways = num_ways          # 8 ways (512 total entries, AMD Zen3)
        self.bypass_threshold_k = k       # K = 1
        self.pitfall_depth = pitfall_depth# 2-slot pitfall detector
        self.weight_bits = weight_bits    # 3 bits -> 8 groups
        self.decode_latency = 5           # Cycles through legacy decode pipeline

# ---------------------------------------------------------------------------
# Jenks Natural Breaks (1D Optimal Clustering via Dynamic Programming)
# ---------------------------------------------------------------------------

def jenks_natural_breaks(values, num_classes):
    """
    Partitions 1D values into num_classes minimizing within-class variance.
    Returns cut-off thresholds for each class.
    """
    if not values:
        return [0.0] * num_classes
    vals = sorted(values)
    n = len(vals)
    if num_classes >= n:
        return vals + [vals[-1]] * (num_classes - n)

    mat1 = [[float('inf')] * (num_classes + 1) for _ in range(n + 1)]
    mat2 = [[0] * (num_classes + 1) for _ in range(n + 1)]

    for c in range(1, num_classes + 1):
        mat1[0][c] = 0.0
        mat1[1][c] = 0.0
        mat2[0][c] = 0
        mat2[1][c] = 1

    for l in range(2, n + 1):
        s1 = 0.0
        s2 = 0.0
        w = 0.0
        for m in range(1, l + 1):
            i3 = l - m + 1
            val = vals[i3 - 1]
            s2 += val * val
            s1 += val
            w += 1.0
            v = s2 - (s1 * s1) / w
            i4 = i3 - 1
            if i4 != 0:
                for j in range(2, num_classes + 1):
                    if mat1[l][j] >= (v + mat1[i4][j - 1]):
                        mat2[l][j] = i3
                        mat1[l][j] = v + mat1[i4][j - 1]
        mat1[l][1] = s2 - (s1 * s1) / w
        mat2[l][1] = 1

    breaks = [0.0] * num_classes
    breaks[-1] = vals[-1]
    k = n
    for j in range(num_classes, 1, -1):
        split = mat2[k][j]
        idx = max(0, split - 2)
        breaks[j - 2] = vals[idx]
        k = split - 1
        if k < 1:
            break

    return breaks

def get_jenks_class(val, breaks):
    for i, b in enumerate(breaks):
        if val <= b:
            return i
    return len(breaks) - 1

# ---------------------------------------------------------------------------
# Micro-op Cache Simulation Engine
# ---------------------------------------------------------------------------

class MicroOpCacheSimulator:
    def __init__(self, config, policy_name="FURBYS"):
        self.cfg = config
        self.policy_name = policy_name
        self.sets = [[CacheEntry() for _ in range(config.num_ways)] for _ in range(config.num_sets)]
        self.cycle = 0

        # Stats
        self.pw_lookups = 0
        self.pw_full_hits = 0
        self.pw_partial_hits = 0
        self.pw_misses = 0
        self.uop_lookups = 0
        self.uop_hits = 0
        self.uop_misses = 0
        self.selective_bypasses = 0
        self.pitfall_triggers = 0
        self.total_evictions = 0

        # Asynchronous insertion queue: deque of (ready_cycle, set_idx, PredictionWindow)
        self.insertion_queue = deque()

        # FURBYS / SRRIP per-set state
        self.pitfall_slots = [deque(maxlen=config.pitfall_depth) for _ in range(config.num_sets)]

        # Future trace for Belady / FLACK
        self.future_map = defaultdict(deque)

    def set_trace_for_offline(self, trace):
        self.future_map.clear()
        for idx, pw in enumerate(trace):
            self.future_map[pw.start_pc].append((idx, pw.size, pw.cost))

    def advance_offline_trace(self, current_idx, pw):
        q = self.future_map.get(pw.start_pc)
        if q and q[0][0] == current_idx:
            q.popleft()

    def process_pending_insertions(self):
        while self.insertion_queue and self.insertion_queue[0][0] <= self.cycle:
            _, set_idx, pw = self.insertion_queue.popleft()
            target_set = self.sets[set_idx]

            # Check if line already present (e.g. from an intermediate hit)
            found_way = -1
            for w in range(self.cfg.num_ways):
                if target_set[w].valid and target_set[w].start_pc == pw.start_pc:
                    found_way = w
                    break

            if found_way >= 0:
                if pw.size > target_set[found_way].size:
                    target_set[found_way].size = pw.size
                    target_set[found_way].cost = pw.cost
                    target_set[found_way].weight = pw.weight_hint
                    target_set[found_way].last_access_cycle = self.cycle
                continue

            # Select victim or decide to bypass
            victim_way = self.select_victim_or_bypass(set_idx, pw)

            if victim_way < 0:
                self.selective_bypasses += 1
                continue

            if target_set[victim_way].valid:
                self.total_evictions += 1

            entry = target_set[victim_way]
            entry.valid = True
            entry.start_pc = pw.start_pc
            entry.size = pw.size
            entry.cost = pw.cost
            entry.weight = pw.weight_hint
            entry.rrpv = 2
            entry.last_access_cycle = self.cycle

    def select_victim_or_bypass(self, set_idx, incoming_pw):
        target_set = self.sets[set_idx]

        # 1. Check for empty way
        for w in range(self.cfg.num_ways):
            if not target_set[w].valid:
                return w

        # 2. Policy-specific eviction logic
        if self.policy_name == "LRU":
            oldest_cycle = float('inf')
            lru_way = 0
            for w in range(self.cfg.num_ways):
                if target_set[w].last_access_cycle < oldest_cycle:
                    oldest_cycle = target_set[w].last_access_cycle
                    lru_way = w
            return lru_way

        elif self.policy_name == "SRRIP":
            while True:
                for w in range(self.cfg.num_ways):
                    if target_set[w].rrpv == 3:
                        return w
                for w in range(self.cfg.num_ways):
                    if target_set[w].rrpv < 3:
                        target_set[w].rrpv += 1

        elif self.policy_name == "Belady":
            furthest = -1
            victim_way = 0
            for w in range(self.cfg.num_ways):
                pc = target_set[w].start_pc
                q = self.future_map.get(pc)
                if not q:
                    return w  # Dead line
                next_use = q[0][0]
                if next_use > furthest:
                    furthest = next_use
                    victim_way = w
            return victim_way

        elif self.policy_name == "FLACK":
            max_score = -1.0
            victim_way = 0
            for w in range(self.cfg.num_ways):
                pc = target_set[w].start_pc
                q = self.future_map.get(pc)
                if not q:
                    return w
                next_idx, next_size, next_cost = q[0]
                dist = max(1, next_idx - self.cycle)
                if target_set[w].size >= next_size:
                    dist *= 0.8  # Partial hit retention priority
                density = target_set[w].cost / max(1, target_set[w].size)
                score = dist / max(0.1, density)
                if score > max_score:
                    max_score = score
                    victim_way = w

            # FLACK selective bypass
            in_q = self.future_map.get(incoming_pw.start_pc)
            if not in_q:
                return -1
            in_dist = max(1, in_q[0][0] - self.cycle)
            in_density = incoming_pw.cost / max(1, incoming_pw.size)
            if (in_dist / max(0.1, in_density)) > max_score * 1.5:
                return -1

            return victim_way

        elif "FURBYS" in self.policy_name:
            # 1. Min Module: scan all ways to find minimum weight
            min_grp = 255
            min_way = 0
            max_rrpv = 0
            for w in range(self.cfg.num_ways):
                w_val = target_set[w].weight
                r_val = target_set[w].rrpv
                if w_val < min_grp:
                    min_grp = w_val
                    min_way = w
                    max_rrpv = r_val
                elif w_val == min_grp:
                    if r_val > max_rrpv:
                        min_way = w
                        max_rrpv = r_val

            # 2. Selective Bypassing: W_new < W_min - K
            if incoming_pw.weight_hint < (min_grp - self.cfg.bypass_threshold_k):
                return -1  # Bypass

            # 3. Local Miss-Pitfall Detector
            p_slots = self.pitfall_slots[set_idx]
            pitfall_triggered = False
            if len(p_slots) == self.cfg.pitfall_depth and self.cfg.pitfall_depth > 0:
                pitfall_triggered = all(slot == min_way for slot in p_slots)

            if pitfall_triggered:
                self.pitfall_triggers += 1
                # Fallback to SRRIP Max Module (RRPV == 3)
                victim_way = self.find_srrip_victim(set_idx)
                p_slots.clear()
            else:
                victim_way = min_way
                p_slots.append(min_way)

            return victim_way

        return 0

    def find_srrip_victim(self, set_idx):
        target_set = self.sets[set_idx]
        while True:
            for w in range(self.cfg.num_ways):
                if target_set[w].rrpv == 3:
                    return w
            for w in range(self.cfg.num_ways):
                if target_set[w].rrpv < 3:
                    target_set[w].rrpv += 1

    def access(self, pw):
        self.cycle += 1
        self.process_pending_insertions()

        set_idx = (pw.start_pc >> 6) % self.cfg.num_sets
        target_set = self.sets[set_idx]
        self.pw_lookups += 1
        self.uop_lookups += pw.cost

        # Search set
        hit_way = -1
        for w in range(self.cfg.num_ways):
            if target_set[w].valid and target_set[w].start_pc == pw.start_pc:
                hit_way = w
                break

        if hit_way >= 0:
            entry = target_set[hit_way]
            entry.last_access_cycle = self.cycle
            entry.rrpv = 0  # Hit promotion

            if entry.size >= pw.size:
                # Full Hit
                self.pw_full_hits += 1
                self.uop_hits += pw.cost
                return
            else:
                # Partial Hit
                self.pw_partial_hits += 1
                self.uop_hits += entry.cost
                missed_uops = pw.cost - entry.cost
                self.uop_misses += missed_uops
                # Schedule insertion of larger window
                self.insertion_queue.append((self.cycle + self.cfg.decode_latency, set_idx, pw))
                return

        # Miss
        self.pw_misses += 1
        self.uop_misses += pw.cost
        self.insertion_queue.append((self.cycle + self.cfg.decode_latency, set_idx, pw))

    def drain(self):
        while self.insertion_queue:
            self.cycle += 1
            self.process_pending_insertions()

# ---------------------------------------------------------------------------
# Trace Generation & Profiling Pipeline
# ---------------------------------------------------------------------------

def generate_benchmark_trace(workload="Clang", num_accesses=20000, seed=42):
    rng = random.Random(seed)
    total_pws = 4096
    base_pcs = [0x400000 + i * 64 for i in range(total_pws)]
    hot_cnt = int(total_pws * 0.05)
    warm_cnt = int(total_pws * 0.25)

    trace = []
    phase_len = num_accesses // 5
    shift = 0

    for i in range(num_accesses):
        if i > 0 and (i % phase_len == 0):
            shift += (total_pws // 8)

        r = rng.random()
        if r < 0.50:
            idx = (shift + rng.randint(0, hot_cnt - 1)) % total_pws
        elif r < 0.85:
            idx = (shift + hot_cnt + rng.randint(0, warm_cnt - 1)) % total_pws
        else:
            idx = (hot_cnt + warm_cnt + rng.randint(0, total_pws - hot_cnt - warm_cnt - 1)) % total_pws

        pc = base_pcs[idx]
        if idx % 5 == 0:
            size = 2 if rng.random() < 0.6 else 1
            cost = rng.randint(9, 15) if size == 2 else rng.randint(1, 8)
        else:
            size = 1 if rng.random() < 0.8 else 2
            cost = rng.randint(1, 8) if size == 1 else rng.randint(9, 15)

        trace.append(PredictionWindow(pc, size, cost, pc + size * 32))

    return trace

def profile_and_tag_trace(trace, config):
    # Step 3-5: Run FLACK offline to collect per-PC hit rate
    flack_sim = MicroOpCacheSimulator(config, "FLACK")
    flack_sim.set_trace_for_offline(trace)
    pc_stats = defaultdict(lambda: [0, 0])  # pc -> [hits, total]

    for idx, pw in enumerate(trace):
        flack_sim.advance_offline_trace(idx, pw)
        prev_hits = flack_sim.uop_hits
        flack_sim.access(pw)
        gained = flack_sim.uop_hits - prev_hits
        pc_stats[pw.start_pc][0] += gained
        pc_stats[pw.start_pc][1] += pw.cost

    flack_sim.drain()

    # Step 6: Jenks clustering per set
    num_classes = 1 << config.weight_bits
    set_pcs = [[] for _ in range(config.num_sets)]
    set_hrs = [[] for _ in range(config.num_sets)]

    for pc, (hits, total) in pc_stats.items():
        set_idx = (pc >> 6) % config.num_sets
        hr = hits / total if total > 0 else 0.0
        set_pcs[set_idx].append(pc)
        set_hrs[set_idx].append(hr)

    pc_weights = {}
    for s in range(config.num_sets):
        if not set_hrs[s]:
            continue
        breaks = jenks_natural_breaks(set_hrs[s], num_classes)
        for i, pc in enumerate(set_pcs[s]):
            w = get_jenks_class(set_hrs[s][i], breaks)
            pc_weights[pc] = w

    tagged_trace = []
    for pw in trace:
        w = pc_weights.get(pw.start_pc, 0)
        tagged_trace.append(PredictionWindow(pw.start_pc, pw.size, pw.cost, pw.end_pc, w))

    return tagged_trace

# ---------------------------------------------------------------------------
# Main Runner & Verification
# ---------------------------------------------------------------------------

def main():
    print("=" * 72)
    print("  FURBYS & FLACK Micro-op Cache Python Reference Simulator")
    print("  Modeling: AMD Zen3 (512 entries, 8-way, 64 sets, 8 uops/entry)")
    print("=" * 72)

    cfg = MicroOpCacheConfig()
    print("[*] Generating benchmark trace (Clang, 20000 accesses)...")
    raw_trace = generate_benchmark_trace("Clang", 20000, 42)

    # 1. LRU Baseline
    lru_sim = MicroOpCacheSimulator(cfg, "LRU")
    for pw in raw_trace:
        lru_sim.access(pw)
    lru_sim.drain()

    # 2. SRRIP
    srrip_sim = MicroOpCacheSimulator(cfg, "SRRIP")
    for pw in raw_trace:
        srrip_sim.access(pw)
    srrip_sim.drain()

    # 3. Belady MIN
    belady_sim = MicroOpCacheSimulator(cfg, "Belady")
    belady_sim.set_trace_for_offline(raw_trace)
    for idx, pw in enumerate(raw_trace):
        belady_sim.advance_offline_trace(idx, pw)
        belady_sim.access(pw)
    belady_sim.drain()

    # 4. FLACK (Offline Near-Optimal)
    flack_sim = MicroOpCacheSimulator(cfg, "FLACK")
    flack_sim.set_trace_for_offline(raw_trace)
    for idx, pw in enumerate(raw_trace):
        flack_sim.advance_offline_trace(idx, pw)
        flack_sim.access(pw)
    flack_sim.drain()

    # 5. FURBYS (Same-Input Profile)
    print("[*] Running 7-step profiling pipeline (FLACK + Jenks 3-bit clustering)...")
    tagged_trace = profile_and_tag_trace(raw_trace, cfg)
    furbys_sim = MicroOpCacheSimulator(cfg, "FURBYS")
    for pw in tagged_trace:
        furbys_sim.access(pw)
    furbys_sim.drain()

    # Comparative Table
    print("\n" + "+-----------------------+-------------+-------------+------------------+-----------------------------+")
    print("| Policy                | Uop Misses  | Miss Rate   | Miss Red. vs LRU | Key Telemetry / Behavior    |")
    print("+-----------------------+-------------+-------------+------------------+-----------------------------+")

    def row(name, sim, base, note):
        red = 100.0 * (base.uop_misses - sim.uop_misses) / base.uop_misses if base.uop_misses > 0 else 0.0
        mr = 100.0 * sim.uop_misses / sim.uop_lookups
        print(f"| {name:<21} | {sim.uop_misses:>11} | {mr:>8.2f}%  | {red:>+14.2f}%  | {note:<27} |")

    row("LRU (Baseline)", lru_sim, lru_sim, "Baseline cache")
    row("SRRIP (2-bit)", srrip_sim, lru_sim, "Ignores uop cost & size")
    row("Belady-MIN (Oracle)", belady_sim, lru_sim, "Object-optimal only")
    row("FLACK (Near-Optimal)", flack_sim, lru_sim, "Cost/size + partial-hit")
    row("FURBYS (Same-Input)", furbys_sim, lru_sim, f"{furbys_sim.selective_bypasses} Bypasses, {furbys_sim.pitfall_triggers} Pitfalls")
    print("+-----------------------+-------------+-------------+------------------+-----------------------------+\n")

    print("[+] Python reference simulation successfully completed!")

if __name__ == "__main__":
    main()

