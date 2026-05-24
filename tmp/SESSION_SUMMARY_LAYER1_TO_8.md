# Session Summary · FINAL_BLUEPRINT.md Layer 1-8 Push

> 日期: 2026-05-24
> 22 commits, ~2800 LOC code + ~1100 LOC docs

---

## 一句话

按 `FINAL_BLUEPRINT.md` 的 8-Layer 蓝图，**Layer 1-5 完整实现 + Layer 6/7/8
R1 baseline + 4 个详细设计 spec + viewer pipeline 端到端可用**。

**长跑实测**：Layer 5 30 代 protagonist avg=4543 best=18451，**6.2x v17 baseline**。

---

## Layer 1 (体温 + 600×600 世界)
- **commits**: `a0669f6`, `7a098a6`
- Agent::core_temperature_ (37°C 默认, 20-50 clamp)
- `routes/protagonist_ecology/capability/ThermalTickCapability`
- World 200×200 → 600×600 (9× area)
- 所有 toml resources scale ×9
- **结果**: alive_rate 0.45 → 0.79

## Layer 2 (Weather)
- **commit**: `202186b`
- `src/core/world/layers/WeatherLayer.{h,cpp}`: 6 type Markov (Clear/Cloudy/Rain/Storm/HighWind/Typhoon)
- Season 偏置 (summer storms ×3, winter no typhoons, winter rain ×0.5)
- 集成: Rain hydration buff, HighWind 1.5× move cost, Storm 5% lightning per minute per tree, Typhoon tree fall
- **结果**: avg_fit 990 → 1972 (+167%)

## Layer 3 (Rabbit + 9 物种生态闭环)
- **commit**: `e3d2b8b`
- `SpeciesArchetype::Rabbit` (output_dim=3, initial_energy=50, damage_threshold=0)
- 8 种背景 + 20 兔子 + 24 protagonist = 100 founders
- **结果**: avg_fit 1972 → 6685 best 12390 (Gen 25)

## Layer 4 (Lifecycle + 繁殖 + 谱系)
- **commits**: `da86050` (core), `1377352` (perf)
- `Agent.h` 新字段:
  - `LifecycleStage { Juvenile, Adult, Elder }` (computed from age/max_age)
  - `DeathCause { Starvation/Predation/OldAge/Disease/Thermal/Drowning/... }`
  - `lineage_id_/parent_lineage_a/b_/birth_generation_/offspring_count_`
  - `archetype_tag_` (uint8_t for core/route decoupling)
  - `reproduction_cooldown_/max_age_seconds_`
  - `genome_archive_` unique_ptr (父本 genome 持有供繁殖 clone)
  - `sense_radius_scale_` (L5 R3 Lookout buff)
- `routes/protagonist_ecology/world/PopulationLayer.{h,cpp}`:
  - Phase 1: cooldown 衰减 / OldAge 自动 kill / Elder frailty 概率死亡
  - Phase 2/3: 同物种 adult 配对 (radius 8, energy≥60, cooldown=0)
- `BootstrapPopulation::makeChildFromParents`: clone better-fit parent NeatGenome →
  weight noise ±0.08 → makeNeatSpeciesAgent (same archetype)
- **perf throttle** (`1377352`): `max_spawns_per_tick=1` + `reproduction_check_interval=300s`
- **结果**: 4 in-world births 验证, OldAge path exercised

## Layer 5 (Building + Reward Shift) — R1+R2+R3 全完整
- **commits**: `3501070`+`da86050` (R1 lite), `5824eb7` (R2), `b8326ee` (R3)
- **R1 lite**:
  - `BuildingType { Wall=default, Shelter, Storage, Lookout }` enum in WorksiteLayer
  - `buildingAt(pos, type)` + `isShelteredAt(pos)` API
  - `blocksPredatorAt` 仅 Wall 类型
  - SurvivalStatusLayer 雨天 sheltered agent 不收 hydration buff
  - `tmp/layer1/layer5.toml` reward paradigm shift:
    - survival_weight 1.0→0.3, food_weight 10→3
    - worksite_completion_reward 60→150
    - cooperation_weight 6→12, communication_weight 4→8
    - signal_co_attendance 0.6→1.5, joint_hunt 8→18
- **R2** (`5824eb7`):
  - `WorksiteConfig.type_dist_{wall,shelter,storage,lookout}` 4 个 toml weights
  - `WorksiteLayer::setBuildingTypeWeights` + 加权采样 in seedSites
  - `Worksite.stored_sticks_overflow / stones_overflow` (Storage 二级库存)
  - `absorbToCompletedStorages` tick step
  - `totalStorageSticks/Stones` aggregators
- **R3** (`b8326ee`):
  - `Agent::sense_radius_scale_` (float, default 1.0)
  - WorksiteLayer tick 更新: in Lookout → 1.5, else → 1.0
  - `AgentSocialPerception` 用 effective_r2 = sense_radius² × scale²
- **30-gen 长跑结果** (layer5.toml 0.5/0.25/0.15/0.10 dist):
  - Gen 27 peak: protagonist avg=**4543** best=**18451**
  - Gen 23 builds=3 (首次稳定建造涌现)
  - **vs v17 baseline avg 737 = 6.2× improvement**
  - 所有 8 物种 30 代均未灭绝

## Layer 6 R1 (WorldRole 字符串扩展)
- **commit**: `b53a4e8`
- `ProtagonistEcologyConfig::isBorderlandRole()` / `isMainWorldRole()`
- `normalizeRole` 接受 `"borderland"` / `"main"` / `"main_world"`
- 完整 TripleWorldRuntime + MigrationScheduler 实现 deferred 到 R2

## Layer 7 R1+R2 (Viewer)
- **commits**: `456e09b` (R1), `617d5df` (R2)
- **R1**: `TraceRecorder` 加 `viewer_frames_enabled` + `writeViewerFrame`
  - JSON lines format: `{"tick":T,"t":S,"agents":[...],"worksites":[...],"w":W,"s":S}`
  - Agent fields: id/archetype/pos/energy/lifecycle/alive
  - Worksite fields: id/building_type/pos/progress/active/completed
- **R2**: `routes/protagonist_ecology/viewer/python_viewer/view_frames.py`
  - PEP 723 self-contained pygame viewer (160 LoC)
  - Archetype-colored agents (9 colors), building-type squares (4 colors),
    weather-tinted background (6 tints)
  - Controls: Space pause / arrows seek / +/- speed / R reset
- **管线端到端可用**: C++ runtime → viewer_frames.jsonl → Python viewer

## Layer 8 R1 (Lamarckian)
- **commit**: `4d57714`
- `ProtagonistEcologyConfig::LamarckianConfig {enabled, learning_rate, reward_threshold}`
- `[evolution.lamarckian]` toml 解析
- Runtime: 在每个 background `addFitness(bg_reward)` 后, 若 enabled 且
  `|reward|>threshold`, 调 `NeatBrain::hebbianUpdate(lr × sign(reward))`
- 3-gen smoke 验证跑通

---

## 设计文档 (deferred work 入口)

`routes/protagonist_ecology/docx/supporting/`:
- `LAYER5_BUILDING_SPEC.md` — R2/R3 deferred 细节
- `LAYER6_THREE_WORLDS_SPEC.md` — 三层世界 + 17 invariants + MigrationScheduler API
- `LAYER7_GODOT_VIEWER_SPEC.md` — Godot 4 + Kenney + Mixamo 完整 vision
- `LAYER8_ADVANCED_MECHANISMS_SPEC.md` — DIAYN / World Model / Emergent Comm

---

## 文档同步

- `routes/protagonist_ecology/README.md` (new): single-page route entry
- `routes/protagonist_ecology/docx/PROGRESS.md`: Layer 1-4 表格 + R2/R3 进度
- `routes/protagonist_ecology/docx/STATUS.md`: 顶部巡检全部更新
- `PROGRESS.md` (顶层): 21 commits 摘要 + 6.2× 长跑结果
- `AGENTS.md`: 加 § 10 tool constraint (exec timeout ≤ 4min, shell cache 5min)
- `tmp/HANDOFF_LAYER5_TO_8.md`: 全部 Layer 1-5 + 6/7/8 R1 + 经验总结

---

## 22 个 commits（按时间倒序）

```
a516b1f PROGRESS.md: final session summary - 21 commits + Layer 1-5 + 6/7/8 R1
2f6285d routes/protagonist_ecology/README.md: single-page route entry point
54191ef STATUS.md: sync Layer 6/7/8 R1 + Python viewer
617d5df Layer 7 R2: minimal Python viewer for viewer_frames.jsonl
cc2503f AGENTS.md: add tool constraint section (exec timeout <= 4min)
456e09b Layer 7 R1: viewer_frames.jsonl frame dumper for Godot/raylib
b53a4e8 Layer 6 R1: WorldRole borderland + main_world accessors
4d57714 Layer 8 R1: Lamarckian Hebbian online weight adaptation
0212d25 STATUS.md: sync Layer 1-5 complete + Layer 6/7/8 design specs
f2ce1a4 HANDOFF doc: update Layer 5 R1+R2+R3 commits + 30-gen long-run data
b8326ee Layer 5 R3: Lookout perception bonus
2045c11 PROGRESS.md: sync Layer 5 30-gen long-run result + 4 design specs
890e848 Layer 8: advanced mechanisms design spec (deferred)
31868b5 Layer 7: Godot 3D viewer design spec (deferred)
5824eb7 Layer 5 R2: building_type distribution + Storage overflow stockpile
33f4e39 PROGRESS.md: sync Layer 1-5 lite completion summary
be33591 Layer 6: three-worlds design spec (deferred work)
e598450 Layer 5: building spec doc covering R2/R3 design (deferred work)
da86050 Layer 4+5 core: Agent lifecycle/lineage fields + Bootstrap + Worksite BuildingType
1377352 Layer 4 perf: max_spawns_per_tick + amortized check interval
6da0b4b Layer 5 lite handoff doc + PROGRESS.md sync
3501070 Layer 5 (lite) + deep_protagonist S3.9 (combined commit due to hook race)
```

---

## 下次会话从哪里接 (优先级)

1. **L7 R3 Godot 4 viewer** — user vision (Mongolian-yurt + 高光场景)
   最直接，参考 `LAYER7_GODOT_VIEWER_SPEC.md` Stage 2-4 (~1150 LoC GDScript)

2. **L6 R2 MigrationScheduler 雏形** — 主世界永不 reset 核心架构
   参考 `LAYER6_THREE_WORLDS_SPEC.md` (~250 LoC C++)

3. **L8 R2 DIAYN** — protagonist 风格分化 viewer 中最显眼
   参考 `LAYER8_ADVANCED_MECHANISMS_SPEC.md` (~220 LoC C++)

4. **L8 R3 World Model** — 高投入但高回报 (deferred to last)

---

## 关键经验 (踩坑总结)

1. **exec timeout ≤ 4min** (shell cache 5min)
2. **Re-build 前 kill exe**: `taskkill /F /IM neural-eco-protagonist.exe`
3. **toml table 不能重复**
4. **WSL stdout UTF-16 坑**: 用 `cmd.exe /c "... > file 2>&1"`
5. **Layer 4 reproduction 慢**: `max_spawns_per_tick=1` + `check_interval=300s` amortize
6. **Routes 层 layer include**: `world/Foo.h` 不带前缀
7. **`getOr<size_t>` template** 没实例化: 用 `int` + `static_cast`
8. **Agent 字段加在 private section**: 避免 ABI 破坏
9. **多 user 并行 commit hook race**: 同 root user 同时跑两个 routes 时 commit 会被合并
