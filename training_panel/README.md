# training_panel — DP / PE 训练管理 Web 面板

DP(deep_protagonist)和 PE(protagonist_ecology)共用的本地训练控制台。
跑在训练机的 localhost 上,用网页统一管理两套训练。

## 启动
```powershell
cd routes\training_panel
.\start_panel.ps1            # 默认 http://127.0.0.1:8070
.\start_panel.ps1 -Port 9000
```
首次运行会建 `.venv` 并装 `fastapi` + `uvicorn`(只两个轻依赖)。
浏览器打开 `http://127.0.0.1:8070`。

## 功能
- **训练控制**:网页调参数/种子/env 开关,一键启动 / 停止 DP、PE 训练;实时命令预览。
- **实时输出**:WebSocket 滚动 stdout/stderr。
- **日志管理**:列出 `.jsonl`/`.log`,清理(默认移到 `_trash/` 可恢复,也可彻底删)。
  - 默认只清 `.log` 文本日志;训练指标 `.jsonl` 不会被自动清理;正在写入的日志受保护。
- **检查点管理**:列出 DP `runs/*.pt` + PE `data/runs/*/checkpoint_gen*`;清理选中。
  - 🔒 红线资产(`ft_gen.pt` / `bc_gen.pt` / `bc_hv2.pt`,以及名字含 `_red/_final/_champ/_keep` 的)**永远不可删**。
  - 删除默认是"移到回收站"(`training_panel/_trash/`),可恢复。
- **数据面板**:把 `scripts/dp_dashboard.py` 的指标移植到网页(Chart.js):
  Reward 曲线 + wander 基线、里程碑触发率、动作分布(红=沉默动作)、死因分布、存活率。支持 5s 实时刷新。
- **3D 回放**:iframe 嵌入 DP / PE 的 web3d viewer,显示当前 trace 信息,可在新标签打开。
- **系统状态**:GPU(nvidia-smi)、磁盘空闲、训练进程状态。

## 路径约定(自动从本文件位置推导)
```
<repo>/routes/training_panel/   <- 本面板
<repo>/routes/deep_protagonist/  exe: build_d122_v2\bin\deep_protagonist.exe  runs/*.pt *.jsonl
<repo>/routes/protagonist_ecology/  exe: build\bin\neural-eco-protagonist.exe  *.toml  data/runs/
```
若 exe 路径与默认不符,改 `server.py` 顶部的 `DP_EXE_CANDIDATES` / `PE_EXE_CANDIDATES`。

## 安全边界
- 只 spawn 已有的训练 exe + 读/移动两套项目树下的文件;**不碰训练核心逻辑、不改游戏机制**。
- 路径做了 traversal 防护;删除默认进回收站;红线检查点硬锁。
