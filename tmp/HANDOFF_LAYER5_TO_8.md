# Handoff: Layer 5 (extended) → Layer 8

> 续接上一份 `tmp/HANDOFF_LAYER3_TO_8.md`。本会话完成 Layer 4 + Layer 5 (lite)，
> 下一会话从 Layer 5 (extended) 或 Layer 6 起步。

---

## 已完成（截至 commit 25658fa）

### Layer 1 (commits a0669f6, 7a098a6) — 体温 + 600×600
- Agent::core_temperature_ + ThermalTickCapability
- World 200×200 → 600×600 (9× area)
- alive 0.45 → 0.79

### Layer 2 (commit 202186b) — WeatherLayer
- 6 weather types: Clear/Cloudy/Rain/Storm/HighWind/Typhoon
- Markov 状态转移 + Season 偏置 (summer storms, winter no typhoons)
- Rain restores hydration; HighWind boosts move cost; Storm ignites trees
- avg_fitness 990 → 1972 (+167%)

### Layer 3 (commit e3d2b8b) — Rabbit + 9 物种
- SpeciesArchetype::Rabbit (output_dim=3, initial_energy=50, damage_threshold=0)
- 8 species + 20 rabbits = 76 background + 24 protagonist = 100 founders
- avg_fitness 1972 → 6685 (gen 25 best 12390)

### Layer 4 (commit 148b6d8) — 生命周期 + 谱系 + 体内繁殖
- Agent.h 新增字段:
  - `LifecycleStage enum { Juvenile, Adult, Elder }` (computed from age/max_age)
  - `DeathCause enum { Unknown/Starvation/Dehydration/Predation/Combat/OldAge/Disease/Thermal/Drowning }`
  - `lineage_id_, parent_lineage_a/b_, birth_generation_, offspring_count_`
  - `archetype_tag_` (uint8_t for core/route decoupling)
  - `reproduction_cooldown_, max_age_seconds_`
  - `genome_archive_ unique_ptr<IGenome>` — 父本 genome 持有，供繁殖时 clone
- `routes/protagonist_ecology/world/PopulationLayer.{h,cpp}`:
  - Phase 1: 每 tick 衰减 cooldown / OldAge 自动 kill / Elder frailty 概率死亡
  - Phase 2/3: 同物种 adult 配对 (radius 8, energy ≥ 60, cooldown=0)
  - SpawnCallback 由 ProtagonistWorldFactory 注入，调用 `makeChildFromParents`
- `BootstrapPopulation`:
  - `stampLifecycleAndLineage`: archetype_tag + per-archetype max_age (jitter ±15%)
  - `makeChildFromParents`: clone better-fit parent NeatGenome → noise weight ±0.08 →
    makeNeatSpeciesAgent (same archetype)
  - 所有 founder agents `setGenomeArchive(genome->clone())`
- 配置: `[world.population]` 8 个物种 max_age + 通用繁殖参数
- 验证: 4 births verified, OldAge path 已 exercise
- **性能问题**: 每次 in-world spawn ~100ms（NeatBrain reconstruction），
  100 agent + reproduction 让 tick rate 5/s vs no-reproduction 40/s。Layer 8 优化。

### Layer 5 (commits 3501070, da86050, e598450, 5824eb7, b8326ee) — Building 闭环 + reward shift
- R1 (lite): WorksiteLayer 加 `BuildingType { Wall=default, Shelter, Storage, Lookout }`
  + `buildingAt(pos, type)` + `isShelteredAt(pos)` API + blocksPredatorAt 仅 Wall 类型
  + SurvivalStatusLayer 雨天 sheltered agent 不收 hydration buff
  + layer5.toml reward paradigm shift
- R2 (5824eb7): WorksiteConfig.type_dist_* 4 个 weight toml 字段 + setBuildingTypeWeights
  + Worksite.stored_sticks_overflow/stones_overflow + absorbToCompletedStorages
  + totalStorageSticks/Stones aggregators
- R3 (b8326ee): Agent::sense_radius_scale_ + WorksiteLayer tick 更新 (1.5 in Lookout)
  + AgentSocialPerception 用 effective_r2 = sense_radius² × scale²

- **Layer 5 30-gen 长跑数据 (建议 baseline)**:
  - Gen 27 peak: protagonist avg=4543 best=18451
  - Gen 23 builds=3 (首次稳定建造涌现)
  - vs v17 baseline avg 737 = **6.2x improvement**
  - 所有 8 物种 30 代均未灭绝

---

## 关键经验 (踩坑总结)

### 性能 (Layer 4)
- in-world spawn 慢的根因: `makeNeatSpeciesAgent` 重新装配所有 perception/capability
  + NeatBrain 从 NeatGenome 解析 connections。
- 优化方向 (deferred to Layer 8):
  - Capability/Perception pool reuse (factory cache)
  - NeatBrain "fast clone" path: copy weights bypass parsing
  - Spawn batching (集中多个 birth 一起做，amortize overhead)

### Build 流程
- Build cmd: `cmd.exe /c "tmp\g1_test\cuda_build.bat neural-eco-protagonist > tmp\bld.log 2>&1"`
- Run cmd: `cmd.exe /c "build_ninja_cuda\bin\neural-eco-protagonist.exe --config XXX.toml > log 2>&1"`
- **WSL/MinGW 注意**: 用 `cmd.exe /c "..."` 一次性命令，避免 background shell 把 stdout 转 UTF-16
- **超时陷阱**: bash 后台 shell 默认 5 分钟超时（user 设的 600000 也被 clamp 5min），
  长 run 用 `cmd.exe /c "..."` 同步等待

### 新 layer 添加
- Routes 层 layer 放 `routes/protagonist_ecology/world/`，include 用 `world/Foo.h` 不带前缀
- CMakeLists.txt 必须加新文件名到 `PROTAGONIST_ECOLOGY_ROUTE_SOURCES` 和 `_HEADERS`
- Factory: `ProtagonistWorldFactory.cpp` 装配 layer (factory 函数有 const& config，
  layer 闭包要 capture by value 防止 lifetime 问题)

### Agent 字段扩展
- 加新字段必须在 `private:` section，避免破坏 ABI
- 涉及 NeatGenome 的 spawn 路径：保留 genome_archive_ unique_ptr
- 死亡分类：`killWithCause(DeathCause)` 新增，旧 `kill(const char*)` 保留

### Toml 配置坑
- 同一个 table 里不能有重复 key (e.g. `reproduction_check_interval`)
- `getOr<int>` 用于 size_t/uint32 字段需要显式 `static_cast<>`

---

## Layer 5 R2-R3 (剩余工作)

### R2: 完整 Building 闭环
当前 Layer 5 lite 只有 BuildingType enum + Shelter rain-immunity。要完成"建造闭环":
1. **配方** — 让 worksite 在 active 时声明 building_type，required_sticks/stones
   随类型变化（Shelter: 5 sticks + 3 stones; Wall: 3+2; Storage: 4+0; Lookout: 2+5）
2. **Storage 建筑** — agents 在 building 范围内自动 deposit carried sticks/stones
   到 BaseLayer 的全局库存（已有 BaseLayer 字段，加 isInStorageZone 判断）
3. **Lookout 建筑** — 给 inside agent 的 perception sense_radius +50%（需要改
   PerceptionLayer 接 WorksiteLayer 查询）
4. **Worksite 选择 building_type 的 capability** — 现在所有 worksite 默认 Wall。
   需要新加 ChoosBuildTypeCapability 让 brain 在 worksite 激活时输出 type 选择，
   或在 toml `[[worksite.recipes]]` 里随机/课程化决定。

### R3: 完整 Reward Shift
layer5.toml 已经把 reward 系数改了，但还需要长训验证。建议:
- 30 代 layer5.toml 跑完整训练，观察 builds (现在=0) 和 hunts 曲线
- 对比 v17 baseline 的 fitness：reward shift 的目的是降低 unmotivated survival fitness，
  期望 avg_fitness 略降但 builds/hunts 显著上升

---

## Layer 6 (defer) — 三层世界

设计 (FINAL_BLUEPRINT.md §6 + PRODUCTION_WORLD_LIFECYCLE_SPEC.md):
- **训练沙盒** — 现有的 episodic world，30 代后扔
- **边境世界** — 缓冲区，从训练沙盒"毕业"的 protagonist 在这里再考验 50 代，活下来才进主世界
- **主世界** — 永不 reset，只接受 (a) 从边境迁徙的成熟 protagonist (b) in-world 出生
  的下一代。**主世界灭绝处理**: 物种 lineage 全灭则 a generic juvenile 从外部"漂流"进
  入（fallback 防止永久缺类）

实现起步:
1. 加 `WorldRole enum { TrainingSandbox, Borderland, MainWorld }` 到 ProtagonistEcologyConfig
2. 三个 World2D 实例并行存活（main 持久化到 disk）
3. 迁徙调度器: 周期性 sample 边境优胜者写入 main world
4. main world 不 reset → episode 概念在 main 里用"按时间窗口" 记录 fitness

工作量: 估计 200-400 行新代码 + 大量集成测试。

---

## Layer 7 (defer) — Godot 3D viewer

资源已就位 (`routes/protagonist_ecology/data/kenney/`):
- viking_male/female (protagonist)
- cow, wolf, eagle (background animals)
- 5×trees, dead trees, bushes
- tent (Mongolian-yurt 视觉)
- bonfire, woodlog, axe

实现起步:
1. 设置 `routes/protagonist_ecology/viewer/godot_project/` (Godot 4.x)
2. 写 C++ side trace recorder 输出 Godot-friendly format (现有 TraceRecorder 改 frame-based)
3. Godot 加载 trace, 用 GDScript 渲染 agent + worksite + weather
4. (Stretch) live mode: C++ runtime 通过 IPC 流式发送 state 到 Godot

工作量: 巨大，可能整个新 session 才能成型 demo。

---

## Layer 8 (defer) — 高级机制

清单 (FINAL_BLUEPRINT.md §8):
- **Lamarckian** — Hebbian online weight update 在 episode 内累积，传给后代
- **World Model** — DreamerV2 风格的 transition predictor (RSSM)
- **DIAYN** — 内在动机 skill discovery
- **Emergent Communication** — VICE-style event-based reward + signal MI metric
  (已有 `scripts/protagonist_communication_mi.py`)
- **Lifelong learning** — protagonist DNC memory 不再 episode 间 reset

每个都是一个 sub-project，建议用户决定优先级后再展开。

---

## 当前 git 状态

```
25658fa Layer 5 (lite): BuildingType enum + Shelter rain-protection + reward shift toml
148b6d8 deep_protagonist S3.8: blueprint coverage for combat / hunting / shelter   ← Layer 4 stuff was bundled
9e7580d deep_protagonist S3.7: training-prerequisite mechanics complete
e3d2b8b Layer 3 partial: add Rabbit species (D3) + handoff doc
202186b Layer 2: WeatherLayer + 6 weather types + integration with movement/hydration/fire
7a098a6 Layer 1 tune: alive_rate 0 -> 0.79 (vs v17 baseline 0.45)
a0669f6 Layer 1: core_temperature + ThermalTickCapability + multi-stage debuff
2fea5a8 docs: FINAL_BLUEPRINT.md - project constitution (Layer 1-8 routes)
```

⚠️ **注意**: Layer 4 的实际代码 (PopulationLayer + Agent.h + BootstrapPopulation 等)
被 commit 148b6d8 一起带走了（同 root user 同时跑两个 routes 的 work）。下一次
会话开始前，先 `git show 148b6d8 --stat` 确认所有 Layer 4 文件齐全。

---

## 重启 session 的入口指令

```
1. cd /mnt/d/claude-code/c++ && git log --oneline -10
2. 读 docx/FINAL_BLUEPRINT.md (§5/§6/§7/§8 决定下一步)
3. 读 tmp/HANDOFF_LAYER5_TO_8.md (本文件)
4. 选择: 完成 Layer 5 R2-R3 / 开 Layer 6 / 开 Layer 7 / 开 Layer 8
5. 用户原话: "做到上下文到730k的时候就不要往下了"
```
