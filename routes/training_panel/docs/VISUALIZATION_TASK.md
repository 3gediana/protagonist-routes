> ★★ 更新 2026-06-07（训练已全停）★★
> 之前的红线"8GB 显存跟训练共享、别抢 GPU"**现在解除了** —— DP/PE 训练已全部停掉，GPU 空闲（~1.8G/18%）。可视化导出/渲染可以自由用 GPU，不会撞挂训练。
> 同时检查点已是**最终稳定态**，重导帧请用这些：DP=`routes\deep_protagonist\runs\ft_gen.pt`(unlocks 3.33/10)；PE=`routes\protagonist_ecology\data\runs\onepot_sD_warm_50025`(密集 me_cov 10/25) 或 50021-24(gen349-379)。
> 下面是原任务文档（现状 + P1-P5 优先级 + 照抄启动命令），仍然有效。


# 可视化任务交接（DP 深度主角 + PE 生态）

> 更新：本轮训练会话（与 `HANDOFF_2026-06-01.md` 顶部"★最新★"块、`_START_HERE.md` 同批）。
> 目的：让一个**并行的新对话**专门做可视化，不跟主训练会话冲突。下面全部是 SSH 上盘**实测**出来的现状，不是凭空写。

---

## 0. 一句话

**两个项目都已经有可视化基础设施（不是从零）**，但有三个硬问题要解决：
1. **数据全过期**——现有 viewer 放的都是 D-128 之前的老世界（没有随机出生、没有两阶段狩猎、没有广度里程碑、没有保暖/穿衣、PE 没有当前种子）。
2. **新机制没被画出来**——这轮做的"涌现谱"故事（哪些机制真触发、解了几个里程碑、为什么冻死）目前**只有数字、没有画面**。
3. **散、没有一键启动**——DP/PE 各有 2~3 套 viewer，入口零散。

任务 = **刷新数据 + 把"涌现"讲清楚（可视化）+ 收敛成一键启动**。

---

## 1. 现状盘点（每个文件都实测存在）

### DP（`D:\claude-code\c++\routes\deep_protagonist\`）
| 组件 | 路径 | 状态 |
|---|---|---|
| **原生实时 viewer**（raylib） | `build_d122_v2\bin\deep_protagonist.exe`（1.4MB） | **可用**。加载策略实时渲染主角行动，`src\main.cpp` + `src\render\WorldRenderer.cpp`(27KB) |
| **web3d 回放**（three.js） | `viewer\web3d\`：`index.html` + `src\main.js`(65KB) + `gallery.html` | 代码可用，但 **data 过期**：`data\frames.json`(2.4MB, 6/3) + `data\world.json`(714KB, 6/3) |
| **指标面板**（matplotlib） | `scripts\dp_dashboard.py`(6KB) | **可用、独立**。读 `runs\*.jsonl` → reward/score/unlocks/milestone/动作直方/死因；另有 `scripts\plot_training.py` |
| 修复记录 | `viewer\web3d\viewer_fix_report.md` | 记了地形渲染修复（cave hills / knoll vs dome / cell 缩放对齐 PE） |

### PE（`D:\claude-code\c++\routes\protagonist_ecology\`）
| 组件 | 路径 | 状态 |
|---|---|---|
| **web3d**（three.js） | `viewer\web3d\`：`index.html` + `src\main.js`(56KB) | 代码可用，**data 过期**：`data\frames.json`(7.8MB, 6/2) + `data\viewer_world.json`(147KB) |
| **python_viewer**（pygame MVP） | `viewer\python_viewer\view_frames.py`(6.8KB) + `README.md` | **可用**。读 `viewer_frames.jsonl`；全套图例（9 种 archetype 配色 / 4 种建筑 / 6 种天气）；操作：Space 暂停、←→ ±10 帧、R 重置、+/- 调速 |
| **Godot demo**（R3 目标） | `viewer\godot_demo\`：`ecosystem_controller.gd`(9.9KB)、`wolf_controller.gd`(5.1KB)、`.tscn` 场景 + Kenney/FBX 资产（res_nature/res_survival） | **仅 demo 级、未接真实帧** |
| Godot 规格 | `docx\supporting\LAYER7_GODOT_VIEWER_SPEC.md`(5.5KB) | Godot4 + Kenney 模型 + 蒙古包视觉 + 高光 的完整 vision |
| 指标说明 | `docx\supporting\METRICS_FIELD_GUIDE.html`(39KB) | 字段含义 |

---

## 2. 数据管线（帧怎么产出 —— 关键，决定能不能刷新）

- **DP 原生 viewer**：**不需要导出文件**，直接实时跑策略渲染。复现训练世界需开 env：`DP_SPAWN_RANDOM=1`、`DP_HUNT_V2=1`、`DP_PREY_PRIORITY=1`。
- **DP web3d**：吃 `frames.json`/`world.json`。旧文件是 6/3 产的——**导出口要先找/补**（grep `frames.json` 在 src 里只命中原生 viewer，web3d 的帧很可能是某个一次性脚本/手工导的）。这是 P1 要补的工具。
- **PE**：训练 toml 里把 `viewer_frames_enabled=true` 打开 → 每个 episode 落 `viewer_frames.jsonl`，路径形如 `data/runs/layer1/<时间戳>/trace/generation_X/episode_Y/viewer_frames.jsonl`。`python_viewer` 直接喂这个 jsonl。web3d 的 `frames.json` 需要再做一层 jsonl→json 转换。

---

## 3. 要加强的地方（按优先级，供并行 agent 认领）

**P1 · 数据刷新（最重要，先做这个）**
现有 viewer 全在放 D-128 之前的老世界。用**当前**资产重导一局：
- DP：用 `runs\ft_bc10.pt`（会打猎的底座）和**即将出炉的** `runs\bc_gen.pt` / `runs\ft_gen.pt`（全能底座微调，主训练会话正在做），在 `DP_SPAWN_RANDOM=1 DP_HUNT_V2=1` 世界跑一局导出帧给 web3d，或直接用原生 viewer 录屏。
- PE：用当前活跃种子 `50021-24`（gen300+，主会话在跑）跑一局开 `viewer_frames_enabled=true`，喂 python_viewer / 转 web3d。

**P2 · 把"涌现谱"可视化**（这轮训练的核心结论现在只有数字）
- DP：给 `dp_dashboard.py` 加一个"机制涌现面板"——每局 `unlocks=N/10`、10 个里程碑（drink/eat/collect/spear/shelter/clothing/seed/house/crop/follower）各自触发率、23 个动作哪些"活"哪些被压成 0（死动作）、死因分布（冻死/饿死）。
- PE：`me_coverage` 25 格（5×5×1：chop×build+craft×hunt）热力图 + 各 archetype 占比 + 各机制成功率（投掷 vs 砍树 vs 近身 vs 建造）。

**P3 · 把新机制画出来**
- DP：两阶段狩猎（猎物受击后 0.35s burst 逃窜 ~2m）、保暖/穿衣（体温条 + 穿衣事件高亮）、广度里程碑解锁时的视觉脉冲。
- PE：建造链（worksite 进度条——python_viewer 已有，web3d 补上）。

**P4 · Godot R3 viewer**：按 `LAYER7_GODOT_VIEWER_SPEC.md` 把 `godot_demo` 接上真实 PE 帧（蒙古包视觉），从 demo 升级成真 viewer。

**P5 · 一键启动**：收敛成 `view_dp.ps1` / `view_pe.ps1`（启原生 viewer / 起本地 http server + 开浏览器），存进 `routes\...\handoff\scripts\`。

---

## 4. 怎么跑现有 viewer（给并行 agent 直接照抄）

```powershell
# DP 原生实时 viewer（不需导出文件；开 env 复现训练世界）
cd D:\claude-code\c++\routes\deep_protagonist
cmd /c "set DP_SPAWN_RANDOM=1&& set DP_HUNT_V2=1&& set DP_PREY_PRIORITY=1&& build_d122_v2\bin\deep_protagonist.exe --policy ppo --load runs\ft_bc10.pt"

# DP web3d 回放（file:// 会被 CORS 挡，用本地 http server）
cd D:\claude-code\c++\routes\deep_protagonist\viewer\web3d
python -m http.server 8080   # 浏览器开 http://localhost:8080/index.html

# DP 指标面板
cd D:\claude-code\c++\routes\deep_protagonist
python scripts\dp_dashboard.py runs\ft_wa.jsonl runs\ft_gen.jsonl --labels wa,gen --out-prefix dp

# PE python viewer（uv 会自动装 pygame）
uv run D:\claude-code\c++\routes\protagonist_ecology\viewer\python_viewer\view_frames.py <path\to\viewer_frames.jsonl>

# PE web3d 回放
cd D:\claude-code\c++\routes\protagonist_ecology\viewer\web3d
python -m http.server 8081   # 浏览器开 http://localhost:8081/index.html
```

---

## 5. 红线 / 坑（沿用主交接 `PITFALLS.md`，可视化特有的额外几条）

- **别删任何检查点 / demos**——viewer 重导帧只**读**检查点，绝不写。
- **8GB 显存是和主训练会话共享的**：主会话正在跑 PE 4 种子 + 即将起 DP 全能底座微调。并行做可视化时，若要跑导出 / 原生 viewer，**优先用已存帧或离线（CPU）导出**，别在训练显存吃紧时抢 GPU，会 OOM 撞挂训练。要跑重的 GPU 导出前，先 `nvidia-smi` 看余量、或跟主会话错峰。
- **frames.json 很大**（PE 7.8MB）：别把大量帧 commit 进 git；导出覆盖 `viewer\web3d\data\` 即可。
- **编码**：仓库里中文文档多为 GBK/混合，SSH `cat` 会乱码——用编辑器按 GBK 打开，或 `Get-Content -Encoding Default`。
- **viewer 的世界几何要和训练一致**：`viewer_fix_report.md` 记过 DP/PE cell 缩放、地形（knoll vs dome、cave hills）对齐的坑，改渲染时回看。

---

## 6. 连接

隧道 + SSH 见 `03_ssh_remote_access_v2.md`（隧道地址固定、付费版；私钥可从 secret 重建，不绑某台 VM）。机器入口：`_START_HERE.md`（盘上 `D:\claude-code\c++\routes\_START_HERE.md`）。
