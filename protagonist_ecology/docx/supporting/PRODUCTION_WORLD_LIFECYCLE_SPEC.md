# Production World Lifecycle Spec · 主世界本体论与谱系流动

> 位置：`routes/protagonist_ecology/docx/supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md`
>
> 创建：2026-05-22
>
> 作用：R-002 / R-006 / R-007 / 3D viewer 共同地基。与不变量冲突的设计不得进入实现。

---

## 0. 立场

- 主世界永不 reset
- 训练永远在别处做
- 新血脉只能通过出生或迁徙进入主世界

---

## 1. 三层世界

| 层 | reset | episode 化 | 选择压力 | 用户可见 | 用户可干预 |
|---|---|---|---|---|---|
| 训练沙盒 | 是 | 是 | fitness | 否 | 否 |
| 边境世界 | 仅维护期 | 否 | 自然死亡 | 次要 | 受限 |
| 主世界 | **绝不** | 否 | 自然死亡 | 主要 | 受白名单约束 |

```
训练沙盒 ──候选 lineage──▶ 边境世界 ──迁徙──▶ 主世界
                              ▲                │
                              └──incident录制──┘
```

---

## 2. 进入主世界

### 2.1 路径 A · 主世界繁殖（主路径）

触发：

- 父母双方在主世界、adult 阶段、距离足够近、状态足够健康、物种兼容、繁殖冷却已过
- 当前季节 / 局部资源 / 危险等级满足繁殖窗口

流程：

- 发 `MatingInitiated`
- crossover + mutation 生成后代基因，记 parent_ids / generation / lineage_id
- 后代以 juvenile 阶段出现在父母附近
- 移动 / 战斗 / 繁殖能力按年龄系数衰减
- 发 `BirthRegistered`

### 2.2 路径 B · 边境迁徙（辅路径）

触发：

- lineage 在边境存活 ≥ `migration_min_lifetime`
- 通过基线保护 + 历史回归 + 无重大恶化信号三道硬门槛

流程：

- 1-3 个个体在主世界**地图边缘**位置出现（边缘海岸 / 远离基地的高地 / 洞穴口 / 未探索地形）
- 以成年形式出现，标签 `migrant`，无特权
- 发 `MigrationArrived`

### 2.3 禁止路径

- ❌ `reset()` / 批量替换 / 集体出现 / 集体消失 / "训练中请等待"提示
- ❌ 训练沙盒和边境世界的 lifecycle 事件不通过 EventBus 直接广播给主世界

---

## 3. 离开主世界

| 死法 | 触发 | 后续 |
|---|---|---|
| 老死 | elder 阶段年龄随机存活率衰减 | 留尸体，可食腐 / 疾病源 |
| 病死 | 长期疾病累积 / 治疗失败 | 留尸体（高传染） |
| 战死 | 被捕食 / 战斗失败 / 外伤 | 留尸体，可食腐 |
| 生理压力死 | hydration / hunger / core_temperature 长期失衡 | 留尸体 |

死亡的基因当场停止存在；只能通过留下后代延续。

---

## 4. 灭绝处理

| 场景 | 处理 |
|---|---|
| 4.1 迁徙者灭绝 | archive 永久保留；可重试（不同季节 / 地点 / 规模 / 时机）；连续 N=3-5 次失败 → 发 `MigrationFailurePersistent` incident |
| 4.2 主世界 lineage 灭绝 | 留尸体 / 谱系记录 / trace；可从 archive fork 重训；重训版带 `ancestral_archive_id`，必须走 frontier → migration |
| 4.3 物种灭绝 | 主世界继续运行；至少留一个 day-night + 季节窗口的生态反应时间，不立即填空；发 `SpeciesExtinction` incident |
| 4.4 主角物种全灭绝 | 主世界继续运行；不黑屏 / 不弹"训练中"；archive 旧 protagonist 通过 frontier 重新孵化 → 边缘重新引入 |

每次重试是独立 lineage event，不是"复活续命"。archive 基因不可直接复活。

---

## 5. 谱系流动闭环

```
[主世界 incident] ──auto录制──▶ [Incident]
                                    │
                                    ▼
                          [Challenge Scenario]
                                    │
                                    ▼
                            [训练沙盒训练]
                                    │
                                    ▼ (fitness + diversity + 回归)
                            [候选 Lineage]
                                    │
                                    ▼
                          [边境世界存活验证]
                                    │
                                    ▼ (通过迁徙门槛)
                          [主世界边缘出现]
                                    │
                                    ▼ 路径 A 复用
                            [新血脉扩散]
```

---

## 6. 工程约束

- **主世界 runtime**：长时间连续；支持热保存 / 热恢复（不视为 reset）
- **lineage_id 持久化**：parent_ids / generation / origin（main_birth | frontier_migration）写入世界 state，不只是 trace
- **共享 brain schema**：训练沙盒 + 边境 + 主世界同一份；schema 变更走 R-003 D 类版本门
- **incident detector**：自动捕获大规模死亡 / 瘟疫 / 生态位塌陷 / 合作链断裂；输出结构化数据，含死亡谱系子图 + 环境快照 + 事件序列 + lineage_id 列表
- **incident → challenge**：自动翻译至少 1-2 类
- **migration entry**：单次 ≤ 3 个体，地图边缘位置；触发 = `migration_min_lifetime` + 三道硬门槛
- **学习型调度器**：只做提议 / 排序 / 加权；不可关基线保护 / 回归通过 / canary 三道门

---

## 7. 不变量清单

1. 主世界永不 reset
2. 主世界活体只能通过出生或迁徙进入
3. 主世界活体只能通过老死 / 病死 / 战死 / 生理压力死离开
4. 训练沙盒事件不直接影响主世界
5. 边境世界故障不导致主世界 reset
6. 一次迁徙 ≤ 3 个体，必须在地图边缘出现
7. 迁徙者无特权
8. 训练成果占据生态位的唯一方式 = 被路径 A 复用
9. 死亡基因不会"训练完了再回来"
10. 学习型调度器不得关闭基线保护 / 回归通过 / canary
11. `lineage_archive` 永久保留所有 lineage（含灭绝）
12. archive 基因必须走 frontier → migration，不可直接复活
13. 物种灭绝后至少留一个 day-night + 季节窗口生态反应时间
14. 任何灭绝场景都不触发主世界 reset
15. 重新选育版本带 `ancestral_archive_id`，是新 lineage
16. 连续迁徙失败硬上限 N=3-5
17. 主角全灭绝期间主世界不黑屏 / 不弹"训练中请等待"

---

## 8. 与现有代码差距

| spec 项 | 当前 |
|---|---|
| 主世界永不 reset | 缺，需新 main_world runtime |
| 三层世界并存 | 仅训练沙盒 |
| 个体生命周期 | 缺 juvenile / adult / elder + 自然死亡 |
| 主世界内性繁殖 | NEAT crossover 仅 episode 边界，主世界无 |
| lineage_id 持久化 | 仅 trace，非世界 state |
| 共享 schema gate | 无 |
| incident 录制 | 无 |
| incident → challenge | 无 |
| 边境世界 | 无 |
| 迁徙入口 | 无 |
| archive 持久化 | 无 |

---

## 9. 验收标准

1. 主世界从启动到关停无 reset 事件
2. 用户连续观察几十分钟到几小时无断层
3. 出生 / 成长 / 衰老 / 死亡 / 繁殖五种事件可观察
4. 至少观察到一次外来血脉迁徙 → 几代后扩散
5. 用户投放灾难事件 → incident 自动录制
6. 至少跑通一次 incident → challenge → 训练 → 边境 → 迁徙完整闭环
7. 全过程主世界无集体消失 / 替换 / 重置

---

## 10. 后续工作 W1~W6

| 阶段 | 内容 |
|---|---|
| W1 | main_world runtime 骨架（长跑 + 热保存 / 热恢复，不加新机制） |
| W2 | 生命周期 + 繁殖 + lineage 持久化（对接 Stage C / R-006） |
| W3 | frontier_world runtime + migration 入口 |
| W4 | incident 录制 + 1-2 类 challenge 翻译模板 |
| W5 | 端到端最小闭环跑通 |
| W6 | 学习型调度器（可选，硬门槛不可关） |
