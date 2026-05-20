#!/usr/bin/env python3
"""Detect fitness staircase patterns in evolution.csv."""
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
    """Detect staircase patterns: plateau (change < 5% for >= N gens) then jump (>= 5%)."""
    stairs = []
    i = 0
    while i < len(fitness):
        # Look for plateau
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
            # Check if next step is a jump
            if fitness[j] != 0:
                jump_pct_actual = (fitness[j+1] - fitness[j]) / max(abs(fitness[j]), 1e-6)
                if jump_pct_actual >= jump_pct:
                    stairs.append({
                        'plateau_start': i,
                        'plateau_end': j,
                        'plateau_length': plateau_length,
                        'plateau_fitness': fitness[j],
                        'jump_gen': j + 1,
                        'jump_fitness': fitness[j+1],
                        'jump_pct': jump_pct_actual * 100
                    })
        i = j + 1
    return stairs

def main():
    if len(sys.argv) < 2:
        print("Usage: detect_fitness_stairs.py <evolution.csv> [plateau_len] [jump_pct]")
        sys.exit(1)
    
    csv_path = Path(sys.argv[1])
    plateau_len = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    jump_pct = float(sys.argv[3]) if len(sys.argv) > 3 else 0.05
    
    fitness = read_best_fitness(csv_path)
    stairs = detect_stairs(fitness, plateau_len, jump_pct)
    
    print(f"Total generations: {len(fitness)}")
    print(f"Staircases detected: {len(stairs)}")
    print(f"Settings: plateau_len>={plateau_len}, jump_pct>={jump_pct*100:.0f}%")
    print()
    
    if stairs:
        print(f"{'#':>3} {'PlatStart':>10} {'PlatEnd':>10} {'PlatLen':>8} {'PlatFit':>10} {'JumpGen':>10} {'JumpFit':>10} {'Jump%':>8}")
        for i, s in enumerate(stairs):
            print(f"{i:3d} {s['plateau_start']:10d} {s['plateau_end']:10d} {s['plateau_length']:8d} "
                  f"{s['plateau_fitness']:10.1f} {s['jump_gen']:10d} {s['jump_fitness']:10.1f} "
                  f"{s['jump_pct']:7.1f}%")
    else:
        print("No staircase patterns detected.")
    
    # Summary stats
    if fitness:
        print(f"\nFitness range: {min(fitness):.1f} - {max(fitness):.1f}")
        print(f"Final fitness: {fitness[-1]:.1f}")

if __name__ == "__main__":
    main()
