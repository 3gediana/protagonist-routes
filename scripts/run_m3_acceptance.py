#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import shutil
import subprocess
import time
import tomllib
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

from plot_fitness import FitnessSeries, load_fitness_series, plot_series_collection


@dataclass(frozen=True)
class EvolutionPoint:
    generation: int
    min_fitness: float
    avg_fitness: float
    best_fitness: float
    species_count: int
    avg_complexity: float


@dataclass(frozen=True)
class EvolutionSeries:
    label: str
    csv_path: Path
    points: tuple[EvolutionPoint, ...]


@dataclass(frozen=True)
class RunSummary:
    seed: int
    run_dir: Path
    config_path: Path
    fitness_csv: Path
    evolution_csv: Path
    generations_recorded: int
    first_avg_fitness: float
    final_avg_fitness: float
    avg_multiplier: float
    peak_best_fitness: float
    final_best_fitness: float
    peak_species_count: int
    final_species_count: int
    peak_avg_complexity: float
    final_avg_complexity: float
    duration_seconds: float


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    repo_root = repo_root_from_script()
    parser = argparse.ArgumentParser(description="Run Neural Eco M3 acceptance experiments")
    parser.add_argument(
        "--exe",
        type=Path,
        default=repo_root / "build" / "bin" / "Release" / "neural-eco.exe",
        help="Path to neural-eco executable",
    )
    parser.add_argument(
        "--base-config",
        type=Path,
        default=repo_root / "configs" / "m3_acceptance.toml",
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
        default=f"m3_acceptance_{time.strftime('%Y%m%d-%H%M%S')}",
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
        default=200,
        help="Override evolution.generations",
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
        "--compatibility-threshold",
        type=float,
        default=None,
        help="Optional override for evolution.speciation.compatibility_threshold",
    )
    parser.add_argument(
        "--add-node-rate",
        type=float,
        default=None,
        help="Optional override for evolution.mutation.add_node_rate",
    )
    parser.add_argument(
        "--add-connection-rate",
        type=float,
        default=None,
        help="Optional override for evolution.mutation.add_connection_rate",
    )
    parser.add_argument(
        "--toggle-connection-rate",
        type=float,
        default=None,
        help="Optional override for evolution.mutation.toggle_connection_rate",
    )
    parser.add_argument(
        "--log-level",
        type=str,
        default="error",
        help="Override runtime.log_level for batch experiments",
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
        return repr(value)
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


def latest_csv(seed_output_dir: Path, name: str) -> Path:
    candidates = sorted(seed_output_dir.rglob(name), key=lambda path: path.stat().st_mtime)
    if not candidates:
        raise FileNotFoundError(f"No {name} found under {seed_output_dir}")
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
    compatibility_threshold: float | None,
    add_node_rate: float | None,
    add_connection_rate: float | None,
    toggle_connection_rate: float | None,
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

    mutation = ensure_nested_table(evolution, "mutation")
    if add_node_rate is not None:
        mutation["add_node_rate"] = add_node_rate
    if add_connection_rate is not None:
        mutation["add_connection_rate"] = add_connection_rate
    if toggle_connection_rate is not None:
        mutation["toggle_connection_rate"] = toggle_connection_rate

    speciation = ensure_nested_table(evolution, "speciation")
    speciation["enabled"] = True
    if compatibility_threshold is not None:
        speciation["compatibility_threshold"] = compatibility_threshold

    brain = ensure_nested_table(config, "brain")
    brain["type"] = "neat"
    ctrnn = ensure_nested_table(brain, "ctrnn")
    ctrnn["enabled"] = True

    output = ensure_nested_table(config, "output")
    output["runs_dir"] = seed_output_dir.as_posix()

    return config


def load_evolution_series(csv_path: Path, label: str) -> EvolutionSeries:
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        points: list[EvolutionPoint] = []
        for row in reader:
            points.append(
                EvolutionPoint(
                    generation=int(row["generation"]),
                    min_fitness=float(row["min_fitness"]),
                    avg_fitness=float(row["avg_fitness"]),
                    best_fitness=float(row["best_fitness"]),
                    species_count=int(float(row["species_count"])),
                    avg_complexity=float(row["avg_complexity"]),
                )
            )
    if not points:
        raise ValueError(f"CSV has no data rows: {csv_path}")
    return EvolutionSeries(label=label, csv_path=csv_path, points=tuple(points))


def compute_run_summary(
    seed: int,
    run_dir: Path,
    config_path: Path,
    fitness_csv: Path,
    evolution_csv: Path,
    duration_seconds: float,
) -> RunSummary:
    fitness_series = load_fitness_series(fitness_csv, label=f"seed_{seed}")
    evolution_series = load_evolution_series(evolution_csv, label=f"seed_{seed}")
    first = evolution_series.points[0]
    last = evolution_series.points[-1]
    peak_best = max(point.best_fitness for point in evolution_series.points)
    peak_species = max(point.species_count for point in evolution_series.points)
    peak_complexity = max(point.avg_complexity for point in evolution_series.points)
    multiplier = last.avg_fitness / first.avg_fitness if first.avg_fitness != 0 else float("inf")
    return RunSummary(
        seed=seed,
        run_dir=run_dir,
        config_path=config_path,
        fitness_csv=fitness_csv,
        evolution_csv=evolution_csv,
        generations_recorded=len(fitness_series.points),
        first_avg_fitness=first.avg_fitness,
        final_avg_fitness=last.avg_fitness,
        avg_multiplier=multiplier,
        peak_best_fitness=peak_best,
        final_best_fitness=last.best_fitness,
        peak_species_count=peak_species,
        final_species_count=last.species_count,
        peak_avg_complexity=peak_complexity,
        final_avg_complexity=last.avg_complexity,
        duration_seconds=duration_seconds,
    )


def run_single_experiment(exe_path: Path, config_path: Path, seed_output_dir: Path) -> tuple[Path, Path, Path, float]:
    start = time.perf_counter()
    command = [str(exe_path), "--mode", "headless", "--config", str(config_path)]
    subprocess.run(command, check=True, cwd=repo_root_from_script())
    duration_seconds = time.perf_counter() - start
    run_dir = latest_run_dir(seed_output_dir)
    fitness_csv = latest_csv(seed_output_dir, "fitness.csv")
    evolution_csv = latest_csv(seed_output_dir, "evolution.csv")
    return run_dir, fitness_csv, evolution_csv, duration_seconds


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
                "evolution_csv",
                "generations_recorded",
                "first_avg_fitness",
                "final_avg_fitness",
                "avg_multiplier",
                "peak_best_fitness",
                "final_best_fitness",
                "peak_species_count",
                "final_species_count",
                "peak_avg_complexity",
                "final_avg_complexity",
                "duration_seconds",
            ],
        )
        writer.writeheader()
        for summary in summaries:
            row = asdict(summary)
            row["run_dir"] = str(summary.run_dir)
            row["config_path"] = str(summary.config_path)
            row["fitness_csv"] = str(summary.fitness_csv)
            row["evolution_csv"] = str(summary.evolution_csv)
            writer.writerow(row)


def write_report(path: Path, summaries: Sequence[RunSummary], aggregate_artifacts: dict[str, Path | None]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    mean_multiplier = sum(summary.avg_multiplier for summary in summaries) / len(summaries)
    mean_final_species = sum(summary.final_species_count for summary in summaries) / len(summaries)
    mean_final_complexity = sum(summary.final_avg_complexity for summary in summaries) / len(summaries)

    lines = [
        "# M3 Acceptance Report",
        "",
        f"- Runs: {len(summaries)}",
        f"- Mean multiplier: {mean_multiplier:.3f}",
        f"- Mean final species count: {mean_final_species:.3f}",
        f"- Mean final avg complexity: {mean_final_complexity:.3f}",
        "",
        "## Per-run summary",
        "",
        "| Seed | First Avg | Final Avg | Multiplier | Peak Species | Final Species | Peak Complexity | Final Complexity | Duration (s) |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]

    for summary in summaries:
        lines.append(
            f"| {summary.seed} | {summary.first_avg_fitness:.3f} | {summary.final_avg_fitness:.3f} | "
            f"{summary.avg_multiplier:.3f} | {summary.peak_species_count} | {summary.final_species_count} | "
            f"{summary.peak_avg_complexity:.3f} | {summary.final_avg_complexity:.3f} | {summary.duration_seconds:.2f} |"
        )

    lines.extend([
        "",
        "## Artifacts",
        "",
        f"- Aggregate fitness plot: `{aggregate_artifacts['fitness_png']}`" if aggregate_artifacts["fitness_png"] else "- Aggregate fitness plot: _(not generated)_",
        f"- Aggregate fitness CSV: `{aggregate_artifacts['fitness_csv']}`" if aggregate_artifacts["fitness_csv"] else "- Aggregate fitness CSV: _(not generated)_",
        f"- Aggregate species plot: `{aggregate_artifacts['species_png']}`" if aggregate_artifacts["species_png"] else "- Aggregate species plot: _(not generated)_",
        f"- Aggregate species CSV: `{aggregate_artifacts['species_csv']}`" if aggregate_artifacts["species_csv"] else "- Aggregate species CSV: _(not generated)_",
        f"- Aggregate complexity plot: `{aggregate_artifacts['complexity_png']}`" if aggregate_artifacts["complexity_png"] else "- Aggregate complexity plot: _(not generated)_",
        f"- Aggregate complexity CSV: `{aggregate_artifacts['complexity_csv']}`" if aggregate_artifacts["complexity_csv"] else "- Aggregate complexity CSV: _(not generated)_",
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
    print(
        f"seed={summary.seed} generations={summary.generations_recorded} "
        f"first_avg={summary.first_avg_fitness:.3f} final_avg={summary.final_avg_fitness:.3f} "
        f"x={summary.avg_multiplier:.3f} peak_species={summary.peak_species_count} "
        f"final_species={summary.final_species_count} peak_complexity={summary.peak_avg_complexity:.3f} "
        f"final_complexity={summary.final_avg_complexity:.3f} duration={summary.duration_seconds:.2f}s"
    )


def build_metric_rows(series_collection: Sequence[EvolutionSeries], attribute: str) -> list[dict[str, float | int]]:
    generations = sorted({point.generation for series in series_collection for point in series.points})
    rows: list[dict[str, float | int]] = []
    for generation in generations:
        values = [getattr(point, attribute) for series in series_collection for point in series.points if point.generation == generation]
        mean_value = sum(values) / len(values)
        rows.append(
            {
                "generation": generation,
                "run_count": len(values),
                "mean_value": mean_value,
                "min_value": min(values),
                "max_value": max(values),
            }
        )
    return rows


def save_metric_aggregate(series_collection: Sequence[EvolutionSeries], attribute: str, title: str, y_label: str, png_path: Path, csv_path: Path) -> tuple[Path, Path]:
    png_path.parent.mkdir(parents=True, exist_ok=True)
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    rows = build_metric_rows(series_collection, attribute)

    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["generation", "run_count", "mean_value", "min_value", "max_value"])
        writer.writeheader()
        writer.writerows(rows)

    figure, axis = plt.subplots(figsize=(11, 6))
    for series in series_collection:
        x = [point.generation for point in series.points]
        y = [getattr(point, attribute) for point in series.points]
        axis.plot(x, y, linewidth=1.1, alpha=0.25, color="#94A3B8")

    generations = [int(row["generation"]) for row in rows]
    mean_values = [float(row["mean_value"]) for row in rows]
    min_values = [float(row["min_value"]) for row in rows]
    max_values = [float(row["max_value"]) for row in rows]

    axis.plot(generations, mean_values, linewidth=2.4, color="#2563EB", label="mean")
    axis.fill_between(generations, min_values, max_values, color="#BFDBFE", alpha=0.35, label="min-max")
    axis.set_title(title)
    axis.set_xlabel("Generation")
    axis.set_ylabel(y_label)
    axis.xaxis.set_major_locator(MaxNLocator(integer=True))
    axis.grid(True, linestyle="--", alpha=0.3)
    axis.legend()
    figure.tight_layout()
    figure.savefig(png_path, dpi=180)
    plt.close(figure)
    return png_path, csv_path


def main() -> int:
    args = parse_args()
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
    fitness_series_collection: list[FitnessSeries] = []
    evolution_series_collection: list[EvolutionSeries] = []

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
            compatibility_threshold=args.compatibility_threshold,
            add_node_rate=args.add_node_rate,
            add_connection_rate=args.add_connection_rate,
            toggle_connection_rate=args.toggle_connection_rate,
            log_level=args.log_level,
        )
        config_path = configs_dir / f"seed_{seed:04d}.toml"
        write_toml_file(config_path, config_data)

        print(f"Running seed {seed}...")
        run_dir, fitness_csv, evolution_csv, duration_seconds = run_single_experiment(
            exe_path=exe_path,
            config_path=config_path,
            seed_output_dir=seed_output_dir,
        )
        summary = compute_run_summary(
            seed=seed,
            run_dir=run_dir,
            config_path=config_path,
            fitness_csv=fitness_csv,
            evolution_csv=evolution_csv,
            duration_seconds=duration_seconds,
        )
        summaries.append(summary)
        fitness_series_collection.append(load_fitness_series(fitness_csv, label=f"seed_{seed}"))
        evolution_series_collection.append(load_evolution_series(evolution_csv, label=f"seed_{seed}"))
        print_run_summary(summary)

    plots_output = plot_series_collection(
        fitness_series_collection,
        aggregate_output_dir=acceptance_root / "plots",
        skip_individual=False,
        title_prefix="Neural Eco M3 Acceptance",
    )

    species_png, species_csv = save_metric_aggregate(
        evolution_series_collection,
        "species_count",
        "Neural Eco M3 Acceptance · Aggregate Species Count",
        "Species Count",
        acceptance_root / "plots" / "aggregate_species_count.png",
        acceptance_root / "plots" / "aggregate_species_count.csv",
    )
    complexity_png, complexity_csv = save_metric_aggregate(
        evolution_series_collection,
        "avg_complexity",
        "Neural Eco M3 Acceptance · Aggregate Average Complexity",
        "Average Complexity",
        acceptance_root / "plots" / "aggregate_avg_complexity.png",
        acceptance_root / "plots" / "aggregate_avg_complexity.csv",
    )

    summary_csv_path = acceptance_root / "acceptance_summary.csv"
    write_summary_csv(summary_csv_path, summaries)

    result_png = copy_best_result_plot(summaries)
    aggregate_png = plots_output["aggregate_png"] if isinstance(plots_output["aggregate_png"], Path) else None
    aggregate_csv = plots_output["aggregate_csv"] if isinstance(plots_output["aggregate_csv"], Path) else None
    report_path = acceptance_root / "acceptance_report.md"
    aggregate_artifacts = {
        "fitness_png": aggregate_png,
        "fitness_csv": aggregate_csv,
        "species_png": species_png,
        "species_csv": species_csv,
        "complexity_png": complexity_png,
        "complexity_csv": complexity_csv,
    }
    write_report(report_path, summaries, aggregate_artifacts)

    metadata_path = acceptance_root / "acceptance_summary.json"
    metadata = {
        "label": args.label,
        "executable": str(exe_path),
        "base_config": str(base_config_path),
        "generations": args.generations,
        "episode_ticks": args.episode_ticks,
        "population_size": args.population_size,
        "resource_count": args.resource_count,
        "intrinsic_weight": args.intrinsic_weight,
        "compatibility_threshold": args.compatibility_threshold,
        "add_node_rate": args.add_node_rate,
        "add_connection_rate": args.add_connection_rate,
        "toggle_connection_rate": args.toggle_connection_rate,
        "seeds": args.seeds,
        "summary_csv": str(summary_csv_path),
        "aggregate_fitness_plot": str(aggregate_png) if aggregate_png else None,
        "aggregate_species_plot": str(species_png),
        "aggregate_complexity_plot": str(complexity_png),
        "best_result_png": str(result_png) if result_png else None,
        "runs": [
            {
                **asdict(summary),
                "run_dir": str(summary.run_dir),
                "config_path": str(summary.config_path),
                "fitness_csv": str(summary.fitness_csv),
                "evolution_csv": str(summary.evolution_csv),
            }
            for summary in summaries
        ],
    }
    metadata_path.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"Saved summary CSV: {summary_csv_path}")
    print(f"Saved report: {report_path}")
    print(f"Saved metadata JSON: {metadata_path}")
    if aggregate_png:
        print(f"Saved aggregate fitness plot: {aggregate_png}")
    print(f"Saved aggregate species plot: {species_png}")
    print(f"Saved aggregate complexity plot: {complexity_png}")
    if result_png:
        print(f"Saved best result plot: {result_png}")
    print("Acceptance status: REPORTED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
