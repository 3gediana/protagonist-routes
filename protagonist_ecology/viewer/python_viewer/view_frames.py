# /// script
# requires-python = ">=3.10"
# dependencies = ["pygame>=2.5"]
# ///
"""
Layer 7 R2 minimal viewer for protagonist_ecology's viewer_frames.jsonl.

Usage:
    uv run view_frames.py <path_to_viewer_frames.jsonl>

Renders 2D top-down: agents as colored dots (by archetype), worksites as
squares (by building_type), with playback controls (Space pause, arrows
seek, +/- speed).

Designed as MVP per LAYER7_GODOT_VIEWER_SPEC.md S7. After this works
end-to-end, port to Godot 4 with Kenney models for the full vision.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pygame

# -------- visual constants --------
WINDOW_W, WINDOW_H = 1200, 1200
WORLD_W, WORLD_H = 600.0, 600.0  # Layer 1 default world size

# 9 archetype colors (Protagonist=0, LargeHerbivore=1, ..., Rabbit=8).
ARCHETYPE_COLORS = [
    (60, 180, 75),     # Protagonist - green
    (255, 225, 25),    # LargeHerbivore - yellow (cow)
    (170, 110, 40),    # SmallForager - brown
    (255, 80, 80),     # ApexPredator - red (tiger)
    (128, 128, 128),   # PackHunter - grey (wolf)
    (245, 130, 48),    # AmbushPredator - orange (leopard)
    (70, 240, 240),    # Scavenger - cyan
    (240, 50, 230),    # Omnivore - magenta (bear)
    (255, 255, 255),   # Rabbit - white
]

# 4 building type colors (Wall, Shelter, Storage, Lookout).
BUILDING_COLORS = [
    (100, 100, 100),  # Wall - grey
    (200, 150, 100),  # Shelter - tan (yurt)
    (140, 80, 40),    # Storage - dark brown
    (50, 100, 200),   # Lookout - blue
]

# 6 weather background tints.
WEATHER_TINTS = [
    (180, 220, 240, 0),   # Clear - soft blue
    (160, 180, 200, 0),   # Cloudy
    (110, 140, 180, 80),  # Rain - darker blue overlay
    (60, 80, 120, 140),   # Storm - dark blue
    (200, 200, 180, 50),  # HighWind - dusty
    (40, 50, 80, 180),    # Typhoon - very dark
]


def world_to_screen(x: float, y: float) -> tuple[int, int]:
    sx = int(x / WORLD_W * WINDOW_W)
    sy = int(y / WORLD_H * WINDOW_H)
    return sx, sy


def load_frames(path: Path) -> list[dict]:
    frames = []
    with path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                try:
                    frames.append(json.loads(line))
                except json.JSONDecodeError as e:
                    print(f"skip malformed line: {e}", file=sys.stderr)
    return frames


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 1
    frames = load_frames(Path(argv[1]))
    if not frames:
        print("no frames loaded")
        return 1
    print(f"loaded {len(frames)} frames, t=[{frames[0]['t']}, {frames[-1]['t']}]")

    pygame.init()
    screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
    pygame.display.set_caption("protagonist_ecology Layer 7 viewer")
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("monospace", 16)

    idx = 0
    paused = False
    speed = 1.0  # frames per render-tick

    while True:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                pygame.quit()
                return 0
            if ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_SPACE:
                    paused = not paused
                elif ev.key == pygame.K_RIGHT:
                    idx = min(idx + 10, len(frames) - 1)
                elif ev.key == pygame.K_LEFT:
                    idx = max(idx - 10, 0)
                elif ev.key in (pygame.K_PLUS, pygame.K_EQUALS):
                    speed = min(speed * 1.5, 32.0)
                elif ev.key == pygame.K_MINUS:
                    speed = max(speed / 1.5, 0.25)
                elif ev.key == pygame.K_r:
                    idx = 0

        if not paused:
            idx = (idx + max(1, int(speed))) % len(frames)

        frame = frames[idx]
        weather = frame.get("w", 0)
        tint = WEATHER_TINTS[weather % len(WEATHER_TINTS)][:3]
        screen.fill(tint)

        # L7 R3: trees first (background layer). Available trees = green
        # circle scaled by integrity ratio; harvested trees = grey stump.
        for t in frame.get("trees", []):
            sx, sy = world_to_screen(t["x"], t["y"])
            iv = t.get("iv", 1.0)
            available = t.get("av", 1)
            if available:
                green = (40, 130 + int(80 * iv), 40)
                radius = 3 + int(4 * iv)
                pygame.draw.circle(screen, green, (sx, sy), radius)
            else:
                pygame.draw.circle(screen, (90, 70, 50), (sx, sy), 2)

        for ws in frame.get("worksites", []):
            sx, sy = world_to_screen(ws["x"], ws["y"])
            bt = ws.get("bt", 0)
            color = BUILDING_COLORS[bt % len(BUILDING_COLORS)]
            size = 18 if ws.get("c", 0) else 10
            pygame.draw.rect(screen, color, (sx - size // 2, sy - size // 2, size, size), 0 if ws.get("c") else 2)
            if ws.get("a", 0) and not ws.get("c", 0):
                p = ws.get("p", 0.0)
                bar_w = int(20 * p)
                pygame.draw.rect(screen, (50, 255, 50), (sx - 10, sy + size // 2 + 2, bar_w, 3))

        for a in frame.get("agents", []):
            if not a.get("al"):
                continue
            sx, sy = world_to_screen(a["x"], a["y"])
            arch = a.get("a", 0)
            color = ARCHETYPE_COLORS[arch % len(ARCHETYPE_COLORS)]
            radius = 5 if arch == 0 else 3
            pygame.draw.circle(screen, color, (sx, sy), radius)
            lifecycle = a.get("L", 1)
            if lifecycle == 0:
                pygame.draw.circle(screen, (255, 255, 255), (sx, sy), radius + 2, 1)
            elif lifecycle == 2:
                pygame.draw.circle(screen, (100, 100, 100), (sx, sy), radius + 2, 1)

        wp = frame.get("wp", 0.0)  # weather progress (0..1)
        sp = frame.get("sp", 0.0)  # season progress (0..1)
        hud = font.render(
            f"frame {idx}/{len(frames) - 1}  tick={frame['tick']}  t={frame['t']:.1f}s  "
            f"weather={weather}({wp:.0%})  season={frame.get('s', 0)}({sp:.0%})  "
            f"speed={speed:.1f}{'  [PAUSED]' if paused else ''}",
            True, (255, 255, 255), (0, 0, 0)
        )
        screen.blit(hud, (10, 10))
        controls = font.render(
            "[Space]pause [Left/Right]+-10frames [R]reset [+/-]speed",
            True, (255, 255, 255), (0, 0, 0)
        )
        screen.blit(controls, (10, WINDOW_H - 30))

        pygame.display.flip()
        clock.tick(30)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
