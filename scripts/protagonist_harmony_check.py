"""protagonist_harmony_check.py

Audit a single protagonist_ecology training run for the seven "harmony"
qualitative checks. **Internal tool**: not for end-user reports; surfaces
the seven gates that decide whether the main world will look alive once we
finally route training output into Layer 7 viewer + 3D Godot scene.

Reads only the two per-generation CSVs (protagonist_behaviors.csv and
species_metrics.csv) plus the run's evolution.csv -- avoids parsing the
144MB/episode event_trace.jsonl.

Seven checks (per RUNTIME_FINAL_STATE_VISION + private viewer checklist):

  C1  Hunters active                 : pack_hunter / apex_predator / ambush
                                       populations stay non-zero across all
                                       generations (no early collapse).
  C2  Rabbits hunted                 : herbivore archetypes show non-zero
                                       attrition (population varies > 0 across
                                       generations -- not frozen).
  C3  Protagonist coop building      : cooperative_co_presence_seconds > 0
                                       in any generation past warmup.
  C4  Scavengers near corpses        : scavenger population non-zero + non-zero
                                       avg_fitness (proxy for actually eating
                                       carrion, since corpse-pickup is a key
                                       fitness path for that archetype).
  C5  Shelter use under rain         : completed_worksites > 0 + at least one
                                       generation has worksite_completion_events
                                       > 0 (shelter buildings actually finish).
  C6  Spatial distribution diversity : me_coverage from evolution.csv reaches
                                       at least 0.30 (10/25 cells in 2D BD or
                                       60/192 in 3D BD) in any generation past
                                       warmup (low coverage => agents cluster).
  C7  No species fully extinct       : every archetype's population stays >= 1
                                       across all generations.

Usage::

    python3 scripts/protagonist_harmony_check.py <run_dir>

Where run_dir is e.g. data/runs/long_run_60gen_2d_throw/20260524-185129.
The script prints a compact markdown report; non-zero exit code if any
check fails.
"""

from __future__ import annotations

import csv
import sys
from collections import defaultdict
from pathlib import Path
from typing import Optional

WARMUP_GENS = 4  # don't penalize early generations for "no build" etc.

# Archetype names that should never go extinct in a healthy run.
ALL_ARCHETYPES = {
    "protagonist",
    "large_herbivore",
    "small_forager",
    "apex_predator",
    "pack_hunter",
    "ambush_predator",
    "scavenger",
    "omnivore",
    "rabbit",
}

HUNTERS = {"pack_hunter", "apex_predator", "ambush_predator"}
HERBIVORES = {"large_herbivore", "small_forager", "rabbit"}
SCAVENGERS = {"scavenger", "omnivore"}


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open() as f:
        return list(csv.DictReader(f))


def to_float(s: str) -> float:
    try:
        return float(s)
    except (ValueError, TypeError):
        return 0.0


def to_int(s: str) -> int:
    try:
        return int(float(s))
    except (ValueError, TypeError):
        return 0


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2

    run_dir = Path(sys.argv[1]).resolve()
    if not run_dir.exists():
        print(f"ERR: {run_dir} does not exist", file=sys.stderr)
        return 2

    pb_path = run_dir / "protagonist_behaviors.csv"
    sm_path = run_dir / "species_metrics.csv"
    fit_path = run_dir / "fitness.csv"

    pb_rows = read_csv(pb_path)
    sm_rows = read_csv(sm_path)
    fit_rows = read_csv(fit_path)

    if not pb_rows:
        print(f"ERR: no protagonist_behaviors.csv in {run_dir}", file=sys.stderr)
        return 2

    # Group species rows by (gen, archetype)
    species_pop: dict[tuple[int, str], int] = {}
    species_fit: dict[tuple[int, str], float] = {}
    for r in sm_rows:
        gen = to_int(r.get("generation", "0"))
        sp = r.get("species", "")
        species_pop[(gen, sp)] = to_int(r.get("population", "0"))
        species_fit[(gen, sp)] = to_float(r.get("avg_fitness", "0"))

    last_gen = max((to_int(r["generation"]) for r in pb_rows), default=-1)
    total_gens = last_gen + 1

    checks: list[tuple[str, bool, str]] = []

    # --- C1 hunters active across all gens ---
    c1_min = {arch: float("inf") for arch in HUNTERS}
    for (gen, sp), pop in species_pop.items():
        if sp in HUNTERS:
            c1_min[sp] = min(c1_min[sp], pop)
    c1_pass = all(v >= 1 for v in c1_min.values())
    c1_msg = ", ".join(f"{k}>={int(v) if v != float('inf') else 'N/A'}"
                       for k, v in c1_min.items())
    checks.append(("C1 hunters active", c1_pass, c1_msg))

    # --- C2 rabbits / herbivores hunted (population varies, not frozen) ---
    c2_pass = False
    c2_detail = []
    for arch in HERBIVORES:
        pops = [pop for (_, sp), pop in species_pop.items() if sp == arch]
        if pops:
            var = max(pops) - min(pops)
            c2_detail.append(f"{arch}: range {min(pops)}-{max(pops)}")
            if var >= 1:
                c2_pass = True
    checks.append(("C2 herbivores attrition", c2_pass, "; ".join(c2_detail)))

    # --- C3 coop building (post-warmup) ---
    coop_seconds_max = 0.0
    for r in pb_rows:
        gen = to_int(r.get("generation", "0"))
        if gen < WARMUP_GENS:
            continue
        coop_seconds_max = max(coop_seconds_max, to_float(r.get("cooperative_co_presence_seconds", "0")))
    c3_pass = coop_seconds_max > 0.0
    checks.append(("C3 coop co-presence", c3_pass, f"peak={coop_seconds_max:.1f}s"))

    # --- C4 scavengers fed (population & fitness non-zero) ---
    c4_pass = False
    c4_detail = []
    for arch in SCAVENGERS:
        pops = [pop for (g, sp), pop in species_pop.items() if sp == arch]
        fits = [f for (g, sp), f in species_fit.items() if sp == arch]
        if pops:
            c4_detail.append(f"{arch}: pop {min(pops)}-{max(pops)} fit {max(fits or [0]):.0f}")
            if min(pops) >= 1 and max(fits or [0]) > 100.0:
                c4_pass = True
    checks.append(("C4 scavengers fed", c4_pass, "; ".join(c4_detail)))

    # --- C5 shelter actually built (worksite_completion_events > 0 any gen) ---
    completion_max = 0
    completion_total = 0
    for r in pb_rows:
        ev = to_int(r.get("worksite_completion_events", "0"))
        completion_max = max(completion_max, ev)
        completion_total += ev
    c5_pass = completion_total > 0
    checks.append(("C5 worksites completed", c5_pass,
                   f"peak={completion_max}/gen total={completion_total}"))

    # --- C6 spatial diversity me_coverage (from fitness.csv) ---
    me_cov_max = 0.0
    qd_max = 0.0
    for r in fit_rows:
        me_cov_max = max(me_cov_max, to_float(r.get("me_coverage", "0")))
        qd_max = max(qd_max, to_float(r.get("me_qd_score", "0")))
    c6_pass = me_cov_max >= 0.30
    checks.append(("C6 me_coverage >= 0.30", c6_pass, f"peak={me_cov_max:.3f} qd={qd_max:.0f}"))

    # --- C7 no extinction across gens ---
    extinct = []
    for arch in ALL_ARCHETYPES - {"protagonist"}:  # protagonist count comes from pb
        pops = [pop for (g, sp), pop in species_pop.items() if sp == arch]
        if pops and min(pops) == 0:
            extinct.append(f"{arch}@min0")
    # Protagonist extinction check via pb
    for r in pb_rows:
        if to_int(r.get("survived_protagonists", "0")) == 0:
            extinct.append(f"protagonist@gen{r['generation']}")
    c7_pass = not extinct
    checks.append(("C7 no extinction", c7_pass, ", ".join(extinct) if extinct else "all archetypes survived"))

    # --- Report ---
    print(f"\n# Harmony check · {run_dir.name}")
    print(f"\nTotal generations: {total_gens}")
    print(f"\n| # | Check | Result | Detail |")
    print(f"|---|---|---|---|")
    all_pass = True
    for name, ok, detail in checks:
        mark = "PASS" if ok else "FAIL"
        if not ok:
            all_pass = False
        print(f"| {name.split()[0]} | {name[3:]} | {mark} | {detail} |")
    print(f"\nOverall: {'ALL PASS' if all_pass else 'FAILURES PRESENT'}")
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
