# Autonomous Dev Status · 通宵自主开发状态

> **这份文件是上下文压缩后的恢复锚点。** 每完成一个阶段/模块后更新。

## 用户指令摘要

- 通宵自主推进，不需要用户参与
- 跳过 M2 可视化，等机制全部稳定后再统一做
- 每个阶段按流程：读文档 → 查资料 → 实现 → 验证 → 更新文档
- 遇到需要用户商榷的设计决策才停
- 不要反复问用户中性问题，默认自己做决策

## 开发路线

1. M4 多线程加速
2. M5 真实 2.5D 地形
3. 跳过 M2 可视化
4. 模块 Stage A→B→C→D（见 docs/modules/README.md）

## 当前进度

| 阶段 | 状态 | 备注 |
|---|---|---|
| M0 脚手架 | ✅ | tag: baseline-M0-done |
| M1 连续2D MVP | ✅ | tag: baseline-M1-done |
| M2 可视化 | ⏸ | 延后至机制稳定 |
| M3 NEAT升级 | ✅✅⛳ | tag: baseline-M3-MVP; 待补 baseline-M3-done |
| M4 多线程加速 | ⬜ | 当前目标 |
| M5 真实地形 | ⬜ | |
| F-PRED 捕食 | ⬜ | Stage A |
| F-FIGHT 战斗 | ⬜ | Stage A |
| F-COMM 通信 | ⬜ | Stage A |
| F-NEUROMOD 神经调节 | ⬜ | Stage A |
| F-DAY 昼夜 | ⬜ | Stage B |
| F-SEASON 季节 | ⬜ | Stage B |
| F-RESOURCE 动态资源 | ⬜ | Stage B |
| F-DISEASE 疾病 | ⬜ | Stage B |
| F-COEVO 共进化 | ⬜ | Stage C |
| F-NOVELTY 新奇搜索 | ⬜ | Stage C |
| F-NSGA 多目标 | ⬜ | Stage C |
| F-CURRICULUM 课程 | ⬜ | Stage C |
| F-LEARN 终身学习 | ⬜ | Stage C |
| F-REPLAY 回放 | ⬜ | Stage D |
| F-HEAT 热力图 | ⬜ | Stage D |
| F-STAT 高级统计 | ⬜ | Stage D |
| F-TREE 系统树 | ⬜ | Stage D |
| F-GOD 上帝面板 | ⬜ | Stage D |
| F-FORK 平行宇宙 | ⬜ | Stage D |
| F-COMPARE 对照 | ⬜ | Stage D |
| F-LLM 解说 | ⬜ | Stage D |

## 仓库信息

- 路径: `D:\claude-code\c++`
- 构建: `cmake --build build --config Release`
- 测试: `ctest --test-dir build -C Release --output-on-failure`
- Git: 已初始化，首次提交完成

## 停止条件

- 需要用户审美/方向决策
- 无法自行解决的技术死锁
- 所有已列出功能全部实现
