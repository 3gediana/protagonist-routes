#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import statistics
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator


@dataclass(frozen=True)
class FitnessPoint:
    generation: int
    min_fitness: float
    avg_fitness: float
    best_fitness: float


@dataclass(frozen=True)
class FitnessSeries:
    label: str
    csv_path: Path
    points: tuple[FitnessPoint, ...]


def infer_label(csv_path: Path) -> str:
    parent = csv_path.parent.name
    grandparent = csv_path.parent.parent.name if csv_path.parent.parent != csv_path.parent else ""
    if grandparent.startswith("seed_"):
        return grandparent
    return parent or csv_path.stem


def discover_fitness_csvs(inputs: Sequence[Path]) -> list[Path]:
    discovered: list[Path] = []
    for raw_path in inputs:
        path = raw_path.resolve()
        if path.is_file():
            if path.name != "fitness.csv":
                raise ValueError(f"Expected a fitness.csv file, got: {path}")
            discovered.append(path)
            continue

        if not path.is_dir():
            raise FileNotFoundError(f"Input path does not exist: {path}")

        direct_csv = path / "fitness.csv"
        if direct_csv.exists():
            discovered.append(direct_csv.resolve())
            continue

        discovered.extend(sorted(candidate.resolve() for candidate in path.rglob("fitness.csv")))

    unique: list[Path] = []
    seen: set[Path] = set()
    for candidate in discovered:
        if candidate not in seen:
            unique.append(candidate)
            seen.add(candidate)

    if not unique:
        raise FileNotFoundError("No fitness.csv files were found in the provided inputs")

    return unique


def load_fitness_series(csv_path: Path, label: str | None = None) -> FitnessSeries:
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        points: list[FitnessPoint] = []
        for row in reader:
            points.append(
                FitnessPoint(
                    generation=int(row["generation"]),
                    min_fitness=float(row["min_fitness"]),
                    avg_fitness=float(row["avg_fitness"]),
                    best_fitness=float(row["best_fitness"]),
                )
            )

    if not points:
        raise ValueError(f"CSV has no data rows: {csv_path}")

    return FitnessSeries(label=label or infer_label(csv_path), csv_path=csv_path, points=tuple(points))


def default_plot_dir(series: FitnessSeries) -> Path:
    return series.csv_path.parent / "plots"


def save_single_plot(series: FitnessSeries, output_path: Path, title: str | None = None) -> Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)

    generations = [point.generation for point in series.points]
    min_values = [point.min_fitness for point in series.points]
    avg_values = [point.avg_fitness for point in series.points]
    best_values = [point.best_fitness for point in series.points]

    figure, axis = plt.subplots(figsize=(10, 6))
    axis.plot(generations, min_values, label="min", color="#4B5563", linewidth=1.5)
    axis.plot(generations, avg_values, label="avg", color="#2563EB", linewidth=2.0)
    axis.plot(generations, best_values, label="best", color="#16A34A", linewidth=2.0)
    axis.set_title(title or f"Fitness Curve · {series.label}")
    axis.set_xlabel("Generation")
    axis.set_ylabel("Fitness")
    axis.xaxis.set_major_locator(MaxNLocator(integer=True))
    axis.grid(True, linestyle="--", alpha=0.3)
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=180)
    plt.close(figure)
    return output_path


def build_aggregate_rows(series_collection: Sequence[FitnessSeries]) -> list[dict[str, float | int]]:
    by_generation: dict[int, list[float]] = {}
    for series in series_collection:
        for point in series.points:
            by_generation.setdefault(point.generation, []).append(point.avg_fitness)

    aggregate_rows: list[dict[str, float | int]] = []
    for generation in sorted(by_generation):
        values = by_generation[generation]
        mean_value = statistics.fmean(values)
        std_value = statistics.stdev(values) if len(values) > 1 else 0.0
        aggregate_rows.append(
            {
                "generation": generation,
                "run_count": len(values),
                "mean_avg_fitness": mean_value,
                "std_avg_fitness": std_value,
                "min_avg_fitness": min(values),
                "max_avg_fitness": max(values),
            }
        )

    return aggregate_rows


def common_series_root(series_collection: Sequence[FitnessSeries]) -> Path:
    parents = [str(series.csv_path.parent) for series in series_collection]
    return Path(os.path.commonpath(parents))


def save_aggregate_plot(
    series_collection: Sequence[FitnessSeries],
    output_path: Path,
    aggregate_csv_path: Path,
    title: str | None = None,
) -> tuple[Path, Path]:
    if len(series_collection) < 2:
        raise ValueError("Aggregate plot requires at least two fitness series")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    aggregate_csv_path.parent.mkdir(parents=True, exist_ok=True)

    aggregate_rows = build_aggregate_rows(series_collection)

    with aggregate_csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "generation",
                "run_count",
                "mean_avg_fitness",
                "std_avg_fitness",
                "min_avg_fitness",
                "max_avg_fitness",
            ],
        )
        writer.writeheader()
        writer.writerows(aggregate_rows)

    figure, axis = plt.subplots(figsize=(11, 6))
    for series in series_collection:
        generations = [point.generation for point in series.points]
        avg_values = [point.avg_fitness for point in series.points]
        axis.plot(generations, avg_values, linewidth=1.1, alpha=0.25, color="#94A3B8")

    generations = [int(row["generation"]) for row in aggregate_rows]
    mean_values = [float(row["mean_avg_fitness"]) for row in aggregate_rows]
    std_values = [float(row["std_avg_fitness"]) for row in aggregate_rows]
    lower = [mean - std for mean, std in zip(mean_values, std_values)]
    upper = [mean + std for mean, std in zip(mean_values, std_values)]

    axis.plot(generations, mean_values, linewidth=2.4, color="#2563EB", label="mean avg fitness")
    axis.fill_between(generations, lower, upper, color="#93C5FD", alpha=0.35, label="±1 std")
    axis.set_title(title or "Aggregate Average Fitness")
    axis.set_xlabel("Generation")
    axis.set_ylabel("Average Fitness")
    axis.xaxis.set_major_locator(MaxNLocator(integer=True))
    axis.grid(True, linestyle="--", alpha=0.3)
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=180)
    plt.close(figure)

    return output_path, aggregate_csv_path


def plot_series_collection(
    series_collection: Sequence[FitnessSeries],
    aggregate_output_dir: Path | None = None,
    skip_individual: bool = False,
    title_prefix: str | None = None,
) -> dict[str, object]:
    outputs: dict[str, object] = {
        "individual": [],
        "aggregate_png": None,
        "aggregate_csv": None,
    }

    if not skip_individual:
        individual_outputs: list[Path] = []
        for series in series_collection:
            output_path = default_plot_dir(series) / "fitness.png"
            title = f"{title_prefix} · {series.label}" if title_prefix else None
            individual_outputs.append(save_single_plot(series, output_path, title=title))
        outputs["individual"] = individual_outputs

    if len(series_collection) >= 2:
        base_dir = aggregate_output_dir or common_series_root(series_collection) / "plots"
        aggregate_png = base_dir / "aggregate_avg_fitness.png"
        aggregate_csv = base_dir / "aggregate_avg_fitness.csv"
        title = f"{title_prefix} · Aggregate Average Fitness" if title_prefix else None
        saved_png, saved_csv = save_aggregate_plot(
            series_collection,
            aggregate_png,
            aggregate_csv,
            title=title,
        )
        outputs["aggregate_png"] = saved_png
        outputs["aggregate_csv"] = saved_csv

    return outputs


def plot_inputs(
    inputs: Sequence[Path],
    aggregate_output_dir: Path | None = None,
    skip_individual: bool = False,
    title_prefix: str | None = None,
) -> tuple[list[FitnessSeries], dict[str, object]]:
    csv_files = discover_fitness_csvs(inputs)
    series_collection = [load_fitness_series(csv_path) for csv_path in csv_files]
    outputs = plot_series_collection(
        series_collection,
        aggregate_output_dir=aggregate_output_dir,
        skip_individual=skip_individual,
        title_prefix=title_prefix,
    )
    return series_collection, outputs


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot Neural Eco fitness.csv files as PNG charts")
    parser.add_argument(
        "inputs",
        nargs="+",
        type=Path,
        help="One or more fitness.csv files or directories to scan recursively",
    )
    parser.add_argument(
        "--aggregate-output-dir",
        type=Path,
        default=None,
        help="Output directory for aggregate plots when multiple runs are provided",
    )
    parser.add_argument(
        "--skip-individual",
        action="store_true",
        help="Only emit the aggregate plot when multiple runs are provided",
    )
    parser.add_argument(
        "--title-prefix",
        type=str,
        default=None,
        help="Optional title prefix to prepend to generated plots",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    series_collection, outputs = plot_inputs(
        args.inputs,
        aggregate_output_dir=args.aggregate_output_dir,
        skip_individual=args.skip_individual,
        title_prefix=args.title_prefix,
    )

    print(f"Loaded {len(series_collection)} fitness run(s)")
    for series in series_collection:
        print(f"  - {series.label}: {series.csv_path}")

    individual_outputs: Iterable[Path] = outputs["individual"] if isinstance(outputs["individual"], list) else []
    for output_path in individual_outputs:
        print(f"Saved individual plot: {output_path}")

    aggregate_png = outputs["aggregate_png"]
    aggregate_csv = outputs["aggregate_csv"]
    if isinstance(aggregate_png, Path):
        print(f"Saved aggregate plot: {aggregate_png}")
    if isinstance(aggregate_csv, Path):
        print(f"Saved aggregate stats: {aggregate_csv}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
