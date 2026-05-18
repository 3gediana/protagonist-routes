#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import shutil
import subprocess
import sys
import time
import tomllib
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence

from plot_fitness import FitnessSeries, load_fitness_series, plot_series_collection


@dataclass(frozen=True)
class RunSummary:
    seed: int
    run_dir: Path
    config_path: Path
    fitness_csv: Path
    generations_recorded: int
    first_avg_fitness: float
    final_avg_fitness: float
    avg_multiplier: float
    peak_best_fitness: float
    final_best_fitness: float
    passed_target_rule: bool | None
    duration_seconds: float


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    repo_root = repo_root_from_script()
    parser = argparse.ArgumentParser(description="Run Neural Eco M1 formal acceptance experiments")
    parser.add_argument(
        "--exe",
        type=Path,
        default=repo_root / "build" / "bin" / "Release" / "neural-eco.exe",
        help="Path to neural-eco executable",
    )
    parser.add_argument(
        "--base-config",
        type=Path,
        default=repo_root / "configs" / "default.toml",
        help="Base TOML config to clone and override per seed",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=repo_root / "data" / "runs",
        help="Directory under which acceptance runs are stored",
    )
    parser.add_argument(
        "--label",
        type=str,
        default=f"m1_acceptance_{time.strftime('%Y%m%d-%H%M%S')}",
        help="Folder name for this acceptance batch",
    )
    parser.add_argument(
        "--seeds",
        type=int,
        nargs="+",
        default=[42, 43, 44, 45, 46],
        help="Random seeds to run",
    )
    parser.add_argument(
        "--generations",
        type=int,
        default=100,
        help="Override evolution.generations",
    )
    parser.add_argument(
        "--target-multiplier",
        type=float,
        default=None,
        help="Optional acceptance threshold for final_avg_fitness / first_avg_fitness",
    )
    parser.add_argument(
        "--episode-ticks",
        type=int,
        default=None,
        help="Optional override for evolution.episode_ticks",
    )
    parser.add_argument(
        "--population-size",
        type=int,
        default=None,
        help="Optional override for population.size",
    )
    parser.add_argument(
        "--resource-count",
        type=int,
        default=None,
        help="Optional override for world.resources.count",
    )
    parser.add_argument(
        "--intrinsic-weight",
        type=float,
        default=None,
        help="Optional override for reward.intrinsic_weight",
    )
    parser.add_argument(
        "--survival-weight",
        type=float,
        default=None,
        help="Optional override for reward.survival_weight",
    )
    parser.add_argument(
        "--food-weight",
        type=float,
        default=None,
        help="Optional override for reward.food_weight",
    )
    parser.add_argument(
        "--base-metabolism",
        type=float,
        default=None,
        help="Optional override for energy.base_metabolism",
    )
    parser.add_argument(
        "--initial-energy",
        type=float,
        default=None,
        help="Optional override for energy.initial",
    )
    parser.add_argument(
        "--log-level",
        type=str,
        default="error",
        help="Override runtime.log_level for batch experiments",
    )
    parser.add_argument(
        "--mode",
        type=str,
        default="headless",
        choices=["headless"],
        help="Runtime mode to use for acceptance",
    )
    return parser.parse_args()


def format_toml_value(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        return f'"{escaped}"'
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        text = repr(value)
        return text
    if isinstance(value, list):
        return "[" + ", ".join(format_toml_value(item) for item in value) + "]"
    raise TypeError(f"Unsupported TOML value type: {type(value)!r}")


def dump_toml_table(data: dict[str, Any], prefix: tuple[str, ...] = ()) -> list[str]:
    scalar_lines: list[str] = []
    nested_lines: list[str] = []

    for key, value in data.items():
        if isinstance(value, dict):
            section_name = ".".join((*prefix, key))
            nested_lines.append(f"[{section_name}]")
            nested_lines.extend(dump_toml_table(value, (*prefix, key)))
            nested_lines.append("")
        else:
            scalar_lines.append(f"{key} = {format_toml_value(value)}")

    lines = scalar_lines[:]
    if scalar_lines and nested_lines:
        lines.append("")
    lines.extend(nested_lines)
    return lines


def write_toml_file(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = dump_toml_table(data)
    path.write_text("\n".join(lines).strip() + "\n", encoding="utf-8")


def ensure_nested_table(root: dict[str, Any], key: str) -> dict[str, Any]:
    value = root.get(key)
    if not isinstance(value, dict):
        value = {}
        root[key] = value
    return value


def latest_fitness_csv(seed_output_dir: Path) -> Path:
    candidates = sorted(seed_output_dir.rglob("fitness.csv"), key=lambda path: path.stat().st_mtime)
    if not candidates:
        raise FileNotFoundError(f"No fitness.csv found under {seed_output_dir}")
    return candidates[-1]


def latest_run_dir(seed_output_dir: Path) -> Path:
    candidates = sorted((path for path in seed_output_dir.iterdir() if path.is_dir()), key=lambda path: path.stat().st_mtime)
    if not candidates:
        raise FileNotFoundError(f"No run directory created under {seed_output_dir}")
    return candidates[-1]


def build_run_config(
    base_config: dict[str, Any],
    seed: int,
    seed_output_dir: Path,
    generations: int,
    episode_ticks: int | None,
    population_size: int | None,
    resource_count: int | None,
    intrinsic_weight: float | None,
    survival_weight: float | None,
    food_weight: float | None,
    base_metabolism: float | None,
    initial_energy: float | None,
    log_level: str,
) -> dict[str, Any]:
    config = json.loads(json.dumps(base_config))

    runtime = ensure_nested_table(config, "runtime")
    runtime["mode"] = "headless"
    runtime["random_seed"] = seed
    runtime["log_level"] = log_level

    evolution = ensure_nested_table(config, "evolution")
    evolution["generations"] = generations
    if episode_ticks is not None:
        evolution["episode_ticks"] = episode_ticks

    population = ensure_nested_table(config, "population")
    if population_size is not None:
        population["size"] = population_size

    world = ensure_nested_table(config, "world")
    resources = ensure_nested_table(world, "resources")
    if resource_count is not None:
        resources["count"] = resource_count

    reward = ensure_nested_table(config, "reward")
    if intrinsic_weight is not None:
        reward["intrinsic_weight"] = intrinsic_weight
    if survival_weight is not None:
        reward["survival_weight"] = survival_weight
    if food_weight is not None:
        reward["food_weight"] = food_weight

    energy = ensure_nested_table(config, "energy")
    if base_metabolism is not None:
        energy["base_metabolism"] = base_metabolism
    if initial_energy is not None:
        energy["initial"] = initial_energy

    output = ensure_nested_table(config, "output")
    output["runs_dir"] = seed_output_dir.as_posix()

    return config


def compute_run_summary(
    seed: int,
    run_dir: Path,
    config_path: Path,
    fitness_csv: Path,
    duration_seconds: float,
    target_multiplier: float | None,
) -> RunSummary:
    series = load_fitness_series(fitness_csv, label=f"seed_{seed}")
    first = series.points[0]
    final = series.points[-1]
    peak_best = max(point.best_fitness for point in series.points)
    multiplier = final.avg_fitness / first.avg_fitness if first.avg_fitness != 0 else float("inf")
    passed_target_rule = multiplier >= target_multiplier if target_multiplier is not None else None
    return RunSummary(
        seed=seed,
        run_dir=run_dir,
        config_path=config_path,
        fitness_csv=fitness_csv,
        generations_recorded=len(series.points),
        first_avg_fitness=first.avg_fitness,
        final_avg_fitness=final.avg_fitness,
        avg_multiplier=multiplier,
        peak_best_fitness=peak_best,
        final_best_fitness=final.best_fitness,
        passed_target_rule=passed_target_rule,
        duration_seconds=duration_seconds,
    )


def run_single_experiment(
    exe_path: Path,
    config_path: Path,
    seed_output_dir: Path,
) -> tuple[Path, Path, float]:
    start = time.perf_counter()
    command = [str(exe_path), "--mode", "headless", "--config", str(config_path)]
    subprocess.run(command, check=True, cwd=repo_root_from_script())
    duration_seconds = time.perf_counter() - start
    run_dir = latest_run_dir(seed_output_dir)
    fitness_csv = latest_fitness_csv(seed_output_dir)
    return run_dir, fitness_csv, duration_seconds


def write_summary_csv(path: Path, summaries: Sequence[RunSummary]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "seed",
                "run_dir",
                "config_path",
                "fitness_csv",
                "generations_recorded",
                "first_avg_fitness",
                "final_avg_fitness",
                "avg_multiplier",
                "peak_best_fitness",
                "final_best_fitness",
                "passed_target_rule",
                "duration_seconds",
            ],
        )
        writer.writeheader()
        for summary in summaries:
            row = asdict(summary)
            row["run_dir"] = str(summary.run_dir)
            row["config_path"] = str(summary.config_path)
            row["fitness_csv"] = str(summary.fitness_csv)
            writer.writerow(row)


def write_report(
    path: Path,
    summaries: Sequence[RunSummary],
    aggregate_png: Path | None,
    aggregate_csv: Path | None,
    target_multiplier: float | None,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    mean_multiplier = sum(summary.avg_multiplier for summary in summaries) / len(summaries)
    lines = [
        "# M1 Acceptance Report",
        "",
        f"- Runs: {len(summaries)}",
        f"- Mean multiplier: {mean_multiplier:.3f}",
    ]

    if target_multiplier is not None:
        pass_count = sum(1 for summary in summaries if summary.passed_target_rule is True)
        lines.append(f"- Target multiplier: {target_multiplier:.3f}")
        lines.append(f"- Passed target: {pass_count}/{len(summaries)}")

    lines.extend([
        "",
        "## Per-run summary",
        "",
        "| Seed | First Avg | Final Avg | Multiplier | Peak Best | Duration (s) | Target |",
        "|---|---:|---:|---:|---:|---:|---|",
    ])

    for summary in summaries:
        if summary.passed_target_rule is None:
            target_status = "n/a"
        else:
            target_status = "yes" if summary.passed_target_rule else "no"
        lines.append(
            f"| {summary.seed} | {summary.first_avg_fitness:.3f} | {summary.final_avg_fitness:.3f} | "
            f"{summary.avg_multiplier:.3f} | {summary.peak_best_fitness:.3f} | {summary.duration_seconds:.2f} | "
            f"{target_status} |"
        )

    lines.extend([
        "",
        "## Artifacts",
        "",
        f"- Aggregate plot: `{aggregate_png}`" if aggregate_png else "- Aggregate plot: _(not generated)_",
        f"- Aggregate CSV: `{aggregate_csv}`" if aggregate_csv else "- Aggregate CSV: _(not generated)_",
    ])

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def copy_best_result_plot(summaries: Sequence[RunSummary]) -> Path | None:
    if not summaries:
        return None
    best = max(summaries, key=lambda item: (item.avg_multiplier, item.final_avg_fitness, item.peak_best_fitness))
    source_plot = best.run_dir / "plots" / "fitness.png"
    if not source_plot.exists():
        return None
    destination = best.run_dir / "result.png"
    shutil.copyfile(source_plot, destination)
    return destination


def print_run_summary(summary: RunSummary) -> None:
    if summary.passed_target_rule is None:
        target_status = "n/a"
    else:
        target_status = "yes" if summary.passed_target_rule else "no"
    print(
        f"seed={summary.seed} generations={summary.generations_recorded} "
        f"first_avg={summary.first_avg_fitness:.3f} final_avg={summary.final_avg_fitness:.3f} "
        f"x={summary.avg_multiplier:.3f} peak_best={summary.peak_best_fitness:.3f} "
        f"target={target_status} duration={summary.duration_seconds:.2f}s"
    )


def main() -> int:
    args = parse_args()
    repo_root = repo_root_from_script()
    exe_path = args.exe.resolve()
    base_config_path = args.base_config.resolve()

    if not exe_path.exists():
        raise FileNotFoundError(f"Executable not found: {exe_path}")
    if not base_config_path.exists():
        raise FileNotFoundError(f"Base config not found: {base_config_path}")

    with base_config_path.open("rb") as handle:
        base_config = tomllib.load(handle)

    acceptance_root = args.output_root.resolve() / args.label
    configs_dir = acceptance_root / "configs"
    configs_dir.mkdir(parents=True, exist_ok=True)

    summaries: list[RunSummary] = []
    series_collection: list[FitnessSeries] = []

    print(f"Acceptance root: {acceptance_root}")
    for seed in args.seeds:
        seed_output_dir = acceptance_root / f"seed_{seed:04d}"
        config_data = build_run_config(
            base_config=base_config,
            seed=seed,
            seed_output_dir=seed_output_dir,
            generations=args.generations,
            episode_ticks=args.episode_ticks,
            population_size=args.population_size,
            resource_count=args.resource_count,
            intrinsic_weight=args.intrinsic_weight,
            survival_weight=args.survival_weight,
            food_weight=args.food_weight,
            base_metabolism=args.base_metabolism,
            initial_energy=args.initial_energy,
            log_level=args.log_level,
        )
        config_path = configs_dir / f"seed_{seed:04d}.toml"
        write_toml_file(config_path, config_data)

        print(f"Running seed {seed}...")
        run_dir, fitness_csv, duration_seconds = run_single_experiment(
            exe_path=exe_path,
            config_path=config_path,
            seed_output_dir=seed_output_dir,
        )
        summary = compute_run_summary(
            seed,
            run_dir,
            config_path,
            fitness_csv,
            duration_seconds,
            args.target_multiplier,
        )
        summaries.append(summary)
        series_collection.append(load_fitness_series(fitness_csv, label=f"seed_{seed}"))
        print_run_summary(summary)

    plots_output = plot_series_collection(
        series_collection,
        aggregate_output_dir=acceptance_root / "plots",
        skip_individual=False,
        title_prefix="Neural Eco M1 Acceptance",
    )

    summary_csv_path = acceptance_root / "acceptance_summary.csv"
    write_summary_csv(summary_csv_path, summaries)

    result_png = copy_best_result_plot(summaries)
    aggregate_png = plots_output["aggregate_png"] if isinstance(plots_output["aggregate_png"], Path) else None
    aggregate_csv = plots_output["aggregate_csv"] if isinstance(plots_output["aggregate_csv"], Path) else None
    report_path = acceptance_root / "acceptance_report.md"
    write_report(report_path, summaries, aggregate_png, aggregate_csv, args.target_multiplier)

    metadata_path = acceptance_root / "acceptance_summary.json"
    metadata = {
        "label": args.label,
        "executable": str(exe_path),
        "base_config": str(base_config_path),
        "generations": args.generations,
        "target_multiplier": args.target_multiplier,
        "episode_ticks": args.episode_ticks,
        "population_size": args.population_size,
        "resource_count": args.resource_count,
        "intrinsic_weight": args.intrinsic_weight,
        "survival_weight": args.survival_weight,
        "food_weight": args.food_weight,
        "base_metabolism": args.base_metabolism,
        "initial_energy": args.initial_energy,
        "seeds": args.seeds,
        "summary_csv": str(summary_csv_path),
        "aggregate_plot": str(aggregate_png) if aggregate_png else None,
        "aggregate_csv": str(aggregate_csv) if aggregate_csv else None,
        "best_result_png": str(result_png) if result_png else None,
        "runs": [
            {
                **asdict(summary),
                "run_dir": str(summary.run_dir),
                "config_path": str(summary.config_path),
                "fitness_csv": str(summary.fitness_csv),
            }
            for summary in summaries
        ],
    }
    metadata_path.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"Saved summary CSV: {summary_csv_path}")
    print(f"Saved report: {report_path}")
    print(f"Saved metadata JSON: {metadata_path}")
    if aggregate_png:
        print(f"Saved aggregate plot: {aggregate_png}")
    if result_png:
        print(f"Saved best result plot: {result_png}")

    if args.target_multiplier is None:
        print("Acceptance status: REPORTED (no fixed target threshold)")
        return 0

    pass_count = sum(1 for summary in summaries if summary.passed_target_rule is True)
    if pass_count == len(summaries):
        print("Acceptance status: PASS")
        return 0

    print(f"Acceptance status: PARTIAL ({pass_count}/{len(summaries)} runs satisfied target multiplier)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
