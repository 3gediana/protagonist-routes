#!/usr/bin/env python3
"""Analyze Darwin vs Lamarck evolution comparison."""
import os
import csv
from pathlib import Path

def read_best_fitness(csv_path):
    """Read best_fitness column from evolution.csv.
    Handles both single-pop (best_fitness) and coevo (pred_best_fitness) formats."""
    best = []
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if 'best_fitness' in row:
                best.append(float(row['best_fitness']))
            elif 'pred_best_fitness' in row:
                # Use predator best as proxy for population fitness
                best.append(float(row['pred_best_fitness']))
            else:
                best.append(0.0)
    return best

def find_run_dirs(base_dir, count=3):
    """Find the most recent run directories."""
    if not os.path.exists(base_dir):
        return []
    dirs = sorted(Path(base_dir).iterdir(), key=lambda p: p.stat().st_mtime, reverse=True)
    return [d for d in dirs if d.is_dir()][:count]

def main():
    data_root = Path(r"D:\claude-code\c++\data\runs")
    
    darwin_runs = find_run_dirs(data_root / "learn_darwin", 3)
    lamarck_runs = find_run_dirs(data_root / "learn_lamarck", 3)
    
    if not darwin_runs or not lamarck_runs:
        print("ERROR: Could not find run directories")
        return
    
    # Collect best_fitness curves
    darwin_curves = []
    for d in darwin_runs:
        csv_path = d / "evolution.csv"
        if csv_path.exists():
            darwin_curves.append(read_best_fitness(csv_path))
    
    lamarck_curves = []
    for d in lamarck_runs:
        csv_path = d / "evolution.csv"
        if csv_path.exists():
            lamarck_curves.append(read_best_fitness(csv_path))
    
    if not darwin_curves or not lamarck_curves:
        print("ERROR: No evolution.csv data found")
        return
    
    # Compute averages
    min_len = min(min(len(c) for c in darwin_curves), min(len(c) for c in lamarck_curves))
    
    darwin_avg = []
    lamarck_avg = []
    for i in range(min_len):
        d_vals = [c[i] for c in darwin_curves if i < len(c)]
        l_vals = [c[i] for c in lamarck_curves if i < len(c)]
        darwin_avg.append(sum(d_vals) / len(d_vals))
        lamarck_avg.append(sum(l_vals) / len(l_vals))
    
    # Key metrics
    darwin_final = darwin_avg[-1]
    lamarck_final = lamarck_avg[-1]
    darwin_peak = max(darwin_avg)
    lamarck_peak = max(lamarck_avg)
    
    # Convergence: generation where fitness reaches 90% of final value
    darwin_conv = next((i for i, v in enumerate(darwin_avg) if v >= 0.9 * darwin_final), min_len)
    lamarck_conv = next((i for i, v in enumerate(lamarck_avg) if v >= 0.9 * lamarck_final), min_len)
    
    # Generate report
    report = f"""# F-LEARN Comparison: Darwin vs Lamarck

## Experimental Setup
- **Population**: 24 agents, NEAT brains
- **Generations**: 200, Episode ticks: 200
- **Seeds**: 3 per mode (42, 43, 44)
- **Key difference**: `lamarck_mode = false` (Darwin) vs `lamarck_mode = true` (Lamarck)
- Both modes have lifetime Hebbian learning enabled

## Results

| Metric | Darwin | Lamarck |
|--------|--------|---------|
| Final avg best_fitness | {darwin_final:.1f} | {lamarck_final:.1f} |
| Peak avg best_fitness | {darwin_peak:.1f} | {lamarck_peak:.1f} |
| Convergence gen (90% final) | {darwin_conv} | {lamarck_conv} |

## Fitness Curve (sampled every 25 gen)

| Gen | Darwin | Lamarck |
|-----|--------|---------|
"""
    for i in range(0, min_len, 25):
        report += f"| {i} | {darwin_avg[i]:.1f} | {lamarck_avg[i]:.1f} |\n"
    report += f"| {min_len-1} | {darwin_avg[-1]:.1f} | {lamarck_avg[-1]:.1f} |\n"
    
    report += f"""
## Analysis

"""
    if lamarck_final > darwin_final * 1.05:
        report += f"**Lamarckian mode achieves higher final fitness** ({lamarck_final:.1f} vs {darwin_final:.1f}), "
        report += f"suggesting that inheriting learned weights provides a cumulative advantage across generations.\n"
    elif darwin_final > lamarck_final * 1.05:
        report += f"**Darwinian mode achieves higher final fitness** ({darwin_final:.1f} vs {lamarck_final:.1f}), "
        report += f"suggesting that Lamarckian inheritance may cause premature convergence or overfitting.\n"
    else:
        report += f"**Both modes achieve similar final fitness** ({darwin_final:.1f} vs {lamarck_final:.1f}), "
        report += f"with no clear advantage from Lamarckian inheritance.\n"
    
    if lamarck_conv < darwin_conv:
        report += f"\nLamarckian mode converges faster (gen {lamarck_conv} vs {darwin_conv}), "
        report += "consistent with the hypothesis that inherited learning accelerates adaptation.\n"
    elif darwin_conv < lamarck_conv:
        report += f"\nDarwinian mode converges faster (gen {darwin_conv} vs {lamarck_conv}).\n"
    
    report += "\n## Conclusion\n"
    if lamarck_final > darwin_final * 1.05 or lamarck_conv < darwin_conv - 10:
        report += "Lamarckian inheritance shows measurable benefit in this configuration, "
        report += "supporting the F-LEARN module's claim that weight inheritance can accelerate evolution.\n"
    else:
        report += "Lamarckian inheritance does not show significant benefit under current parameters. "
        report += "Further tuning (higher learning rate, longer episodes) may be needed.\n"
    
    # Write report
    out_dir = Path(r"D:\claude-code\c++\docs\experiments")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "F-LEARN_comparison.md"
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(report)
    print(f"Report written to {out_path}")
    print(f"Darwin final: {darwin_final:.1f}, Lamarck final: {lamarck_final:.1f}")

if __name__ == "__main__":
    main()
