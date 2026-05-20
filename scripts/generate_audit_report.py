#!/usr/bin/env python3
"""Generate emergence audit report from emergence_audit.csv."""
import csv
from pathlib import Path

THRESHOLDS = {
    'signal_behavior_mi_avg': 0.3,
    'modulatory_usage_ratio': 0.3,
    'modulatory_corr': 0.3,
    'pred_prey_fitness_corr': -0.2,
    'heavy_tasks_completed': 5,
    'host_immunity_pathogen_virulence_corr': 0.3,
    'species_dig_preference_var': 0.1,
    'cave_occupancy_ratio': 0.01,
    'hunt_success_rate': 0.1,
}

MODULE_MAP = {
    'signal_behavior_mi_avg': 'F-COMM',
    'modulatory_usage_ratio': 'F-NEUROMOD',
    'modulatory_corr': 'F-NEUROMOD',
    'pred_prey_fitness_corr': 'F-COEVO',
    'heavy_tasks_completed': 'F-COOP',
    'host_immunity_pathogen_virulence_corr': 'F-DISEASE',
    'species_dig_preference_var': 'F-DIG',
    'cave_occupancy_ratio': 'F-DIG',
    'hunt_success_rate': 'F-PRED',
}

def classify(value, threshold, higher_is_better=True):
    if higher_is_better:
        if abs(value) >= abs(threshold):
            return '✅真涌现'
        elif abs(value) >= abs(threshold) * 0.5:
            return '🟡部分涌现'
        elif abs(value) > 0.001:
            return '⚠️弱信号'
        else:
            return '❌空管道'
    else:
        if value <= threshold:
            return '✅真涌现'
        elif value <= 0:
            return '🟡部分涌现'
        else:
            return '❌空管道'

def main():
    data_root = Path(r"D:\claude-code\c++\data\runs")
    
    # Find latest full_validation run
    runs = sorted((data_root / "full_validation").iterdir(), key=lambda p: p.stat().st_mtime, reverse=True)
    if not runs:
        print("No full_validation runs found")
        return
    
    csv_path = runs[0] / "emergence_audit.csv"
    if not csv_path.exists():
        print(f"No emergence_audit.csv in {runs[0]}")
        return
    
    # Read last 50 rows
    rows = []
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    
    last_n = rows[-50:] if len(rows) >= 50 else rows
    
    # Compute averages
    metrics = {}
    for key in THRESHOLDS:
        vals = []
        for row in last_n:
            try:
                vals.append(float(row.get(key, 0)))
            except (ValueError, TypeError):
                pass
        metrics[key] = sum(vals) / len(vals) if vals else 0.0
    
    # Classify each metric
    results = {}
    for key, threshold in THRESHOLDS.items():
        higher = key != 'pred_prey_fitness_corr'  # pred_prey_corr: negative is better
        results[key] = {
            'value': metrics[key],
            'threshold': threshold,
            'classification': classify(metrics[key], threshold, higher),
        }
    
    # Aggregate per module
    module_status = {}
    for key, res in results.items():
        mod = MODULE_MAP.get(key, 'UNKNOWN')
        if mod not in module_status:
            module_status[mod] = []
        module_status[mod].append(res['classification'])
    
    # Determine module-level status
    def module_class(classifications):
        if any('✅' in c for c in classifications):
            if all('✅' in c for c in classifications):
                return '✅✅✅ 完美涌现'
            return '✅✅⛳ 部分涌现'
        elif any('🟡' in c for c in classifications):
            return '✅⛳⛳ 弱涌现'
        elif any('⚠️' in c for c in classifications):
            return '⚠️ 降级'
        else:
            return '❌ 空管道'
    
    # Generate report
    report = """# Emergence Audit Report

## Metrics (last 50 generations average)

| Metric | Value | Threshold | Classification |
|--------|-------|-----------|----------------|
"""
    for key in THRESHOLDS:
        r = results[key]
        report += f"| {key} | {r['value']:.4f} | {r['threshold']} | {r['classification']} |\n"
    
    report += "\n## Module-Level Assessment\n\n"
    report += "| Module | Metrics | Assessment |\n"
    report += "|--------|---------|------------|\n"
    for mod, classifications in sorted(module_status.items()):
        mc = module_class(classifications)
        report += f"| {mod} | {', '.join(classifications)} | {mc} |\n"
    
    report += "\n## Recommendations\n\n"
    for key, r in results.items():
        if '❌' in r['classification'] or '⚠️' in r['classification']:
            report += f"- **{key}**: Current value {r['value']:.4f} below threshold {r['threshold']}. "
            if key == 'signal_behavior_mi_avg':
                report += "Consider increasing backprop_factor or backprop_window.\n"
            elif key == 'modulatory_usage_ratio':
                report += "Consider increasing add_modulatory_rate in mutation config.\n"
            elif key == 'heavy_tasks_completed':
                report += "Consider reducing required_workers or required_ticks.\n"
            else:
                report += "May need parameter tuning or longer evolution.\n"
    
    out_dir = Path(r"D:\claude-code\c++\docs\experiments")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "AUDIT_REPORT.md"
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(report)
    print(f"Report written to {out_path}")

if __name__ == "__main__":
    main()
