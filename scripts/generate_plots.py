#!/usr/bin/env python3
"""
generate_plots.py: Generates high-quality SVG charts from simulation CSV results
Paper: 'From Optimal to Practical: Efficient Micro-op Cache Replacement Policies
        for Data Center Applications' (HPCA 2025)
No external dependencies required (pure standard Python).
"""

import csv
import os

def create_bar_chart(title, x_labels, series, filename, y_label="Miss Reduction vs LRU (%)", y_max=40):
    """
    series: list of (series_name, list_of_values, color)
    """
    width = 850
    height = 420
    margin_left = 70
    margin_right = 160
    margin_top = 60
    margin_bottom = 60

    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    num_groups = len(x_labels)
    num_series = len(series)
    group_width = plot_w / num_groups
    bar_width = (group_width * 0.75) / num_series

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}" style="background-color:#ffffff; font-family: -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Helvetica, Arial, sans-serif;">')

    # Title
    svg.append(f'<text x="{width/2}" y="30" font-size="16" font-weight="bold" text-anchor="middle" fill="#1e293b">{title}</text>')

    # Gridlines and Y-axis labels
    grid_steps = 5
    for i in range(grid_steps + 1):
        val = (y_max / grid_steps) * i
        y_pos = margin_top + plot_h - (i / grid_steps) * plot_h
        svg.append(f'<line x1="{margin_left}" y1="{y_pos}" x2="{margin_left + plot_w}" y2="{y_pos}" stroke="#e2e8f0" stroke-width="1" />')
        svg.append(f'<text x="{margin_left - 10}" y="{y_pos + 4}" font-size="11" text-anchor="end" fill="#64748b">{val:.0f}%</text>')

    # Y-axis Title
    svg.append(f'<text x="20" y="{margin_top + plot_h/2}" font-size="12" font-weight="bold" fill="#475569" transform="rotate(-90 20 {margin_top + plot_h/2})" text-anchor="middle">{y_label}</text>')

    # Axes lines
    svg.append(f'<line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{margin_left + plot_w}" y2="{margin_top + plot_h}" stroke="#94a3b8" stroke-width="1.5" />')
    svg.append(f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#94a3b8" stroke-width="1.5" />')

    # Bars
    for g_idx, label in enumerate(x_labels):
        g_start = margin_left + g_idx * group_width + (group_width * 0.125)
        # X-label
        x_center = margin_left + (g_idx + 0.5) * group_width
        svg.append(f'<text x="{x_center}" y="{margin_top + plot_h + 20}" font-size="11" font-weight="600" text-anchor="middle" fill="#334155">{label}</text>')

        for s_idx, (s_name, values, color) in enumerate(series):
            val = values[g_idx]
            bar_h = max(0.0, (val / y_max) * plot_h)
            bar_x = g_start + s_idx * bar_width
            bar_y = margin_top + plot_h - bar_h
            svg.append(f'<rect x="{bar_x:.1f}" y="{bar_y:.1f}" width="{bar_width-2:.1f}" height="{bar_h:.1f}" fill="{color}" rx="2" />')

    # Legend
    legend_x = margin_left + plot_w + 15
    legend_y = margin_top + 10
    for s_idx, (s_name, _, color) in enumerate(series):
        y = legend_y + s_idx * 24
        svg.append(f'<rect x="{legend_x}" y="{y}" width="14" height="14" fill="{color}" rx="2" />')
        svg.append(f'<text x="{legend_x + 20}" y="{y + 11}" font-size="11" fill="#1e293b">{s_name}</text>')

    svg.append('</svg>')
    with open(filename, 'w', encoding='utf-8') as f:
        f.write('\n'.join(svg))
    print(f"[+] Created plot: {filename}")

def create_line_chart(title, x_vals, y_vals, filename, x_label, y_label, color="#2563eb"):
    width = 600
    height = 360
    margin_left = 65
    margin_right = 40
    margin_top = 50
    margin_bottom = 50

    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom

    y_max = max(y_vals) * 1.25 if y_vals else 10.0
    y_min = min(0.0, min(y_vals))

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}" style="background-color:#ffffff; font-family: -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Helvetica, Arial, sans-serif;">')

    # Title
    svg.append(f'<text x="{width/2}" y="28" font-size="14" font-weight="bold" text-anchor="middle" fill="#1e293b">{title}</text>')

    # Grid
    for i in range(5):
        val = y_min + ((y_max - y_min) / 4) * i
        y_pos = margin_top + plot_h - (i / 4) * plot_h
        svg.append(f'<line x1="{margin_left}" y1="{y_pos}" x2="{margin_left + plot_w}" y2="{y_pos}" stroke="#e2e8f0" stroke-width="1" />')
        svg.append(f'<text x="{margin_left - 8}" y="{y_pos + 4}" font-size="10" text-anchor="end" fill="#64748b">{val:.1f}%</text>')

    # Axes
    svg.append(f'<line x1="{margin_left}" y1="{margin_top + plot_h}" x2="{margin_left + plot_w}" y2="{margin_top + plot_h}" stroke="#94a3b8" stroke-width="1.5" />')
    svg.append(f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_h}" stroke="#94a3b8" stroke-width="1.5" />')

    # Points & Lines
    num_pts = len(x_vals)
    pts = []
    for i in range(num_pts):
        px = margin_left + (i / max(1, num_pts - 1)) * plot_w
        py = margin_top + plot_h - ((y_vals[i] - y_min) / (y_max - y_min)) * plot_h
        pts.append((px, py))
        svg.append(f'<text x="{px}" y="{margin_top + plot_h + 18}" font-size="11" text-anchor="middle" fill="#334155">{x_vals[i]}</text>')

    # Draw polyline
    path_data = " ".join([f"{'M' if i==0 else 'L'} {px:.1f} {py:.1f}" for i, (px, py) in enumerate(pts)])
    svg.append(f'<path d="{path_data}" fill="none" stroke="{color}" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" />')

    # Draw circles & data labels
    for i, (px, py) in enumerate(pts):
        svg.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="4.5" fill="{color}" stroke="#ffffff" stroke-width="2" />')
        svg.append(f'<text x="{px:.1f}" y="{py - 8:.1f}" font-size="10" font-weight="bold" text-anchor="middle" fill="{color}">{y_vals[i]:.1f}%</text>')

    # Axis Labels
    svg.append(f'<text x="{margin_left + plot_w/2}" y="{height - 10}" font-size="11" font-weight="bold" text-anchor="middle" fill="#475569">{x_label}</text>')
    svg.append(f'<text x="18" y="{margin_top + plot_h/2}" font-size="11" font-weight="bold" fill="#475569" transform="rotate(-90 18 {margin_top + plot_h/2})" text-anchor="middle">{y_label}</text>')

    svg.append('</svg>')
    with open(filename, 'w', encoding='utf-8') as f:
        f.write('\n'.join(svg))
    print(f"[+] Created plot: {filename}")

def main():
    os.makedirs("results", exist_ok=True)

    # 1. Plot Figure 8 / Figure 10: Multi-Workload Miss Reduction
    agg_file = "results/all_workloads_summary.csv"
    if os.path.exists(agg_file):
        workloads = []
        srrip_vals = []
        belady_vals = []
        flack_vals = []
        furbys_vals = []

        with open(agg_file, "r") as f:
            reader = csv.DictReader(f)
            data = list(reader)

        unique_wl = sorted(list(set(r["Workload"] for r in data)))
        for wl in unique_wl:
            workloads.append(wl)
            wl_rows = {r["Policy"]: float(r["MissReductionVsLRU"]) for r in data if r["Workload"] == wl}
            srrip_vals.append(wl_rows.get("SRRIP (2-bit)", 0.0))
            belady_vals.append(wl_rows.get("Belady-MIN", 0.0))
            flack_vals.append(wl_rows.get("FLACK (Offline)", 0.0))
            furbys_vals.append(wl_rows.get("FURBYS (Same-Input)", 0.0))

        series = [
            ("SRRIP (2-bit)", srrip_vals, "#94a3b8"),
            ("Belady-MIN", belady_vals, "#f59e0b"),
            ("FLACK (Offline)", flack_vals, "#10b981"),
            ("FURBYS (Online)", furbys_vals, "#3b82f6")
        ]
        create_bar_chart(
            "Figure 8: Micro-op Miss Reduction vs LRU Baseline Across Data Center Workloads",
            workloads, series, "results/fig8_miss_reduction.svg", y_max=40)

    # 2. Plot Figure 19: Sensitivity to Number of Bits
    bits_file = "results/sensitivity_bits.csv"
    if os.path.exists(bits_file):
        bits = []
        reds = []
        with open(bits_file, "r") as f:
            reader = csv.DictReader(f)
            for r in reader:
                bits.append(r["Bits"])
                reds.append(float(r["MissReductionVsLRU"]))
        create_line_chart(
            "Figure 19: Miss Reduction vs. Number of Representative Bits (Jenks Groups)",
            bits, reds, "results/fig19_sensitivity_bits.svg",
            x_label="# of Representative Bits (Groups = 2^Bits)",
            y_label="Miss Reduction vs LRU (%)",
            color="#2563eb")

    # 3. Plot Figure 20: Depth of Local Pitfall Detector
    depth_file = "results/sensitivity_pitfall_depth.csv"
    if os.path.exists(depth_file):
        depths = []
        reds = []
        with open(depth_file, "r") as f:
            reader = csv.DictReader(f)
            for r in reader:
                depths.append("Depth " + r["Depth"])
                reds.append(float(r["MissReductionVsLRU"]))
        create_line_chart(
            "Figure 20: Miss Reduction vs. Local Pitfall Detector Depth",
            depths, reds, "results/fig20_sensitivity_pitfall_depth.svg",
            x_label="Pitfall Detector Register Depth",
            y_label="Miss Reduction vs LRU (%)",
            color="#059669")

    # 4. Plot Figure 21: Bypass On vs Bypass Off
    bypass_file = "results/sensitivity_bypass.csv"
    if os.path.exists(bypass_file):
        modes = []
        reds = []
        with open(bypass_file, "r") as f:
            reader = csv.DictReader(f)
            for r in reader:
                modes.append(r["BypassMode"])
                reds.append(float(r["MissReductionVsLRU"]))
        series = [("FURBYS", reds, "#3b82f6")]
        create_bar_chart(
            "Figure 21: Benefit of FURBYS Selective Bypassing (K=1)",
            modes, series, "results/fig21_sensitivity_bypass.svg",
            y_label="Miss Reduction vs LRU (%)",
            y_max=15)

if __name__ == "__main__":
    main()

