#!/usr/bin/env python3
"""Analyze F-NOVELTY ablation: novelty on vs off."""
import csv
import sys
from pathlib import Path

def read_best_fitness(csv_path):
    best = []
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if 'best_fitness' in row:
                best.append(float(row['best_fitness']))
            elif 'pred_best_fitness' in row:
                best.append(float(row['pred_best_fitness']))
    return best

def detect_stairs(fitness, plateau_len=10, jump_pct=0.05):
    stairs = []
    i = 0
    while i < len(fitness):
        j = i
        while j < len(fitness) - 1:
            if fitness[j] == 0:
                j += 1
                continue
            pct_change = abs(fitness[j+1] - fitness[j]) / max(abs(fitness[j]), 1e-6)
            if pct_change < jump_pct:
                j += 1
            else:
                break
        plateau_length = j - i
        if plateau_length >= plateau_len and j < len(fitness) - 1:
            if fitness[j] != 0:
                jump_pct_actual = (fitness[j+1] - fitness[j]) / max(abs(fitness[j]), 1e-6)
                if jump_pct_actual >= jump_pct:
                    stairs.append({'start': i, 'end': j, 'length': plateau_length})
        i = j + 1
    return stairs

def find_run_dirs(base_dir, count=3):
    if not Path(base_dir).exists():
        return []
    dirs = sorted(Path(base_dir).iterdir(), key=lambda p: p.stat().st_mtime, reverse=True)
    return [d for d in dirs if d.is_dir()][:count]

def main():
    data_root = Path(r"D:\claude-code\c++\data\runs")
    
    on_dirs = find_run_dirs(data_root / "novelty_on", 3)
    off_dirs = find_run_dirs(data_root / "novelty_off", 3)
    
    on_curves = []
    for d in on_dirs:
        csv_path = d / "evolution.csv"
        if csv_path.exists():
            on_curves.append(read_best_fitness(csv_path))
    
    off_curves = []
    for d in off_dirs:
        csv_path = d / "evolution.csv"
        if csv_path.exists():
            off_curves.append(read_best_fitness(csv_path))
    
    if not on_curves or not off_curves:
        print("ERROR: Missing run data")
        return
    
    # Compute averages
    min_len = min(min(len(c) for c in on_curves), min(len(c) for c in off_curves))
    
    on_avg = []
    off_avg = []
    for i in range(min_len):
        on_vals = [c[i] for c in on_curves if i < len(c)]
        off_vals = [c[i] for c in off_curves if i < len(c)]
        on_avg.append(sum(on_vals) / len(on_vals))
        off_avg.append(sum(off_vals) / len(off_vals))
    
    # Detect stairs
    on_stairs = []
    for curve in on_curves:
        on_stairs.extend(detect_stairs(curve))
    off_stairs = []
    for curve in off_curves:
        off_stairs.extend(detect_stairs(curve))
    
    on_final = on_avg[-1] if on_avg else 0
    off_final = off_avg[-1] if off_avg else 0
    on_peak = max(on_avg) if on_avg else 0
    off_peak = max(off_avg) if off_avg else 0
    
    report = f"""# F-NOVELTY Ablation: Novelty Reward On vs Off

## Experimental Setup
- **Population**: 24 agents, NEAT brains, coevolution mode
- **Generations**: 200, Episode ticks: 200
- **Seeds**: 3 per mode (42, 43, 44)
- **Key difference**: `novelty_enabled = true` (weight=5.0) vs `novelty_enabled = false`

## Results

| Metric | Novelty ON | Novelty OFF |
|--------|-----------|-------------|
| Final avg best_fitness | {on_final:.1f} | {off_final:.1f} |
| Peak avg best_fitness | {on_peak:.1f} | {off_peak:.1f} |
| Staircase patterns detected | {len(on_stairs)} | {len(off_stairs)} |

## Fitness Curve (sampled every 25 gen)

| Gen | Novelty ON | Novelty OFF |
|-----|-----------|-------------|
"""
    for i in range(0, min_len, 25):
        report += f"| {i} | {on_avg[i]:.1f} | {off_avg[i]:.1f} |\n"
    report += f"| {min_len-1} | {on_avg[-1]:.1f} | {off_avg[-1]:.1f} |\n"
    
    report += f"""
## Analysis

"""
    if len(on_stairs) > len(off_stairs):
        report += f"**Novelty ON shows more staircase patterns** ({len(on_stairs)} vs {len(off_stairs)}), "
        report += "suggesting that novelty-driven exploration creates more distinct fitness plateaus followed by innovation jumps.\n"
    elif len(off_stairs) > len(on_stairs):
        report += f"**Novelty OFF shows more staircase patterns** ({len(off_stairs)} vs {len(on_stairs)}). "
        report += "This may indicate that without novelty pressure, evolution gets stuck in local optima longer.\n"
    else:
        report += f"Both modes show similar staircase counts ({len(on_stairs)} vs {len(off_stairs)}).\n"
    
    if on_final > off_final * 1.05:
        report += f"\nNovelty ON achieves higher final fitness ({on_final:.1f} vs {off_final:.1f}).\n"
    elif off_final > on_final * 1.05:
        report += f"\nNovelty OFF achieves higher final fitness ({off_final:.1f} vs {on_final:.1f}).\n"
    
    report += "\n## Conclusion\n"
    if on_final > off_final * 1.05 or len(on_stairs) > len(off_stairs):
        report += "Novelty reward shows measurable benefit, supporting F-NOVELTY's role in maintaining exploration pressure.\n"
    else:
        report += "Novelty reward does not show significant benefit under current parameters. "
        report += "The coevolution dynamics may already provide sufficient exploration pressure.\n"
    
    out_dir = Path(r"D:\claude-code\c++\docs\experiments")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "F-NOVELTY_ablation.md"
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(report)
    print(f"Report written to {out_path}")
    print(f"Novelty ON: final={on_final:.1f}, stairs={len(on_stairs)}")
    print(f"Novelty OFF: final={off_final:.1f}, stairs={len(off_stairs)}")

if __name__ == "__main__":
    main()
