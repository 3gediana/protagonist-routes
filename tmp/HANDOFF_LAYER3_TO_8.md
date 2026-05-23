# Handoff · Layer 3 → Layer 8 续作清单

> 给 compact 后继续工作的 AI。**先读这份**。
>
> 创建时间: 2026-05-24 02:50
> 上下文: protagonist_ecology / FINAL_BLUEPRINT.md Layer 1-8 工程线

---

## 立刻读什么

按优先级:
1. `routes/protagonist_ecology/docx/FINAL_BLUEPRINT.md` — 项目宪法 (499 行)
2. 本文件 — 当前状态 + 待做
3. `AGENTS.md` — 协作规则 (root)

---

## 已完成 (5 个 commit)

```
202186b Layer 2: WeatherLayer + 6 weather types
7a098a6 Layer 1 tune: alive_rate 0 -> 0.79
a0669f6 Layer 1: core_temperature + Thermal + multi-stage debuff
2fea5a8 docs: FINAL_BLUEPRINT.md project constitution
6f36821 L2.9 v18: CVT-MAP-Elites archived as failed
```

### Layer 1 完成 (alive 0.45 → 0.79):
- Agent.h: `core_temperature_` 字段
- src/core/world/layers/TimeOfDayLayer.h/cpp: 加 phaseSin/Cos/visibilityFactor/ambientTemperatureDelta
- routes/protagonist_ecology/capability/ThermalTickCapability.h/cpp: 体温整合 (season + day_night + fire warming + metabolism + radiative loss)
- routes/protagonist_ecology/world/SurvivalStatusLayer.h/cpp: 加 hunger + temperature damage 多阶段 debuff + track_all_living=true (R-001)
- BootstrapPopulation.cpp: 4 个 agent factory 都 attach ThermalTickCapability
- 600×600 world (toml: tmp/layer1/layer1.toml)

**关键调参** (avoid alive=0):
```cpp
hunger_damage_threshold = 0.9   // was 0.7 - too harsh
hunger_damage_per_second = 0.08 // was 0.25
cold_temp_threshold = 25        // was 32
hot_temp_threshold = 45         // was 42
thermal_damage_per_second = 0.15 // was 0.4
```

```toml
# 600×600 world 需要 water 密度高
water source_count = 16          # 8 → 16
water spawn_radius_min = 30      # 54 → 30 (close to base)
initial_hydration_ratio = 0.85   # 0.68 → 0.85 (more buffer)
hydration_decay_per_tick = 0.035 # 0.060 → 0.035 (water further)
```

### Layer 2 完成 (avg_fitness +167%):
- src/core/world/layers/WeatherLayer.h/cpp: 6 weather types, season biases, accessors (moveCostMultiplier / hydrationRecovery / signalDiffusion / lightningPerTreePerSecond / treeFellProbability)
- src/core/perception/WeatherPerception.h/cpp: 6-dim one-hot (background only, protagonist 不变 sensory_dim)
- src/core/capability/MoveCapability.cpp: 加 weather_cost
- routes/protagonist_ecology/world/SurvivalStatusLayer.cpp: 雨自动恢复 hydration
- routes/protagonist_ecology/world/CampfireLayer.cpp: 雷暴/台风期间概率点燃 dormant fire (igniteNextDormant aggregate sampling)
- routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.h/cpp: WeatherConfig + toml read
- routes/protagonist_ecology/runtime/ProtagonistWorldFactory.cpp: attach WeatherLayer
- BootstrapPopulation: WeatherPerception attach + **CRITICAL: computeBackgroundSensoryDim 同步 +6** (我第一次忘记这步导致 sensory_dim mismatch crash)

### Layer 3 进行中 (rabbit 已加，30 代 train 跑到 gen 17):
- BootstrapPopulation.h: `enum SpeciesArchetype` 加 `Rabbit`
- BootstrapPopulation.cpp:
  - speciesArchetypeName / parseSpeciesArchetype 加 "rabbit"
  - isHerbivoreArchetype 包含 Rabbit
  - backgroundOutputDim 加 Rabbit case (Move 2 + Eat 0 + Flee 1 = 3)
  - makeNeatSpeciesAgent 加 Rabbit case (initial_energy=50, MoveCapability 2.8 fast, FleeCapability 8.0 strong, damage_threshold=0 single-bite)
  - drive setpoint loneliness=0 (colony), eat efficiency grass=1.0 strict herbivore
- tmp/layer1/layer1.toml: 加 `[[population.background.species]] archetype="rabbit" count=20`

**Layer 3 训练数据 (run dir: data/runs/layer1/20260524-023836/, gen 0-17)**:
- gen 17 avg_fitness=3528, best=8343 (比 v17 baseline 高很多)
- Build 通过, system stable
- 待: 等到 gen 30 完成 + 完整对比

---

## 关键技术经验 (避免重踩)

### 1. CMake build 系统
- Binary: `D:/claude-code/c++/build_ninja_cuda/bin/neural-eco-protagonist.exe`
- Build cmd: `cmd.exe /c "tmp\g1_test\cuda_build.bat <target>"`
- target: `neural-eco-protagonist` (执行) / `neural_eco_route_protagonist` (lib)
- src/core 用 GLOB_RECURSE: 新 .cpp 自动 picked up
- routes/protagonist_ecology 显式 list: 新 .cpp 必须加到 `routes/protagonist_ecology/CMakeLists.txt`
- WSL bash 用 `/mnt/d/claude-code/c++/...` 路径

### 2. Layer dependency rules (硬约束)
- src/core 不能 include routes/* (route-local 文件)
- 我把 ThermalTickCapability 误放 core 后改成 route-local 解决
- 跨 layer query: 用 `world.getLayer<X>()` 而非 include

### 3. Sensory dim 一致性 (BUG TRAP!)
- **加 perception 必须同时加 sensory_dim 计算!**
- 我加 WeatherPerception 到 background factory 但忘了 `computeBackgroundSensoryDim()` 加 6 → crash gen 0
- 类似的 protagonistSensoryDim 也要同步 (但 protagonist 没加 weather perception 保持 brain shape)

### 4. Config template 实例化
- `Config::getOr<T>` 是 explicit instantiation
- 已实例化: bool, int, int64_t, std::size_t, float, double, std::string
- **没有 unsigned int / std::uint32_t** → 用 int 读再 cast

### 5. Build truncation
- cmd.exe 重定向有时截断输出, 但 build 实际成功. 检查 binary timestamp:
  `ls -la /mnt/d/claude-code/c++/build_ninja_cuda/bin/neural-eco-protagonist.exe`
- Link 错误用 UTF-16LE encoding (中文 MSVC), 用 `xxd` 看 hex 才能 decode

### 6. 每跑 30 代 ~5-10 分钟 (CUDA build, 24 protagonist + 89 background, 1200 ticks/ep)

---

## 当前 git status (未 commit)

需要 commit Layer 3 (rabbit) — 应在 Layer 3 train 完成后:
```
M routes/protagonist_ecology/runtime/BootstrapPopulation.cpp (rabbit)
M routes/protagonist_ecology/runtime/BootstrapPopulation.h (Rabbit enum)
M tmp/layer1/layer1.toml (rabbit species + L1 tuning)
```

---

## TODO 清单 (Layer 4-8)

### Layer 3 收尾 (~30 分钟)
- [ ] 等 Layer 3 train 完成 30 代 (run dir 20260524-023836)
- [ ] 完整 fitness/behavior 对比 (vs v17 / Layer 1 / Layer 2)
- [ ] 验证 ecology balance: rabbit 没被吃光 + predator 没饿死
- [ ] **Commit Layer 3** (rabbit 加完整)

### Layer 4: 生命周期 + 繁殖 + 谱系 (R-006, 4-5 项工作)
- [ ] Agent.h 加 lifecycle stage enum (Juvenile/Adult/Elder)
- [ ] Agent.h 加 birth_time + lineage_id + parent_ids + generation 字段
- [ ] AgeCapability (新): tick 推进 age, 转换 stage, 影响 move_speed/combat/breed
- [ ] BreedCapability (新): 配对条件 + crossover + mutation + 后代入世
- [ ] DeathClassifier: 5 death types (老死 / 病死 / 战死 / 生理压力死 / 灾难死) — 死时设 cause_of_death
- [ ] DiseaseLayer 强化: contact infection + 尸体污染 + 年龄/营养易感性 (R-006)
- [ ] LineageLogger (新 observer): 每出生/死亡写 lineage trace
- [ ] toml 加 [population.lifecycle] 配置 (juvenile_to_adult_age, adult_to_elder_age, max_age, breed_cooldown, breed_distance, ...)
- [ ] 30 代 smoke 验证 (繁殖率 / 代际差异 / 谱系树深度)
- [ ] **Commit Layer 4**

### Layer 5: 建造闭环 + Reward paradigm shift (核心 vision 4.6, ~5-7 项)
- [ ] WorksiteLayer 视觉升级: 5 种建筑 (围墙 / 茅草顶 / 蒙古包 / 火堆 / 储物堆) — 完工时变成 collidable + visible mesh hint
- [ ] StigmergicPerception (新): 堆料数量 / 尸体数量 / 足迹密度 进 protagonist perception
- [ ] HER-style fitness relabel: 在 ProtagonistEcologyRuntime episode end 时识别 implicit goals (wall_formed, sheltered_at_night, predator_wave_survived) → 加 outcome bonuses
- [ ] **Reward paradigm shift**: toml [reward] 14 个 scalar → outcome-level only (survival + group_extinction + outcome_bonuses)
- [ ] Autocurricula: predator wave 强度按 generation 递增
- [ ] 30 代 smoke + 看 vision 4.6 高光 (建墙挡狼 + 围火睡觉)
- [ ] **Commit Layer 5** (这是项目核心高光)

### Layer 6: 三层世界 (PRODUCTION_WORLD_LIFECYCLE_SPEC, 大工程)
- [ ] MainWorldRuntime: 永不 reset, 热保存/热恢复
- [ ] FrontierWorldRuntime: 自然死亡, 仅维护期 reset
- [ ] migration entry: 边境 → 主世界, ≤ 3 个体, 地图边缘出现
- [ ] incident detector: 大规模死亡 / 瘟疫 / 生态位塌陷, 输出结构化数据
- [ ] incident → challenge 翻译模板 (1-2 类)
- [ ] lineage_archive 持久化
- [ ] toml 加 [main_world] / [frontier_world] 配置
- [ ] **Commit Layer 6**

### Layer 7: Godot 3D viewer (Kenney + Mixamo + replay + live)
- [ ] Godot 4 项目结构: routes/protagonist_ecology/viewer/godot_demo/ 已经有雏形
- [ ] 已有资源: Viking Male/Female (protagonist), Cow/Wolf/Eagle (animals), 5 CommonTree+4 DeadTree+Bush+Clover (plants), Tent+Bonfire+Torch+WoodLog (buildings)
- [ ] 需补: Sheep, Tiger/Bear (apex), Rabbit, Stone variants
- [ ] trace.jsonl loader + 时间轴播放 (replay mode)
- [ ] socket/IPC 实时推送 (live intervention mode)
- [ ] 第一人称 / 跟随 / 上帝视角
- [ ] Overlay: 个体生理状态条 / 谱系树 / me_coverage 视图
- [ ] AI procedural mesh: 蒙古包升级版 + 储物堆动态
- [ ] **Commit Layer 7**

### Layer 8: 高级机制 (持续)
- [ ] **Lamarckian** 跨代记忆继承: 父代 DNC 关键 slot 传子代
- [ ] DIAYN-style L4 技能分化
- [ ] L5 emergent communication 任务驱动
- [ ] L6 atomic capability primitives (Touch/Strike/Launch)
- [ ] World Model (Ha & Schmidhuber 2018) 作为 protagonist 额外 perception
- [ ] 完整 incident → challenge → 训练 → 边境 → 迁徙闭环 (W5+W6)
- [ ] 建筑 damage + 维护 (D4 升级)

---

## 蓝图决策对应表

FINAL_BLUEPRINT.md §5 决策已全部按"效果最好"路线确定:
- D1 600×600 ✓
- D2 80分钟一年 ✓ (toml season_length_seconds=1200, 4 day/season × 1200 ticks/day)
- D3 加 rabbit ✓ (Layer 3 已加)
- D4 Layer 5 单向 + Layer 8 加破坏 (Layer 5 PENDING)
- D5 Lamarckian 纳入 Layer 8 (PENDING)
- D6 Live intervention 完整 UI 在 Layer 7 (PENDING)
- D7 不设答辩 deliverable, 全部 8 层完成

---

## 已知 issues / 偏移

- v17 baseline reward 块仍在 layer1.toml (drive_weight=5, 14 个 behavior reward) — Layer 5 reward paradigm shift 才会改
- 行为指标 chops/crafts/worksites 在 Layer 1+2 都低于 v17 baseline (但 fitness/alive 远高于) — 是 reward 通路 + 行为分布 redistribution, 不是机制坏. Layer 5 reward shift 应该 normalize.
- alive 在 Layer 2 (0.61) 略低于 Layer 1 (0.79) — 可能因为 weather 加 stress (wind move cost / storm fires); fitness avg +99% 抵消之
- protagonist 没加 WeatherPerception (避免改 brain shape) — 仅通过 hydration/energy 间接感知

---

## 启动下次工作的快捷命令

```bash
# Recent runs
ls -lt /mnt/d/claude-code/c++/data/runs/layer1/ | head -5

# Continue Layer 3 train if not done
cd /mnt/d/claude-code/c++ && ./build_ninja_cuda/bin/neural-eco-protagonist.exe --config tmp/layer1/layer1.toml

# Build
cmd.exe /c "cd /d D:\claude-code\c++ && tmp\\g1_test\\cuda_build.bat neural-eco-protagonist"

# Comparison Python
cd /mnt/d/claude-code/c++ && python3 -c "
import csv
def load(p): return list(csv.DictReader(open(p)))
def avg_last(rows, k, n=10): return sum(float(r[k]) for r in rows[-n:]) / n
def avg_per(rows, k, n=10): return sum(int(r[k]) for r in rows[-n:]) / n / 72
# add comparison logic
"
```

---

## 风格 + 协作约束 (CLAUDE.md)

- 简洁直接, 不画饼
- 一次只执行一件事
- 不擅自改 claude code config
- bash on Windows: cmd.exe /c "..." for batch
- 编辑 toml/cpp 直接写, 不要 sleep 太久 (max sleep 5 分钟)

---

完。继续按 Layer 3 收尾 → Layer 4-8 顺序工作。
