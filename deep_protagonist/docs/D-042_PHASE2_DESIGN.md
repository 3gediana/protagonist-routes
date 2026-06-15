# D-042 设计草稿 · Phase 2 机制深化

> Phase 1 (D-041) 跑完后看数据决定。如果 last 20 mean 稳定 > +50 → 进 Phase 2；如果 < +50 → 先调 Phase 1 参数。

## 用户原话（决策依据）

> "他可以耕种 → 是不是因为他有农场 → 是不是他需要去造农场 → 是不是他家里能种种 → 是不是他有植物 → 他可以去造植物 → 他得有种子 → 他能耕种是不是他得有出头 → 先把地给耕了 → 然后他长出来的食物是不是可以收获 → 然后存储起来 → 饲养也同样的道理"

## 设计原则

**不动 PPO 架构**（不加新动作通道，不变 obs 维度）。让 Phase 1 训练好的 checkpoint 能 `--load` 继续训。所有机制扩展通过：
- 复用现有 16 个动作（语义二段化）
- 扩 BuildingType 枚举（4 → 6）
- 在 RewardSystem 加新 milestone helpers
- Environment 内部加新 sub-system（pen / storage / hoe）

## A. 农耕完整链条

| 现有 | 扩展后 |
|---|---|
| plant_seed 直接造 plot（无门槛） | **plant_seed 二段化**：① 站非 tilled 草地 + Hoe → till（创建空 plot）② 站 tilled empty plot + Seed → 真种 |
| 不需要工具 | **craft_spear 二段化**：进入 mid stage 后，inventory 已有 spear → 造 hoe；否则造 spear |
| 没有作物种类 | 现有 PlantKind: Berry/Fruit/Mushroom/Tree → 加 Wheat / Carrot |
| 收获 +0.5 collect | 收获 +1（成熟）或 +20 milestone（first_crop） |
| 没有存储 | 见 B 节 |

新 milestone：
- first_hoe_crafted（craft_spear → hoe 成功）
- first_till（用 hoe 创造 tilled plot）
- first_planted_in_till（在 tilled 上 plant_seed）

## B. 存储系统

| 项 | 设计 |
|---|---|
| BuildingType 枚举 | Shed / Wood / Stone / Big → 加 **Storage** + **Pen** |
| 造箱子 | cycle_building_type 选 Storage → place_blueprint 放图纸 → deposit 4W → 完工成箱子 |
| StorageBox 数据 | `pos_x/y/z` + `slots[20]`（独立于 inventory） |
| 存物 | `deposit_to_site` 当站在 Storage 上 → 把 inventory 中所有物倒进 storage（按 free slot）|
| 取物 | （Phase 3 再做）—— 当前 storage 只单向收 |
| 死亡保留 | StorageBox 跨 episode 不删（蓝图原话"世界永不重置"）|

新 milestone：
- first_storage_built（box 完工）
- first_item_stored（首次往 box 倒物）

## C. 围栏 + 动物产出

| 项 | 设计 |
|---|---|
| 造围栏 | cycle_building_type 选 Pen → place_blueprint 放图纸 → deposit 8W + 4G → 完工 |
| Pen 数据 | 圆形区域 r=8m，注册到 PenSystem |
| Tame 加速 | 当 follower 在 Pen 内时 trust 累积速度 × 3 |
| 动物产出 | follower 在 Pen 内 + trust > 0.7：每 60s 产 1 资源到 agent inventory（rabbit→Grass/Wool, deer→Fur, wolf→Fur, fish→无）|

新 milestone：
- first_pen_built
- first_pen_follower（follower 进入 pen）
- first_resource_produced（动物自动产出第 1 个）

## D. RewardSystem 接口扩展

```cpp
// RewardSystem.hpp
void event_milestone_named(const char* name);  // 仍 +20 但带 tag
void event_animal_produce(ItemKind k);         // +0.5 / 资源
void event_storage_deposit();                  // +0.3 / 物
```

## E. obs 改动（小）

不加新维度（保 obs=564）。已有的 `selected_building` (4-onehot) 扩成 6-onehot 占现有位 → 维度 +2。`obs 564 → 566`。

或者：`selected_building` 的 onehot 维度从 4 改成 8（预留扩展），现有 4 位填 1，新加 4 位填 0 → obs 568 但 Phase 1 checkpoint 兼容（多余位 zero-pad）。

## 工作量估算

| 任务 | 开发 | 训练 |
|---|---|---|
| BuildingType 枚举 + cycle 扩展 | 30 min | — |
| StorageBox + StorageSystem | 1.5h | — |
| PenSystem + animal produce | 2h | — |
| Hoe / craft_spear 二段化 + plant_seed 二段化 | 1h | — |
| RewardSystem 接口扩展 + 6 个新 milestone | 1h | — |
| viewer key 绑定（不强制，box 模型占位）| 30 min | — |
| 100k smoke + verify | 30 min | — |
| 续训 5M 步（用 Phase 1 ckpt --load）| — | 4-5h |
| **合计** | **6-7h** | **4-5h** |

## 触发条件

进 Phase 2 必须满足：
1. ✅ Phase 1 ppo_run7 last 20 mean ≥ +50（破 🟧）
2. ✅ Phase 1 11 个 first_X 至少 6 个稳定触发
3. ✅ 用户确认想看更多过日子细节

如不满足 1，先调 Phase 1（可能加 thirst_decay 8 / first_X 权重微调 / 训更长）。

## 风险

| 风险 | 缓解 |
|---|---|
| StorageBox/Pen 跨 episode 持久化破坏 spawn 逻辑 | 跟 BuildingSystem 同位，已有跨 episode 持久化 |
| Hoe 二段化让 PPO 困惑（一个键多义）| 用 progression stage 区分（early=spear, mid=hoe）|
| 新 milestone 让 agent 主线 reward 上升过快 → drink/eat 边缘化 | 监控 r_water/r_food 不降至 0 |
| 5M 续训显存不够 | --save-every 200 + checkpoint 续训 |
