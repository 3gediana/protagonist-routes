# deep_protagonist · PROGRESS

> 最后更新：2026-05-24 凌晨 03:30
> 当前阶段：**S3.7 训练前置机制完成 ✅，S4 PPO 准备就绪**

## ✅ 本次会话完成：S3.7 训练前置机制

用户睡前指示："美化不重要，先把世界机制完善好让训练能出效果"。AI 重读最终蓝图发现 S3.5 视觉整改其实违反 line 79（"❌ 真实美术资源，留给 S6"），于是推翻 S3.5，直接做 S3.7（训练前置机制）。

### S3.7 完成清单（按依赖顺序）

| 子阶段 | 内容 | 状态 |
|---|---|---|
| **S3.7a** | `AgentAction` 接口（PPO 输入定义；wander 改成 action-driven）| ✅ |
| **S3.7b** | `RewardSystem` v1（10 个奖励组件，按"加新任务保留旧奖励"）| ✅ |
| **S3.7c** | `EpisodeManager`（max_seconds=300、死/超时 reset；地形保留）| ✅ |
| **S3.7d** | `ObservationBuilder`（124 维 PPO 输入向量，全部归一化）| ✅ |
| **S3.7e** | `ResourceSystem`（干柴 600 / 石 400 / 长草 800，按 biome 分布）| ✅ |
| **S3.7f** | `Inventory`（3 格背包，木+石→长矛 craft）| ✅ |
| **S3.7g** | `Shelter`（1W+1G 建造，3m 半径覆盖体温保护）| ✅ |
| **S3.7h** | Wolf 夜行加强（perception +50%, speed +20% × night_factor）| ✅ |
| **集成** | main.cpp 接入 R/B/C 键、HUD 显示 inventory + shelter + episode + reward | ✅ |
| **可视化** | WorldRenderer 画出 wood/stone/grass 几何体 + shelter A-frame | ✅ |
| **self-test** | 64 个测试全过（原 32 + 新增 32）| ✅ |

### 阶段路线

| 阶段 | 内容 | 状态 |
|---|---|---|
| **S0** | 项目骨架 + libtorch + CUDA 跑通 | ✅ |
| **S1** | 3D 世界基础（高度图 + 洞穴 + 物理） | ✅ |
| **S2** | 1024m 大世界 + 6 biome + 河流 + 昼夜 | ✅ |
| **S3** | 动物 + 植被 + agent 生命系统 | ✅ 机制完成 |
| **S3.7** | **训练前置机制**（action/reward/episode/obs/资源/背包/庇护/夜狼）| ✅ 本次完成 |
| **S4** | PPO 训练（觅食 + 喝水 + 躲狼）+ 5-10M 模型 | ⬜ **下一阶段** |
| **S5** | 工具 + 生火 + 扩 50M + 长训 | ⬜ |
| **S6** | 视觉美化 + 用户干预 + 答辩预演 | ⬜ |

## 🔴 下次会话第一件事：进入 S4

S4 = 用 libtorch 实现 PPO，让 agent 自己学会觅食/喝水/躲狼。

### S4 准备就绪清单（已具备）
- ✅ AgentAction：动作格式（神经网络输出格式）
- ✅ RewardSystem：标量 reward 信号（PPO 优化目标）
- ✅ EpisodeManager：一局边界（advantage 计算需要）
- ✅ ObservationBuilder：124 维输入向量（神经网络 input）
- ✅ Wander baseline：可对比"网络学到了 vs 随机走"

### S4 第一步要做的事
1. **设计网络架构**：MLP + GRU/LSTM + actor-critic head（输入 124 → 隐 256/128 → 动作分布 + value）
2. **实现 PPO loop**：rollout collect → advantage compute → policy/value update
3. **跑通最简 smoke test**：1 个环境跑 10000 步，看 reward 是否上升
4. **接 28 并行环境**：CPU 多线程 rollout，GPU forward+backward
5. **接 checkpoint + resume**

预估：开发 1-2 天 AI 时间。训练时长 1-2 周（觅食基础任务）。

## 最近会话摘要

### 2026-05-24 凌晨 03:45 · S3.9 地形适应
- **swamp 减速**：CapsuleAgent.apply_action 在 swamp biome 时 move_speed × 0.5
- **desert 白天加速失水**：VitalSystem.tick 在 desert + sun_h>0.4 时 thirst 损耗 × 2
- **69 self-test 全过**
- 遗留：默认 World seed 下 mountain 占 60%+ 挤掉了 desert，BiomeMap 阈值调整推迟

### 2026-05-24 凌晨 03:35 · S3.8 蓝图覆盖度补全
- 5 个机制让蓝图 line 16-25 都落地
  - **8a** Agent attack（F 键，spear 加 reach+damage）
  - **8b** Wolf 狩猎 rabbit/deer（生态链）
  - **8c** Shelter 防狼（窝内 perception×0.3, speed×0.7）
  - **8d** 抓鱼（Q 键 in river/swamp 同时抓 fish）
  - **8e** Mushroom 中毒 30%
- **67 self-test 全过**（原 64 + 新 3）

### 2026-05-24 凌晨 03:30 · S3.7 训练前置机制
- 用户睡前指示："美化不重要，机制完善优先"
- AI 重读最终蓝图，发现 S3.5 视觉整改违反 line 79 蓝图条款，**推翻**
- 改做 S3.7：8 个机制 + 集成 + 可视化 + self-test 扩展
- **代码量**：约 1100 行新增（include 7 hpp + src 7 cpp + main.cpp 集成）
- **64 个 self-test 全过** (a-h + Observation + Episode 系列)
- 新文件：
  - `include/agent/AgentAction.hpp` (43 行)
  - `include/agent/RewardSystem.hpp` (130 行) + `src/.../cpp` (74)
  - `include/agent/EpisodeManager.hpp` (71 行) + `src/.../cpp` (60)
  - `include/agent/Inventory.hpp` (64 行) + `src/.../cpp` (55)
  - `include/agent/Shelter.hpp` (42 行) + `src/.../cpp` (29)
  - `include/agent/Observation.hpp` (75 行) + `src/.../cpp` (223)
  - `include/world/ResourceSystem.hpp` (74 行) + `src/.../cpp` (127)
- **未来 AI 接力提示**：S4 的 PPO loop 直接消费 ObservationBuilder + AgentAction + RewardSystem + EpisodeManager 这 4 个对象，**机制已经完整**

### 2026-05-24 凌晨（之前会话）
- 立项 deep_protagonist 路线，与 protagonist_ecology 并行
- 写完 AGENTS.md 协作契约（极简版 55 行）
- 工具链确认（MSVC 14.44 + CMake/Ninja + CUDA 12.8）
- 资源预算：CPU 28 并行 / GPU 7GB / RAM 26GB
- **S0 完成**：
  - libtorch 2.11.0 + cu128 下载（2.65GB）+ 解压（5.3GB）
  - CMakeLists.txt + tomlplusplus + spdlog（FetchContent）
  - main.cpp smoke test 跑通（GPU 矩阵乘 40ms / TOML 读取 / 彩色日志）
  - 踩坑：中文 VS 路径 `E:\c语言` 让 vcvars64 挂掉，改用 C:\ 的 BuildTools
  - 踩坑：libtorch 下载中途断 5x，curl `-C -` 续传成功

## 下一步建议

S2 完成（1024m × 1024m 世界 + 6 biome + 3 河流 + 6 洞穴 + 4854 装饰物 + 昼夜循环）。
后台正在从 poly.pizza 下载 Quaternius CC0 资源（48 bundle，~3GB），不阻塞主线。

**进入 S3**：带生物的植被/动物系统：
- 植被：果树/浆果/蘑菇/仙人掌（可采集，按 biome 生长）
- 动物：兔/鹿/狼/鱼/鹰（带状态机 AI，狼追兔追 agent）
- 替换旧 FoodSystem（蓝球作废）
- agent 增加生命系统：饥饿/口渴/体力/体温

## 关键路径

- 项目根：`routes/deep_protagonist/`
- 协作契约：`AGENTS.md`
- 4 份速查：`docs/术语速查.md` / `参数速查.md` / `决策记录.md` / `Q&A速查.md`
- 编译入口：`scripts\configure.ps1` → `scripts\build.ps1`
- 主程序：`build\bin\deep_protagonist.exe`

## S0 验收记录

```
[01:27:13] libtorch version : 2.11.0
[01:27:13] CUDA available   : true / CUDNN: true / Devices: 1
[01:27:13] 100x [1024x1024] matmul on GPU: 40.40 ms (0.40 ms/iter)
[01:27:13] S0 SMOKE TEST PASSED
```

## S1 验收记录

```
[01:50:37] libtorch + CUDA 仍正常
[01:50:37] World: 256x256 cells @ 1m, max_height=30m, caves=3
[01:50:38] Window open. WASD+Mouse to fly, Ctrl=fast, Esc=quit
```

代码量：约 700 行 C++（包含 Heightmap / CaveSystem / Physics / CapsuleAgent / FlyCamera / WorldRenderer / main）

S1 实现内容：
- 程序化分形噪声地形（自实现 value noise + fBm，无第三方依赖）
- 3 个长方体洞穴（procedural generated）
- 胶囊物理：重力 + 地形碰撞 + 洞穴底/顶碰撞 + 摩擦/空阻
- "wander" agent：每 2 秒换一次方向，随便走（神经网络 S2 加）
- raylib 3D 地形 mesh（含按高度的颜色渐变）+ 半透明洞穴可视化
- First-person 飞行相机（WASD+鼠标）+ G 键切换 agent 跟随

S1 踩坑：
- MSVC 不支持 GCC 复合字面量 `(Vector3){...}`，全部改成 `Vector3{...}`
- GitHub TLS 偶发失败，改用顶层项目 `_deps` 已下载源码（FETCHCONTENT_SOURCE_DIR_*）
