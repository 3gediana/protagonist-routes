# Layer 7 · Godot 3D Viewer 设计规格 (FINAL_BLUEPRINT.md §7)

> 状态: 设计阶段（实现 deferred to next session）
> 用户原话: "建造闭环 + 视觉建筑（Mongolian-yurt-level）+ 高光场景"

---

## 1. 目标

让用户能"看见"训练后的世界——主世界（Layer 6）的实时和回放：

| 视觉要素 | 目标 |
|---|---|
| 地形 | 600×600 平面 + 简单高度图 (BaseLayer) |
| 树木 | 5 种 (fruit/deciduous/evergreen/seasonal/dead) — Kenney 已就位 |
| 建筑 | 4 种 (Wall/Shelter/Storage/Lookout) — Mongolian-yurt 级别 |
| Agent | 9 种物种用 Kenney+Mixamo 模型 (viking_male/female/cow/wolf/eagle 等) |
| 天气 | 6 种 (Clear/Cloudy/Rain/Storm/HighWind/Typhoon) — 粒子+灯光 |
| 季节 | 4 种 — 全局色调+草地/雪覆盖 |
| 高光场景 | 群猎、护卫、繁殖、信号、围火、围墙建造 |

---

## 2. 技术选型

### Godot 4.x (推荐)

理由:
1. 原生 GDScript / C# 双语言，C# 容易和现有 C++ runtime 对接
2. GLB/GLTF 直接加载 Kenney/Mixamo 模型
3. Built-in particle system 适合天气
4. Open source，protocol buffers 集成简单

### 数据流: C++ → JSON/binary → Godot

```
[C++ runtime] ── tracedumper ──> trace files (per-tick state)
                                          │
[Godot replay viewer] ──── parses ─────┘
```

或 live mode:
```
[C++ runtime: --live] ── stdout JSON-lines ──> [Godot reads stdin]
```

---

## 3. 已就位资源

### Kenney resources (`routes/protagonist_ecology/data/kenney/`)
- viking_male / viking_female (protagonist)
- cow (large_herbivore)
- wolf (pack_hunter, apex_predator with skin variant)
- eagle (ambush_predator alternative)
- 5 trees (fruit/deciduous/evergreen + 2 dead)
- bushes (resource_layer rendering)
- tent (Mongolian-yurt — Shelter building 直接用)
- bonfire (CampfireLayer 已有)
- woodlog (Storage stockpile visual)
- axe (carry visual when ChopTreeCapability active)

### 缺少 (需 Mixamo / 自建)
- rabbit (small fast 4-legged)
- small_forager (rodent / squirrel)
- ambush_predator (leopard/cat — 可用 wolf 着色变体)
- scavenger (vulture — 可用 eagle 着色变体)
- omnivore (bear — Mixamo 有 free)
- Wall building (logs stack — 简单 ProcGen)
- Storage building (woodlog stack)
- Lookout building (tower — Mixamo 有 free 'wooden lookout')

---

## 4. 实施路径

### Stage 1: trace dumper (C++ side, ~200 行)
- 扩展 `runtime/TraceRecorder.cpp` 输出 frame-based JSON（vs 现有 event-based）
- 每个 frame 包含:
  - sim_time
  - agents[]: id, species, position, lifecycle_stage, energy, alive
  - worksites[]: id, position, building_type, build_progress, completed
  - trees[]: id, position, kind, harvested
  - weather: type, intensity
  - season: type, progress

### Stage 2: Godot project skeleton (~600 行)
- `routes/protagonist_ecology/viewer/godot_project/`
  - project.godot
  - main_scene.tscn (camera + lighting + terrain)
  - scripts/replay_loader.gd (parses JSON frames)
  - scripts/agent_renderer.gd (instances Kenney 模型)
  - scripts/weather_controller.gd (粒子+雾)
  - scripts/season_controller.gd (color grading)
  - assets/ (导入 Kenney glb)

### Stage 3: live mode IPC (~150 行)
- C++ runtime --live 标志: 每 N ticks 输出 frame JSON 到 stdout
- Godot subprocess 读 stdin, 实时渲染
- 用户可暂停/快进/截图

### Stage 4: 高光检测 (~100 行)
- C++ side: 在 trace 里标记"高光事件"（kill_burst, build_complete, mass_birth）
- Godot: 收到高光事件 -> 自动相机切换到事件位置 + slow-motion 3 sec

---

## 5. 工作量

| 阶段 | 估算 | 复杂度 |
|---|---|---|
| Stage 1: trace dumper | 200 行 | low |
| Stage 2: Godot skeleton | 600 行 GDScript | medium |
| Stage 3: live mode | 150 行 | medium |
| Stage 4: 高光检测 | 100 行 + Godot 100 行 | low |
| 总 | **~1150 行** | medium |

需要熟悉 Godot 的 session — 估计 1-2 个完整 session。

---

## 6. 与 Layer 5/6/8 的接口

### Layer 5 (Building)
- viewer 用 Worksite.building_type 字段选模型 (tent/log_stack/woodlog_pile/lookout_tower)
- 显示 stored_sticks_overflow / stored_stones_overflow 在 Storage 上

### Layer 6 (三层世界)
- viewer 可同时观察 sandbox / borderland / main world (multi-window 或 split-screen)
- 主世界 replay 直接从存档加载

### Layer 8 (Lamarckian / DIAYN)
- viewer 显示 lineage 谱系树 (parent_lineage_a/b → birth_generation)
- 显示 protagonist DNC memory state 可视化（attention heatmap）

---

## 7. 极简 MVP (1 day version)

如果时间紧:
1. 跳过 Godot，直接用 raylib 做简单 3D viewer（routes/protagonist_ecology/viewer/raylib_viewer.cpp）
2. 用现有 C++ TraceRecorder JSON 输出 + raylib 加载 glb
3. 不做 live mode (replay-only)

raylib 已经在依赖里 (build_ninja_cuda/_deps/raylib-src)，集成成本最低。
缺点：功能受限，没有 Godot 的高级粒子/灯光系统。

---

## 8. 决策建议

按用户 vision (Mongolian-yurt 级别 + 高光场景) 必须用 Godot 才有视觉冲击。
但下次 session 可以分两步:
1. **第一周** raylib MVP 跑通"replay 看到 agent 移动 + worksite 完成"
2. **第二周** 移植到 Godot，加粒子/灯光/Mixamo 动画

**优先级**: 先做 Stage 1 (trace dumper)，因为这是 raylib 和 Godot 都需要的基础。
