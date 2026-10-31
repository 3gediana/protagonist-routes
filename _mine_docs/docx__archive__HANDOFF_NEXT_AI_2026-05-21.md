# Protagonist Ecology · 给下一位 AI 的短交接

> 状态：**单次会话历史参考**。
>
> 当前默认入口已切换到：
> - `DEVELOPMENT_EXECUTION_GUIDE.md`
> - `DEVELOPMENT_TASK_PLAN.md`
> - `STATUS.md`
>
> 本文件保留是为了追溯 2026-05-21 那次 fixed-task memory ablation 修复与交接语境，**不再作为 route 默认主导航**。

> 日期：2026-05-21
> 目的：用户准备换对话，这份文档用于让下一位 AI **快速接住当前项目**，不要再被上下文压缩带偏。

---

## 1. 先说结论

- **当前开发项目就是 `routes/protagonist_ecology/`；根目录早期阶段内容只作为共享底座和历史记录**
- 最近真正收口的一件事是：**fixed-task memory ablation 已经从“入口可跑”推进到“修好 `worksite_return` 并完成 full run”**
- 下一步**不要**默认继续堆 `BrainV3` 壳层，也**不要**默认转去 GPU 线
- 最值得继续做的是：**解释 full run 里剩余不够干净的分离**
  - `signal_response` 为什么还偏弱
  - `water_return` 里的 `no_spatial` 为什么还有残余

---

## 2. 下一位 AI 的最小必读顺序

1. `AGENTS.md`
2. 顶层 `PROGRESS.md`
3. `routes/protagonist_ecology/docx/PROGRESS.md`
4. **本文件**
5. `docs/experience_log.md` 里这次新增的 memory ablation 记录
6. 如果要深挖路线优先级，再读：
   - `routes/protagonist_ecology/docx/supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
   - `routes/protagonist_ecology/docx/supporting/MEMORY_SYSTEM_MASTERPLAN.md`

补充说明：

- `routes/protagonist_ecology/docx/STATUS.md` 仍然是大 handoff 文档，但**这次最新的 fixed-task memory ablation 修复细节没有完整同步进去**
- 关于这次修复，优先信任：
  - 本文件
  - 顶层 `PROGRESS.md`
  - route `PROGRESS.md`
  - `docs/experience_log.md`

---

## 3. 当前代码真实状态

### 3.1 已经确认可用的能力

当前 protagonist route 已经具备：

- 共同训练（主角 + 背景物种）
- checkpoint / resume
- trace
- continuous-time 主链
- 标准 DNC
- Memory Phase A
- DNC micro-task 最小验证
- BrainV3 最小壳层
- mixed compute / CUDA build path（**仅说明可运行，不说明训练已提速**）

### 3.2 当前长蓝图完成度口径

按 `WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md` 的 phase 口径：

- **Phase 1 Trace/Resume：完整**
- **Phase 2/3/5/6/7：partial**
- **Phase 4/8：未真正开始**

所以现在的准确说法仍然是：

> **少量完整 + 多项 partial**

不要把 route 现状说成“大蓝图基本做完”。

---

## 4. 这次刚刚做完并验证过的事

### 4.1 问题背景

fixed-task memory ablation 的 runtime 入口之前已经接通，但真正跑 traceprobe 后发现：

- `water_return` 已经恢复
- `worksite_return` 仍然是 0

### 4.2 确认到的根因

根因不是 `WorksiteLayer` 本体坏了，而是**物流动作在同一个 tick 里互相打架**：

1. `GoalActionAssistCapability` 会直接帮 agent pickup stick / stone
2. 旧 `DropCapability` 还能在同 tick 把刚拿到的材料立刻丢掉
3. `ThrowCarriedObjectCapability` 会打断 stone logistics
4. `WorksiteReturn` 任务还允许 nursery logistics 干扰材料流向

### 4.3 已经落地的修法

#### 代码改动方向

- `routes/protagonist_ecology/capability/GoalActionAssistCapability.cpp`
  - 在 `GatherStick / GatherStone / BuildWorksite` 下，携带材料时优先回 remembered worksite
  - 没有 remembered worksite 时退回 remembered base

- `routes/protagonist_ecology/capability/GoalAwareDropCapability.{h,cpp}`
  - 新增 route-local drop capability
  - 在 logistics/build goal 下不让普通 drop 路径把 stick/stone 立刻丢掉

- `routes/protagonist_ecology/capability/ThrowCarriedObjectCapability.cpp`
  - 在 logistics/build 相关 goal 下抑制 stone throw

- `routes/protagonist_ecology/runtime/MemoryAblationEval.cpp`
  - `WorksiteReturn` 任务禁用 `nursery.enabled`

- 相关 wiring
  - `routes/protagonist_ecology/runtime/BootstrapPopulation.cpp`
  - `routes/protagonist_ecology/CMakeLists.txt`

#### 对应测试

- `tests/route_protagonist/test_goal_action_assist.cpp`
- `tests/route_protagonist/test_memory_ablation_eval.cpp`

---

## 5. 最新验证结果

### 5.1 full run 结果目录

- `data/runs/protagonist_memory_ablation_fixed/20260521-212657/`

关键文件：

- `memory_ablation_results.csv`
- `memory_ablation_summary.md`

### 5.2 4-episode full run 关键结果

#### water_return

- baseline = `162`
- `no_episodic` = `162`
- `no_social` = `162`
- `no_spatial` = `67.5`
- `no_goal` = `0`

#### worksite_return

- baseline = `0.5`
- `no_episodic` = `0.5`
- `no_social` = `0.5`
- `no_spatial` = `0`
- `no_goal` = `0`

#### signal_response

- baseline = `15.5`
- `no_spatial` = `13`
- `no_episodic` = `12.25`
- `no_social` = `13`
- `no_goal` = `0`

#### danger_avoidance

- baseline survived = `8`
- `no_episodic` survived = `6.25`

### 5.3 当前应有的结论

- `worksite_return` 的 recent bug **确实修好了**
- `no_goal` 仍然是最稳定的强 ablation
- `no_spatial` 在 `worksite_return` 上很干净，但在 `water_return` / `signal_response` 上还不够强
- 所以下一步该做的是**解释和任务 tightening**

---

## 6. 下一位 AI 接下来应该做什么

### 第一优先级

继续追这两个问题：

1. **`signal_response` 为什么分离偏弱**
   - baseline `15.5`
   - 非 goal ablation 仍有 `12.25~13`

2. **`water_return` 为什么 `no_spatial` 还有 `67.5`**
   - 说明当前 spatial ablation 对这项任务不是完全打断

### 推荐做法

- 不要先改大结构
- 先读结果表、trace、任务定义
- 优先判断是：
  - 任务太宽松
  - assist 还给了替代路径
  - perception / memory query 让 spatial ablation 仍可部分绕开
  - signal task 本身没有足够强的 communication pressure

### 暂时不要优先做的事

- 不要默认继续扩 `BrainV3`
- 不要默认继续 GPU profiling
- 不要默认清理大工作树
- 不要擅自开正式长训 / overnight

---

## 7. 当前工作树注意事项

### 7.1 仓库现在是脏树

`git status --short` 显示：

- 很多 **已修改** 文件
- 很多 **未跟踪** 文件

这些并不都来自这最后一小段工作，很多是前面路线演进留下的**未提交工作树**

### 7.2 下一位 AI 不要擅自清理

尤其注意这些东西：

- `build_vs3131/`
- `routes/protagonist_ecology/docx/data/`
- `routes/protagonist_ecology/docx/NUL`
- `routes/protagonist_ecology/docx/""`
- `routes/protagonist_ecology/docx/"" 2>''`
- `$testDir/...`
- `.tavily-state.json`

其中有些明显像误产物 / 垃圾文件，但**没有用户明确同意前不要删**

---

## 8. 给下一位 AI 的一句话指令

> 当前项目是 `routes/protagonist_ecology/`；fixed-task memory ablation 的 `worksite_return` 已经修好并跑完 full run。接下来不要再做“入口能不能跑”的重复劳动，直接去解释 `signal_response` 和 `water_return/no_spatial` 为什么分离还不够干净。
