# M0 · 脚手架与抽象

> 这是整个项目的**地基**。M0 设计错了，后续所有阶段都要付代价。

---

## 目标

把"框架级"东西一次性建好，让 M1 之后所有阶段只关注业务逻辑：
1. 编译/构建系统能跑
2. 配置加载能用
3. 日志能输出
4. 事件总线能发布订阅
5. 所有核心抽象接口（IBrain / IWorld / IPerception / IAgentCapability / IRewardComponent / IWorldLayer / IObserver / IEventBus 等）头文件就位
6. headless 和 interactive 双运行时骨架就位

**M0 不写任何业务逻辑**——没有进化、没有世界规则、没有渲染。只有脚手架。

---

## 前置条件

- 安装 Visual Studio 2022（含 C++ Desktop Development workload）或 MinGW-w64 GCC 13+
- 安装 CMake 3.25+
- 安装 vcpkg（用于第三方依赖）
- Git

---

## 关键产出

### 构建系统
```
CMakeLists.txt              顶层
src/CMakeLists.txt          总聚合
src/core/CMakeLists.txt     core 静态库
src/viz/CMakeLists.txt      viz 静态库（M0 阶段空骨架，M2 填实）
src/runtime/CMakeLists.txt  runtime 可执行
tests/CMakeLists.txt        测试
vcpkg.json                  依赖清单
```

依赖（M0 阶段最小集）：
- `tomlplusplus` —— TOML 配置加载
- `spdlog` —— 日志
- `gtest` —— 单元测试
- `raylib` —— 占位（M2 才用，但 M0 就纳入构建避免 M2 才发现集成问题）

### 核心接口（src/core/interfaces/）

```
IBrain.h               大脑前向推理
IGenome.h              可序列化、可变异、可交叉的基因
IWorld.h               世界顶层
IWorldLayer.h          世界的一层（地形/食物/信号...）
IAgent.h               个体
IPerception.h          感知组件
IAgentCapability.h     能力组件
IRewardComponent.h     奖励组件
IEvolutionStrategy.h   进化策略
IObserver.h            观测者（订阅事件）
IEventBus.h            事件总线
IRenderer.h            渲染器（M2 用，先占位）
```

### 事件总线（src/core/events/）

```cpp
// 简化签名
struct Event { EventType type; uint32_t agent_id; std::any payload; };

class EventBus : public IEventBus {
public:
    using HandlerId = uint64_t;
    HandlerId subscribe(EventType type, std::function<void(const Event&)> handler);
    void unsubscribe(HandlerId id);
    void publish(const Event& event);
    void flush();   // 一次 tick 结束统一派发
};
```

事件类型枚举（先把后续阶段需要的全部声明，M0 不需要全部触发）：
```
AgentBorn / AgentDied / AgentMoved / AgentAteFood
AgentEmittedSignal / AgentReceivedSignal
AgentAttacked / AgentDamaged
AgentPickedUp / AgentDropped
GenerationEnded / SpeciesFormed / SpeciesExtinct
WorldTick / WorldReset
InterventionTriggered  (F-GOD)
```

### 配置系统（src/core/config/）

加载 `configs/default.toml`，热更新支持（运行时修改 toml 后通知订阅者）。
所有模块取参数都通过 `Config::get<T>("path.to.key")`。

### 日志系统（src/core/logging/）

包装 spdlog，提供：
- 控制台输出（带颜色）
- 文件输出（每次运行一份 `data/runs/<timestamp>/log.txt`）
- 级别过滤可配置

### 双运行时骨架

```
src/runtime/headless/HeadlessRuntime.cpp
src/runtime/interactive/InteractiveRuntime.cpp
src/main.cpp                    根据 CLI 参数选择运行时
```

CLI:
```
neural-eco --mode headless --config configs/default.toml
neural-eco --mode interactive --config configs/default.toml
```

---

## 三档完美标准

### MVP（最小可运行）✅⛳⛳

- [ ] CMake 配置生成成功
- [ ] 全项目编译通过
- [ ] `neural-eco --hello` 输出 "Neural Eco v0.1 - Hello"
- [ ] 单元测试框架接入，至少 1 个空的 dummy 测试通过

### 完整（feature complete）✅✅⛳

- [ ] 所有 11 个核心接口头文件就位（纯虚接口，无实现）
- [ ] 事件总线能 subscribe / publish / unsubscribe，带单元测试
- [ ] 配置系统能加载 TOML，支持嵌套 key 读取，带单元测试
- [ ] 日志能控制台 + 文件双输出
- [ ] HeadlessRuntime 和 InteractiveRuntime 框架就位（不做实事，但能 init / tick / shutdown）
- [ ] CLI 参数解析能用

### 完美（polished）✅✅✅

- [ ] 接口设计经过 review：能容纳 NEAT（M3）和 2.5D 地形（M5）—— 先把这两个未来需求"模拟代入"看接口能否承载
- [ ] 配置文件结构成型，不是临时凑合的（参考 `configs/default.toml` 完整模板）
- [ ] 日志格式统一、信息量合理
- [ ] CMake 输出干净（无警告）
- [ ] README 顶层和 `src/core/README.md` 的"模块速览"一致
- [ ] 单元测试覆盖所有 M0 产物
- [ ] git tag `baseline-M0-done`

---

## 🧠 你的补充点

| 项 | 你需要决定 |
|---|---|
| 配置项命名风格 | `snake_case.dot.path`（默认）/ 其他 |
| 日志默认级别 | DEBUG / INFO / WARN（默认 INFO） |
| 事件命名风格 | enum class 还是字符串？（默认 enum class，类型安全） |
| 是否启用编译期事件检查 | 是（默认）/ 否 |

> 这些不是"必须你决定"——你不答我就用默认。但每个决定都会影响后续开发体验。

---

## 🔌 接入点（M1 之后会怎么用）

- M1 创建第一个 `World` 实现 → 实例化 `EventBus` → 注册第一个 `Observer`
- M2 viz 模块**只**通过 `IObserver` 订阅事件来获取数据，绝不直接读 `World`
- M3 实现 `NeatGenome` + `NeatBrain` → 替换 M1 的 `FixedTopologyBrain`，**测试 IBrain 接口设计**
- M5 实现 `TerrainLayer` + `TerrainPerception` → **测试 IWorldLayer 和 IPerception 接口设计**

---

## ⚠️ 易踩坑

1. **接口太具体**：忍住"这里加个具体方法吧"的诱惑。M0 接口必须只定义"是什么"，不定义"怎么做"
2. **过早优化**：事件总线先用 `std::function` + `unordered_map`，M5 之后如果 profile 有热点再换 lock-free queue
3. **vcpkg 集成**：第一次接 vcpkg 容易卡半天。如果两小时没搞定，**直接换 FetchContent**（CMake 自带）

---

## 验收清单一览

```
□ MVP 三项全过 → 写 git tag baseline-M0-MVP
□ 完整 七项全过 → 写 git tag baseline-M0-feature-complete
□ 完美 七项全过 → 写 git tag baseline-M0-done → 进入 M1
```

---

## 下一阶段

→ [M1 · 连续 2D MVP](M1_mvp.md)
