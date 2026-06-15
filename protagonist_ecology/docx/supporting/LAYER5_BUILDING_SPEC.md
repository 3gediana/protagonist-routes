# Layer 5 · 建筑闭环设计规格 (FINAL_BLUEPRINT.md §5)

> 规格快照: 2026-05-24 (Layer 5 lite 完成于 commit da86050 + 1377352)
>
> 本文档分为：(A) 已完成 lite 部分，(B) 待完成 R2/R3 详细设计，(C) Reward
> paradigm shift 长训观察预期。

---

## A. 已完成 (Layer 5 lite)

### A.1 BuildingType 类型枚举 (commit da86050)

`routes/protagonist_ecology/world/WorksiteLayer.h`:

```cpp
enum class BuildingType : std::uint8_t {
    Wall    = 0,  // 默认；blocksPredatorAt 仅对此类型生效
    Shelter = 1,  // 雨天遮蔽；inside 时不收 hydration buff
    Storage = 2,  // 自动堆栈材料（R2 待实现）
    Lookout = 3,  // 感知半径加成（R2 待实现）
};
```

`Worksite` 加 `building_type{BuildingType::Wall}` 字段（默认 Wall 保留旧行为）。

### A.2 API 扩展 (commit da86050)

```cpp
std::optional<Worksite> WorksiteLayer::buildingAt(Vec2 pos, BuildingType type) const;
bool WorksiteLayer::isShelteredAt(Vec2 pos) const;  // = buildingAt(pos, Shelter).has_value()
```

`blocksPredatorAt(pos)` 加了 `building_type == Wall` 的条件。

### A.3 SurvivalStatusLayer Shelter 雨天免疫 (commit da86050)

```cpp
// rain_recovery 在 sheltered agent 上设为 0
if (rain_recovery > 0.0f && worksite_layer->isShelteredAt(agent->position())) {
    rain_recovery = 0.0f;
}
```

设计意图：让 Shelter 不是"免费胜"——下雨天 sheltered agent 失去 hydration 自然回复，
要么离开 Shelter 沾雨水补水，要么先期储水。激励 Storage 建造（储水）。

### A.4 Reward Paradigm Shift (`tmp/layer1/layer5.toml`)

| 参数 | 旧值 | 新值 | 设计 |
|---|---|---|---|
| survival_weight | 1.0 | **0.3** | "没死"本身奖励降到 30% |
| food_weight | 10.0 | **3.0** | 食物奖励降到 30%，主要靠 drive penalty |
| worksite_completion_reward | 60 | **150** | 建造价值 2.5x，是高级成就 |
| cooperation_weight | 6.0 | **12.0** | 协作 2x |
| communication_weight | 4.0 | **8.0** | 通信 2x |
| signal_co_attendance_reward | 0.6 | **1.5** | 信号共同响应 2.5x |
| joint_hunt_reward | 8.0 | **18.0** | 联合狩猎 2.25x |

### A.5 验证数据

- 3 代 short run: avg_fit 751→1093, best 3713→10336
- 30 代 long run: 启动于 commit da86050 后 (background, run_l5_long.log)，
  预期产生 fitness 曲线 + builds/hunts/signals 计数

---

## B. R2 待完成: 完整建筑闭环

### B.1 Worksite building_type 决定权

当前所有 worksite 默认 Wall。需要让 worksite 在创建 / 激活时选择 type:

#### 方案 1: toml 显式声明
```toml
[[worksite.recipes]]
building_type = "shelter"
required_sticks = 5
required_stones = 3
weight = 0.3       # 30% 的 worksite 是 Shelter

[[worksite.recipes]]
building_type = "storage"
required_sticks = 4
required_stones = 0
weight = 0.2

[[worksite.recipes]]
building_type = "lookout"
required_sticks = 2
required_stones = 5
weight = 0.1

[[worksite.recipes]]
building_type = "wall"
required_sticks = 3
required_stones = 2
weight = 0.4
```

`WorksiteLayer::seedSites` 按 weight 抽样选 type。每个 site 有不同 required 配方。

#### 方案 2: Brain 决定（涌现）
让 protagonist 输出一个 4-dim "building selector" head，
worksite 创建时收集 nearby protagonists 的 selector 平均值决定 type。
**这是真涌现路线但难调**，先做方案 1。

### B.2 Storage 自动堆栈

```cpp
// 新加 absorbToCompletedStorages() 在 tick() 末尾调用
void WorksiteLayer::absorbToCompletedStorages(IWorld& world) {
    if (objects_ == nullptr || agents_ == nullptr) return;
    for (auto& site : sites_) {
        if (!site.completed || site.building_type != BuildingType::Storage) continue;
        for (auto* agent : agents_->agents()) {
            if (agent == nullptr || !agent->alive() || !agent->isCarrying()) continue;
            if (Vec2::distanceSquared(agent->position(), site.position) > site.radius * site.radius) continue;
            auto* obj = objects_->findById(agent->carriedObject());
            if (obj == nullptr) continue;
            // 累加到 Storage 的"二级库存"，不限上限
            if (obj->type == ObjectType::Stick) ++site.stored_sticks_overflow;
            else if (obj->type == ObjectType::Stone) ++site.stored_stones_overflow;
            else continue;
            agent->setCarriedObject(0);
            // event publish, agent 加小 reward
        }
    }
}
```

需要在 `Worksite` 加 `stored_sticks_overflow` / `stored_stones_overflow` 字段。

### B.3 Lookout 感知半径加成

让 perception layer 查询 `worksiteLayer.buildingAt(pos, Lookout)`，
inside agent 的 sense_radius × 1.5。

### B.4 Reward 接线

- `worksite.completion_reward` 按 building_type 不同：Wall=60, Shelter=120, Storage=80, Lookout=100
- agent inside Storage 时每秒 +0.05 reward
- agent inside Lookout 看到新预警目标时 +0.5 reward

### B.5 视觉资源映射

(Kenney 资源已就位 in `data/kenney/`):
- Wall: stacked logs/stones (默认现有 visual)
- Shelter: tent (Mongolian-yurt-level，已有)
- Storage: woodlog stack
- Lookout: tall structure (需要 Mixamo / 自建)

---

## C. R3 待完成: Reward Shift 长训验证

### C.1 实验设计

baseline = `layer4.toml` (旧 reward 系数)
treatment = `layer5.toml` (新 reward shift)

跑 30 代各 3 seed (5h 总时间)。对比指标:
- avg_fitness (期望降低 — 正常 — 因为不再奖励"miracle survive")
- builds/gen, hunts/gen, signals/gen (期望显著上升)
- Lineage 多代繁殖率 (max_living=200，看到第几代)

### C.2 假设

| 假设 | 数据预期 |
|---|---|
| H1 | layer5 avg_fitness < layer4 avg_fitness（reward 权重重整） |
| H2 | layer5 builds/gen >> layer4 (worksite_completion 150 vs 60) |
| H3 | layer5 cooperation events >> layer4 (signal_co 1.5 vs 0.6) |
| H4 | layer5 protagonist 的 build/cooperation 行为延迟数代再涌现 |

### C.3 失败回退

如果 H1 太激进（avg_fitness 降到 < -2000），考虑:
- survival_weight 0.3 → 0.5 (中等)
- food_weight 3.0 → 5.0 (中等)
- 重跑

---

## D. 决策记录

### D.1 为什么不用 RL fine-tune？
NEAT 是 evolutionary，没有 backprop。reward 直接进 fitness sum。
shift 等于"选择压力"重定向。

### D.2 为什么不让 brain 选 building_type？
方案 2 涌现更纯但训练时间不够。lite 用 toml 配方可控，等 30 代验证 reward
shift 工作之后再考虑。

### D.3 Layer 5 与 Layer 6 的接口
Layer 6 三层世界——主世界永不 reset，建筑会累积。Layer 5 的 worksite 系统
要保证序列化/反序列化保留 building_type 和 storage_overflow。
具体在 Layer 6 实现时落地。
