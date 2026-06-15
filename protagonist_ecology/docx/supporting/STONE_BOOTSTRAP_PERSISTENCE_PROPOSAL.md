# Stone Bootstrap 跨 Episode 持久化提案

> 调研日期：2026-05-25
> 范围：read-only 调研，**未实施**。
> 触发：3D BD long_run #10 + 2D BD long_run #11 都显示 stone deposit 是 build chain 的核心瓶颈。

## 1 · 现象

| 指标 | 3D BD 60-gen | 2D BD 60-gen | 备注 |
|---|---|---|---|
| stone_deposit / gen | 0-2 | 6-13 (峰 gen 28-32) | per episode 极低 |
| stone_unlock_rate ≈ 13/48 evals | < 5% | 27% (峰) | 即每 ≈73% 的 episode 完全没碰过 stone |
| chop_attempts | 100k+ | 100k+ | 链路前半段没问题 |

## 2 · 根因

两段强耦合：

### (a) Training role bootstrap 0 stones · `ProtagonistWorldFactory.cpp` line 121

```cpp
object.type = (live_starter_stone || live_stone) ? ObjectType::Stone : ObjectType::Stick;
```

`live_starter_stone` 和 `live_stone` 都需要 `live_role = true`。Training role 下 `live_role = false`，所以 bootstrap 全是 Stick，**0 stones**。

### (b) NurseryProgressionLayer 每 episode reset · `NurseryProgressionLayer.h` line 84-88

```cpp
bool first_fire_ignited_ = false;
bool worksite_unlocked_ = false;
bool stone_unlocked_ = false;
bool heavy_unlocked_ = false;
bool second_fire_ignited_ = false;
```

每 `createBootstrapWorld(...)` 都 new 一个 NurseryProgressionLayer，所有 unlock flag reset 到 false。Episode 结束后下 episode 必须从 zero 重新 deposit ≥ 6 sticks 才会触发 `ScatterStone` 撒 10 个石头。

### (c) Episode 时长不足

Episode = 4900 ticks ≈ 80 sim seconds。2D BD 平均 deposit_per_eval ≈ 3.2，**低于 unlock threshold 6**。所以 majority of episodes 全程没 stones 可用。

## 3 · 方案对照

### A · Quick win · 改 toml unlock threshold = 0

| 项 | 评估 |
|---|---|
| 改动 | 1 行 toml |
| 风险 | 失去 curriculum 渐进，stone 一启动就在场 |
| 验证成本 | 1 个 60-gen 长跑 |
| 推荐 | 短期实验可用，长期非最优 |

### B · Training bootstrap N initial stones

| 项 | 评估 |
|---|---|
| 改动 | `seedRouteObjects` + ProtagonistEcologyConfig 新字段 |
| 风险 | 启动有少量 stones 但保留"deposit 6 解锁更多" curriculum |
| 验证成本 | 中等 |
| 推荐 | 较平衡，但仍 per-episode reset，protagonist 还是要重学 deposit |

### C · per-genome unlock state

| 项 | 评估 |
|---|---|
| 改动 | 大 — genome blob 加 curriculum 字段 |
| 风险 | unlock 是环境 / curriculum 一侧的事，不是 genome 一侧；语义错位 |
| 推荐 | **不推荐** |

### D · archetype-level unlock state（推荐）

| 项 | 评估 |
|---|---|
| 改动 | ProtagonistEcologyRuntime 维护 `ArchetypeProgressionState`，每代结束观察 / merge，下代每 episode 注入 NurseryProgressionLayer 初始化 |
| 风险 | 中等 — 需要 thread-safe merge（多 episode 并行） |
| 验证成本 | 1 个 60-gen 长跑 + unit test |
| 推荐 | **首选**，与 MainWorldPersistence "经验持久化" 哲学一致 |

## 4 · 方案 D 详细设计

### 4.1 新数据结构

```cpp
// runtime/ProtagonistEcologyRuntime.h
struct ArchetypeProgressionState {
    bool first_fire_ignited = false;
    bool worksite_unlocked = false;
    bool stone_unlocked = false;
    bool heavy_unlocked = false;
    bool second_fire_ignited = false;
    std::size_t cumulative_deposits = 0;  // 跨 episode 累计
    std::size_t cumulative_stone_stock = 0;
};
// 9 个 archetype 各持一份；protagonist (tag=0) 才真正使用
std::array<ArchetypeProgressionState, 9> archetype_progression_;
```

### 4.2 NurseryProgressionLayer 注入 API

```cpp
// world/NurseryProgressionLayer.h 新增
void seedFromProgressionState(const ArchetypeProgressionState& state);
```

`createBootstrapWorld(...)` 接收 optional progression state，构造完 layer 后调 `seedFromProgressionState`。

### 4.3 每代结束 merge

Episode loop 收完后从 NurseryProgressionLayer 读最新 state（getter API），逐项 OR-merge 到 archetype state（unlock 是 monotonic，true 永不变 false）。

### 4.4 toml gate

```toml
[runtime.curriculum]
archetype_progression_persistence_enabled = false  # 默认 off
```

默认 off → 完全向后兼容。

### 4.5 MainWorldPersistence 顺路扩展（可选）

`ArchetypeProgressionState` 可以一并塞进 `MainWorldSnapshotMetadata` 的 JSON manifest（已是 `2e24abf` 提交的结构），实现跨进程 / 跨会话持久化。

## 5 · 实施路径

1. **commit 1**：`ArchetypeProgressionState` 结构 + NurseryProgressionLayer getter + setter + 2 unit test（roundtrip + merge OR semantics）
2. **commit 2**：`createBootstrapWorld(config, &progression_state)` 重载 + Runtime 维护 merge loop + toml gate + 1 integration test（2 episode：ep0 unlock stone，ep1 检查 stone 已在场）
3. **commit 3**：把 `ArchetypeProgressionState` 塞进 `MainWorldSnapshotMetadata` JSON + 1 test（save / load roundtrip）

预计：commit 1 ≈ 60 行 / commit 2 ≈ 120 行 / commit 3 ≈ 50 行，总计 < 250 行 + 4 test。

## 6 · 与现有 spec 的关系

- 与 `PRODUCTION_WORLD_LIFECYCLE_SPEC.md` invariant #1（主世界永不 reset）一致 — archetype 知识等价于物种集体记忆，应当跨 episode 累积。
- 与 ADR-0004（渐进添加）一致 — toml gate 默认 off，可在 baseline 稳定后再启用。
- 与 SELF_GROW_CAPABILITY_LIMITS_SPEC.md L0-L7 分层一致 — 这属于 L1（环境参数学习）而非 L7（凭空新 capability）。

## 7 · 不在本提案范围

- 改 unlock 阈值数值（属于调参，应单独实验）
- 改 episode 时长（影响面广，独立讨论）
- 把 unlock state 加进 genome blob（方案 C，**明确不推荐**）
