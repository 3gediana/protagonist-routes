# tests

单元测试与集成测试。Google Test。

## 组织

```
tests/
├── core/        与 src/core 镜像的目录结构
├── viz/         viz 测试（M2 之后）
├── integration/ 跨模块集成测试
└── fixtures/    测试用配置 / 数据
```

## 约定

- 测试文件名 `test_<被测模块>.cpp`
- 用 `gtest_main` 自动 main
- 通过 `ctest` 一键跑全部
- CI 必须全绿才允许进入下一阶段（参见 ADR-0004 完美标准）

## 优先级

每个 baseline 阶段结束时必测：

- M0：EventBus / Config / 接口编译
- M1：FixedTopologyBrain forward / GA crossover & mutation / RewardComponent
- M3：NEAT 拓扑变异 / 物种分化 / CTRNN 时序稳定性
- M5：TerrainLayer 查询 / 视野遮挡

模块测试在对应模块卡里说明。
