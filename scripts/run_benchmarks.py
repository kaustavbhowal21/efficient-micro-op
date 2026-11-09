#!/usr/bin/env python3
"""
Benchmark Runner and Sensitivity Evaluator for Micro-op Cache Policies
Paper: 'From Optimal to Practical: Efficient Micro-op Cache Replacement Policies
        for Data Center Applications' (HPCA 2025)
"""

import subprocess
import os
import csv
import sys

WORKLOADS = ["Clang", "Cassandra", "Kafka", "PostgreSQL", "Mediawiki", "Tomcat"]

def run_workload(workload, accesses=20000):
    cmd = ["./uop_sim.exe", "--workload", workload, "--accesses", str(accesses)]
    print(f"[*] Running workload: {workload} ({accesses} accesses)...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error running {workload}: {res.stderr}")
        return None
    
    # Read main evaluation results
    rows = []
    if os.path.exists("results/main_evaluation.csv"):
        with open("results/main_evaluation.csv", "r") as f:
            reader = csv.DictReader(f)
            for r in reader:
                r["Workload"] = workload
                rows.append(r)
    return rows

def main():
    print("=" * 70)
    print("  HPCA 2025 Micro-op Cache Replacement Policies Sweep")
    print("=" * 70)

    all_results = []
    for wl in WORKLOADS:
        res = run_workload(wl, accesses=25000)
        if res:
            all_results.extend(res)

    # Save aggregated CSV
    agg_file = "results/all_workloads_summary.csv"
    if all_results:
        with open(agg_file, "w", newline="") as f:
            fieldnames = ["Workload"] + [k for k in all_results[0].keys() if k != "Workload"]
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for r in all_results:
                writer.writerow(r)
        print(f"\n[+] Aggregated results saved to {agg_file}")

    # Display comparison table
    print("\n" + "=" * 80)
    print(f"{'Workload':<12} | {'Policy':<22} | {'Uop Misses':<12} | {'Miss Red. vs LRU':<16} | {'Bypasses':<10}")
    print("-" * 80)
    for r in all_results:
        print(f"{r['Workload']:<12} | {r['Policy']:<22} | {r['UopMisses']:<12} | {float(r['MissReductionVsLRU']):>14.2f}% | {r['SelectiveBypasses']:<10}")
    print("=" * 80)

if __name__ == "__main__":
    main()

