# PE d_gate smoke 结果（2026-10-01 12:28 / 12:51，batched.exe 10-01 构建）

两个种子（50031/50032，threshold 22→14 + share 0.5 + signal_response_require_approach gate）：
- 50031: 30 gens, cov 0.067→0.065, resp=0.00, emits=48.0, surv 2.7/48, avgFit 1542
- 50032: 15 gens 时提前读表（后台队列未跑完时查的），cov 0.061→0.088, resp=0.00, emits=48.0

## 核心诊断（读代码确认）
resp（即时响应事件）与 approach gate 无关——gate 只作用于 dwell-share 结算支付；
resp=0 说明 listener 从未在 emit 存活期内进入 halo。结构原因：600×600 世界（Layer-1
硬约束）×48 主体，密度太稀，信号 halo 几乎不相交。emits=48=主体数（饱和发射，每主体
恰好 1/gen）。

## 下一轮设计方向（未实施）
1. halo 半径 / emit 数量重新标定到 600×600 尺度（数据驱动：按平均邻居距离反推半径）；
2. emit 语义改造：找到食物才 emit（信息论上诚实），listener 靠近后受益 → 可测的因果；
3. 或 emit 附带短促趋性引导（curriculum 式），先让"听→靠近→受益"链条存在再谈语义。

注意：avgFit 1542 @ surv 2.7/48 —— fitness 依旧被非生存项主导，该列继续不作总结指标。
