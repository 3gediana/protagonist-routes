# protagonist_ecology (PE) · PITFALLS

> 先看 DP 的 `../deep_protagonist/handoff/PITFALLS.md`（连接/PowerShell/反-Goodhart 通用）。这里只列 PE 特有的。

1. **共用热启动基座别删**：上轮误删了 `onepot_sC_long_50002/.../checkpoint_gen79`，它是多个 warm 种子的共用基座，删了导致新种子全崩。现在 warm 基座是 `data/runs/onepot_sD_warm_50002/20260604-115936/checkpoint_gen159`——**清盘前确认某个 checkpoint 不是别的种子的 resume 依赖**。
2. **二进制需要 CUDA 在 PATH**：启动前 `set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%`，否则进程起不来或秒退。
3. **进程名是 `neural-eco-protagonist-batched`**（不是 deep_protagonist_train）。监控/停训要按这个名字找。
4. **TOML 的 `generations` 有上限**：warm 续跑要把 `generations = 160` 改成更大（如 320），否则一启动就判定"已达代数上限"直接退出。`clone_pe.ps1` 已处理。
5. **"slice overflow" 是常见告警**：monitor 里统计它，少量正常；暴涨说明并发/显存出问题。
6. **CSV 路径分种子**：行为指标写在各自 `data\runs\onepot_sD_warm_<seed>\<时间戳>\` 下，不是固定路径。读数据先定位最新时间戳目录。
7. **指标同样会"空按"**：chop/craft/hunt 的"attempts"是尝试次数不是成功；看 `*_successes`、成功率算 `successes/attempts`。
