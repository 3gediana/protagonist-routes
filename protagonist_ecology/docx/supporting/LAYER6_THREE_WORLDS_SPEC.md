# Layer 6 · 三层世界设计规格 (FINAL_BLUEPRINT.md §6)

> 状态: 设计阶段（实现 deferred to next session）
> 依据: FINAL_BLUEPRINT.md §6 + PRODUCTION_WORLD_LIFECYCLE_SPEC.md 17 invariants

---

## 1. 三层架构

```
[训练沙盒] → [边境世界] → [主世界（永不 reset）]
   episodic       semi-       persistent
   30-100 代       persistent    save to disk
   随机种子        50 代        episode = day
   reset = clear    reset =
   生命              loose
                  newcomers
                  back to
                  sandbox
```

### 1.1 训练沙盒 (TrainingSandbox)
- 现有 protagonist_ecology 的 episodic world
- 每 episode 重置 → 物理重生
- 跑 30-100 代 NEAT evolution
- 优秀基因（top-k by fitness）"毕业"到边境

### 1.2 边境世界 (Borderland)
- 半持久化：episode 结束不全 reset，但弱者 cull 回 sandbox
- 50 代考验：物种延续 + 不灭绝才进入主世界
- 数量限制：每物种 5-10 founder
- 时间速度同 sandbox（compressed-time）

### 1.3 主世界 (MainWorld)
- **永不 reset**（PRODUCTION_WORLD_LIFECYCLE_SPEC invariant #1）
- save to disk every N sim minutes (R-008 self-grow)
- episode 概念变为"按时间窗口"记录 fitness
- 新血脉只能通过 (a) 边境迁徙 (b) in-world 出生
- 物种全灭 → 触发 fallback "外部 juvenile drift in"
- 时间 1:1（real time）或 1:60 (1 wall sec = 1 sim min)

---

## 2. 实现路径

### 2.1 ProtagonistEcologyConfig 加 WorldRole

```cpp
enum class WorldRole : std::uint8_t {
    TrainingSandbox = 0,   // existing behavior
    Borderland      = 1,
    MainWorld       = 2,
};

struct WorldConfig {
    WorldRole role{WorldRole::TrainingSandbox};
    bool persistent_state{false};                     // save/load on close
    std::filesystem::path persist_dir;                // where to save
    SimTimeSeconds save_interval_seconds{600.0};     // save every 10 min
};
```

### 2.2 ProtagonistEcologyRuntime 三个 World 实例

```cpp
class TripleWorldRuntime {
public:
    void runFrame() {
        sandbox_.tick(dt);
        borderland_.tick(dt);
        main_world_.tick(dt);
        scheduleMigrations();  // promote/demote agents between worlds
    }
private:
    World2D sandbox_;
    World2D borderland_;
    World2D main_world_;  // 永不 reset
    MigrationScheduler migration_;
};
```

### 2.3 迁徙调度器

```cpp
class MigrationScheduler {
    // sandbox -> borderland: 每代结束，top-K fitness genome 迁出
    void promoteSandboxToBorderland(World2D& sandbox, World2D& border);
    // borderland -> main: 每 50 代，存活的 lineage 迁入
    void promoteBorderlandToMain(World2D& border, World2D& main);
    // main -> NULL: 物种灭绝 → fallback 外部 juvenile
    void rescueExtinctSpecies(World2D& main);
};
```

### 2.4 主世界存档

```cpp
// 序列化到 protobuf / FlatBuffers / 自定义二进制
struct MainWorldSnapshot {
    std::uint64_t snapshot_id;
    SimTimeSeconds sim_time;
    std::vector<AgentSnapshot> agents;       // genome + lineage + position + drives
    std::vector<WorksiteSnapshot> worksites; // building_type + progress + storage
    std::vector<TreeSnapshot> trees;
    std::vector<WaterPondSnapshot> waters;
    WeatherType current_weather;
    SeasonType current_season;
    // ... all layer states
};

class MainWorldPersistence {
    void save(const World2D&, std::filesystem::path);
    World2D load(std::filesystem::path);
};
```

存档考虑:
- Agent::genome_archive_ 每个 agent 1.4MB (protagonist) — 存档时 dehydrate
  到 disk format
- Agent::brain_ 状态（DNC memory bank）需要每个 agent 序列化
- 建筑全部 building_type + 进度持久化

### 2.5 主世界 episode 概念

不再 reset 怎么算 fitness？
- 每 N 分钟（配置）记一次 "snapshot fitness window"
- agent 在该窗口内的 fitness delta 作为 evolution 信号
- 但**不强制**全员重置 — fitness 只用于 NEAT crossover (cross-window) 选择

---

## 3. 边境 → 主世界迁徙规则

### 3.1 进入条件
- 边境内存活 50 代
- 谱系存活率 ≥ 30% (lineage descendants > 0.3 * founder count)
- 物种 fitness ≥ baseline 平均 1.5x

### 3.2 退出条件
- 主世界物种全灭 → 边境复活（不退回 sandbox）
- 主世界 protagonist 数量 < 5 → emergency 边境迁徙

---

## 4. PRODUCTION_WORLD_LIFECYCLE 17 invariants 检查表

| # | Invariant | Layer 6 实现保证 |
|---|---|---|
| 1 | 主世界永不 reset | TripleWorldRuntime::main_world_ 不 destroy |
| 2 | 训练永远在别处做 | sandbox 是 evolutionary 训练唯一位点 |
| 3 | 新血脉只能通过出生或迁徙 | MigrationScheduler 严格控制 |
| 4 | 灭绝 → fallback drift | rescueExtinctSpecies |
| 5 | 种群不能从 0 重启 | 主世界 fallback 必有外部 juvenile |
| 6 | 谱系流动单向 | sandbox → borderland → main，反向只在 fallback |
| 7-17 | (详见 PRODUCTION_WORLD_LIFECYCLE_SPEC.md) | (本 spec 实现需逐条检查) |

---

## 5. 工作量估计

| 阶段 | 文件改动 | 估算行数 | 复杂度 |
|---|---|---|---|
| WorldRole + WorldConfig | 2 files | ~30 | low |
| TripleWorldRuntime (skeleton) | 2 new + 1 modified | ~200 | medium |
| MigrationScheduler | 2 new | ~250 | medium |
| MainWorldPersistence | 3-5 new files | ~500-800 | high |
| Agent/Worksite/Tree snapshot serialize | scattered | ~300 | medium |
| Migration tests | 3-5 new test files | ~400 | medium |
| **总计** | **15-20 files** | **~1700-2000 行** | high |

---

## 6. 优先级建议

按 user vision 倒推:
1. **优先做** MigrationScheduler 雏形（sandbox → borderland 简单 promote）
   而非完整持久化。证明三层流动管线工作。
2. **次优先** MainWorld 不 reset 但内存中（不存盘）。
3. **最低优先** 序列化到 disk（user 明确要"主世界永不 reset" — 内存
   生命周期内永不 reset 已经覆盖核心需求）。

---

## 7. 与 Layer 5/7/8 的接口

### Layer 5 (Building)
- 主世界 worksite 持久化保留 building_type + progress
- 跨 world 边界后 worksite stockpile 清空（同物理位置语义保留）

### Layer 7 (Godot Viewer)
- viewer 可同时观察三个世界（多窗口或 tab）
- main world replay 从存档恢复

### Layer 8 (Lamarckian)
- 存档恢复时 Hebbian-modified weights 也需保存
- DNC memory bank 是否跨 episode 保存 — 主世界为是，沙盒 episode 边界仍 reset
