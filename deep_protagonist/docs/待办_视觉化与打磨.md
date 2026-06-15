# 待办：视觉化 & 答辩打磨清单

> **决策 D-038（2026-05-24）**：用户拍板"先训练，后装修"。
> 全部视觉化条目延后到 **PPO 训练真正训好、obs/action/SpatialMemory 接口冻结后** 再做。
> 现阶段任何会话**不要主动加这些 HUD**——会反复改、白干活。
>
> 触发条件（满足任一即可启动这份清单）：
> - mean reward 稳定 > +50（🟧 学到点）
> - 用户明确说"开始做答辩"
> - 蓝图阶段进入 S6（视觉美化）

---

## A. PPO 内部状态可视化（debug overlay，已许诺）

| 项 | 内容 | 数据源 | 优先 |
|---|---|---|---|
| A1 | **60m 视野圈**：agent 周围画青色 3D 圆环 | `ObservationBuilder::VISION_RANGE` | 高 |
| A2 | **当前 obs 4 棵植物高亮**：从 agent 划 4 条线到最近 4 棵熟果 | `obs[39..66]` 重建坐标 | 高 |
| A3 | **当前 obs 4 只动物高亮**：兔/鹿/狼/鱼用不同颜色线 | `obs[67..98]` | 高 |
| A4 | **当前 obs 3 个资源高亮** | `obs[99..116]` | 中 |
| A5 | **nearest_river 箭头**：从 agent 指向最近水源 | `obs[145..147]` | 高 |
| A6 | **nearest_blueprint 箭头 + 剩余材料文字** | `obs[158..160] + buildings.sites()` | 中 |

## B. SpatialMemory 认知地图（关键答辩素材）

| 项 | 内容 | 数据源 | 优先 |
|---|---|---|---|
| B1 | **mini-map**：屏幕右下 200×200 px，64×64 网格 | `SpatialMemory::raw()` 4096×6 floats | **最高** |
| B2 | **颜色映射**：visited→灰、water→蓝、food→绿、resource→棕、danger→红、building→金 | feature channel | **最高** |
| B3 | **agent 当前位置 cursor**：mini-map 上一个白点跟着移 | `agent.state().pos_x/y` | 高 |
| B4 | **8×8 当前窗口框**：mini-map 上用方框标出"网络这一帧实际读了哪 8×8 格" | agent 位置 + WINDOW | 高 |
| B5 | **回放模式**：录每秒一帧 mini-map 截图，episode 结束导出 GIF | post-episode | 中（炫技） |

## C. GRU 内部状态可视化（让人看到"网络在想什么"）

| 项 | 内容 | 数据源 | 优先 |
|---|---|---|---|
| C1 | **256 维 hidden state 条形图**：屏幕左下 8×32 个像素条 | `PPO::current_hidden()` | 中 |
| C2 | **action 概率分布**：屏幕底部显示 16 个离散 action 的 sigmoid(logit) 实时 | `actor head` 输出 | 高 |
| C3 | **value 实时折线**：滚动显示最近 10 秒的 V(s) 估值，与 reward 对比 | `value head` | 中 |
| C4 | **entropy 实时数字**：当前 policy 的不确定度 | derived from logits | 低 |

## D. 训练监控可视化（不在 viewer 里，在脚本里）

| 项 | 内容 | 数据源 | 优先 |
|---|---|---|---|
| D1 | **Python matplotlib 训练曲线**：reward / 各分量 / entropy / loss 多面板 | `runs/*.jsonl` | 高 |
| D2 | **多 run 对比图**：把 run2/run3/runN 画在一张图上 | 多个 jsonl | 高 |
| D3 | **死亡热力图**：把 100 局死亡位置叠加到地形俯视图 | 训练时 dump position | 中（炫技） |
| D4 | **行为统计仪表盘**：按行为分别记录得分（蓝图 line 108 要求）| jsonl | **必做** |

## E. 答辩 demo 模式（一键启动看完整故事）

| 项 | 内容 | 优先 |
|---|---|---|
| E1 | `--demo` flag：开启加速、HUD 全开、镜头跟随、定时切换视角 | 中 |
| E2 | **第一人称镜头**：跟在 agent 屁股后面 | 中 |
| E3 | **预设故事**：spawn → 找水 → 采集 → 建造 → 农耕 → 驯服一条龙 | 高（答辩压轴） |
| E4 | **多语言切换**：HUD 中英文（默认中文）| 低 |

## F. 录制 / 导出

| 项 | 内容 | 优先 |
|---|---|---|
| F1 | **raylib 截屏导出 mp4**：用 FFmpeg pipe | 中 |
| F2 | **gif 导出 mini-map 演化** | 低 |
| F3 | **jsonl 可读化**：把 episode 摘要导成 markdown 表 | 低 |

---

## 现在能做的（不破坏接口）

虽然主要可视化延后，但**有几件事不影响训练接口**，可以提前做：

- **D1 D2 D4**（Python 脚本看 jsonl）—— 完全外挂，不影响 C++ 代码
- **训练曲线绘图** —— 答辩素材的一半

这几件等你说"开始动手"再做。

---

## 接口冻结清单（这些定下来才能开装修）

可视化大规模启动前，下列接口必须**至少 2 周不再变动**：

- `ObservationBuilder::SIZE` 和字段顺序（hpp 注释里的 0..N 布局）
- `AgentAction` 的 19 个通道（3 cont + 16 disc）
- `SpatialMemory::FEATURES`、`GRID`、`CELL_SIZE_M`、`WINDOW`
- `ActorCriticImpl::HIDDEN`
- `RewardSystem::EpisodeStats` 字段名（mini-map 颜色映射依赖）
- `runs/*.jsonl` 字段名

任何修改这些接口的 commit 都要 grep 过这个文件并更新可视化对应代码。
