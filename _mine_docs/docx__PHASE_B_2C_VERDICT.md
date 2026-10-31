# Phase B #2c 验收报告（80 代长跑 @ dmg=0.25 / cold_thr=30, env-perc ON）

> run: `data/runs/realism_pressure_long/20260530-172038`（seed 50001, 80 代）
> 日期：2026-05-30。结论：**harmony 7/7 PASS；冷压力下进化提升了冬季存活；"主动避寒"信号真实但偏弱（部分成立）**。

## 1. harmony 7/7 ALL PASS（冷压力没破坏生态）
| 检查 | 结果 | 细节 |
|---|---|---|
| C1 hunters active | PASS | ambush>=3, pack>=7, apex>=2 |
| C2 herbivores attrition | PASS | rabbit 21-24 / large_herb 17-46 / small_forager 13-17 |
| C3 coop co-presence | PASS | peak=1920.7s |
| C4 scavengers fed | PASS | omnivore pop13-27 fit3259 / scavenger pop6 fit449 |
| C5 worksites completed | PASS | peak=2/gen total=145 |
| C6 me_coverage>=0.30 | PASS | **peak=0.440** qd=73485（从 16 代短跑的 0.28 回升，证明 C6 之前差只是短跑未收敛）|
| C7 no extinction | PASS | 所有 archetype 存活 |

→ 加冷压力（dmg=0.25/cold30）满 80 代，围猎/食物网/协作/建造/物种分工**全部保留**。

## 2. 冷压力确实在咬人（差异化压力真实）
- gen0：83% 的 tick 处于受冻状态（core_temp<=30），受冻时 health 0.585 vs 不冻 0.722。
- gen79：74% 受冻，受冻 health 0.660 vs 不冻 0.862。
- → 冷是真实代价（受冻 health 明显低于不冻），且没杀光（C7 PASS）。

## 3. gen0 → gen79 进化对比（brained 主角，受冻 vs 不冻分桶）
| 指标 | gen0 受冻 | gen0 不冻 | gen79 受冻 | gen79 不冻 |
|---|---|---|---|---|
| spatial_fire_cells | 0.000 | 0.000 | **1.576** | 1.461 |
| health_ratio | 0.585 | 0.722 | **0.660** | **0.862** |
| speed | 1.320 | 1.771 | 1.402 | 1.808 |
| 受冻时间占比 | 83% | — | 74% | — |

**诚实解读：**
- ✅ **存活提升**：gen79 受冻 health 0.585→0.660、不冻 0.722→0.862 → 进化让 agent 更扛冻、整体更健康。
- ✅ **火源接近度从无到有**：gen0 火源感知恒为 0（根本不到火边）→ gen79 ~1.5 → 进化把主角带到了篝火附近。
- ⚠️ **"主动避寒"信号偏弱**：受冻时火源比不冻只多 +0.115（方向对但幅度小）；worksite_distance 受冻反而更远（+22.18，方向相反）。→ 不能强断言"一冷就冲去烤火"。
- 综合：**进化在冷压力下提升了冬季存活 + 提升了火源接近度**，但"受冷→反应式找火"这条因果链信号弱、混合。

## 4. 待办（下一会话，用户重启电脑后继续）
- **ON/OFF 环境感知对照**（真实压力下）：再跑一条 env-perc OFF 的 80 代（同 seed 50001、同 dmg=0.25），对比 ON vs OFF 的冬季存活/火源接近度。若 ON > OFF → 环境感知在真实压力下终于被选择性用上（因果成立）；若无差异 → 压力仍不足或感知仍没被用上（考虑降 core_temp 钳位下限 / 加大 dmg / 加冬季食物枯竭）。
- 本轮因用户要重启电脑，**未起对照跑**（遵守"跑完这一轮停下来"）。
