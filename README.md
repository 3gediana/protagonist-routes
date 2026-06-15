# _START_HERE — 接手这两个项目的总入口（2026-06-06）

> 写给接手的 AI 模型。这里有**两个平行项目**，都在研究"让自然生存世界里尽量多的机制/行为真正涌现"。
> **先读这篇（2 分钟），再按指引进对应项目的 `handoff\START_HERE.md`。**

## 两个项目
| 目录 | 是什么 | 入口 |
|---|---|---|
| `deep_protagonist`（DP，**主**） | 单智能体 PPO，在 3D 自然世界自学求生（找水/觅食/造矛/狩猎/造衣/造屋/用火/采矿…） | `deep_protagonist\handoff\START_HERE.md` |
| `protagonist_ecology`（PE，平行） | 群体协同进化（NEAT），看生态位分化 + 协作/通讯能否涌现 | `protagonist_ecology\handoff\START_HERE.md` |

## 一句话现状
两个系统都**坍缩到约 1/3 的机制宽度，撞的是同一面墙**：绝大多数机制要么机制上够不着（成功率≈0），
要么够得着但没收益压力非做不可 → 各自坍缩到一条最省力活路（DP=eat+collect+spear；PE=投掷型杂食通才）。
根因是"**标量爬坡 → 单策略坍缩**"。已完成 **P0 涌现谱 baseline**（纯埋点，把谱有多窄钉死）；P1（怎么改）方向待用户拍。

## 你要做的第一件事
1. 读 `deep_protagonist\handoff\CONNECT.md` → 连上用户的 Windows 机器（chisel 隧道会断，按文档重连）。
2. 读 `deep_protagonist\handoff\START_HERE.md` + `PITFALLS.md`（**坑很多，省你几天**）。
3. 跑 `deep_protagonist\handoff\scripts\monitor.ps1` 看现状。

## 用户是谁、要什么（很重要）
- 大一学生，**零 RL/DL 基础、不读代码、看效果不看术语**。第一次出现的术语必须当场一句大白话解释。
- 要的是"**尽量全面抓多种机制涌现**，别盯着一两个指标"。
- 红线：**反-Goodhart**（别"按一下就给分"刷指标，用 PBRS 势函数）；**发现根因就停下来从根上改**，别堆奖励尝试；别阉割功能/填零假装做了/重置网络。
- 文档铁律（见各项目 `docs\`）：术语→大白话、决策必记、速查文档不能用术语解释术语。

## 注意：哪些文档是新的、哪些过时
- **新（权威，照这个做）**：各项目 `handoff\` 下的所有文件（本次交接整理，UTF-8 干净）。
- **旧（只查史）**：`docs\HANDOFF.md`（停在 D-064）、`docs\PROGRESS.md`（停在 D-122，有 GBK 乱码）。别照着这两个做，状态已过时。
- 本轮真正的进展（D-128→D-135：两阶段狩猎、随机出生、BC→PPO、P0 谱）**只在 `handoff\` 里**，旧 docs 没有。

---

## Export Note

Sanitized public export of three C++ / Python AI projects:

- **deep_protagonist** — Single-agent 3D survival with PPO + libtorch
- **protagonist_ecology** — Multi-agent ecology with NEAT + MAP-Elites
- **training_panel** — Web panel for LLM-agent-driven automated training iteration

Large binary assets (textures, Godot scenes, training checkpoints, viewer frame data) excluded.
