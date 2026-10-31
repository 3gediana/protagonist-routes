# M5 · 真实 2.5D 地形

> 让世界从光秃秃的平面变成有山有谷有山洞的真实生境。
> 个体能感知地形、行为受地形物理影响，渲染用等距投影呈现 2.5D。
> **接口压力测试 #2**：M0 设计的 `IWorldLayer` 和 `IPerception` 接口能否承载新的世界元素？
> **执行顺序更新**：按 [ADR-0005](../adr/0005-defer-visualization-until-mechanics-stabilize.md)，M5 当前先完成 terrain 的 core 机制（layer / perception / physics / resource coupling），而可视化相关的等距渲染与视觉验收会顺延到后续统一展示整合阶段。

---

## 目标

依据 [ADR-0001](../adr/0001-terrain-is-real-physics-not-decoration.md)：

1. **TerrainLayer** —— 高度图 + 山洞 mask，作为世界的一等公民
2. **TerrainPerception** —— 个体能感知自身位置的高度、坡度、是否在洞内
3. **物理影响** —— 移动速度、能量消耗、视野遮挡都受地形影响
4. **2.5D 渲染** —— 等距投影呈现，不做真 3D
5. **核心代码 0 修改** —— 只新增 layer 和 perception，evolution / world / agent 主框架不动

---

## 前置条件

- M4 完美通过（`baseline-M4-done` tag）
- 你已经决定了地形生成的整体审美（见 🧠 补充点）

---

## 关键产出

### TerrainLayer

```
src/core/world/layers/terrain/
├── TerrainLayer.h / .cpp           实现 IWorldLayer
├── HeightField.h / .cpp            连续高度查询（双线性插值）
├── CaveMask.h / .cpp               山洞标记 + 进入/离开判定
├── TerrainGenerator.h              地形生成接口
└── generators/
    ├── PerlinNoiseGenerator.cpp    Perlin 噪声生成基础地形
    ├── VoronoiCaveGenerator.cpp    Voronoi 划分山洞
    └── HandcraftedGenerator.cpp    从图片或 TOML 加载手工地形
```

接口（关键查询方法）：
```cpp
class TerrainLayer : public IWorldLayer {
public:
    float heightAt(float x, float y) const;        // 双线性插值，连续
    Vec2 slopeAt(float x, float y) const;          // 高度梯度（dh/dx, dh/dy）
    bool inCave(float x, float y) const;
    bool isVisible(Vec2 from, Vec2 to) const;     // 视线遮挡检查（Bresenham + 高度比较）
};
```

### TerrainPerception

```
src/core/perception/
└── TerrainPerception.h / .cpp
```

输出向量（5 维）：
```
[ self_height_normalized,
  slope_x, slope_y,
  in_cave (0 or 1),
  forward_visibility_distance ]
```

加入个体感知输入向量后，**Genome input_dim 增加 5**——M3 设计 NeatGenome 时已确保动态维度，这里平滑通过。

### 物理影响 hook

在 `MoveCapability` 中查询 TerrainLayer：
```cpp
Vec2 desired_velocity = brain_output;
float slope_magnitude = world.terrain.slopeAt(pos).magnitude();
float terrain_factor = 1.0f / (1.0f + slope_magnitude * config.slope_resistance);
Vec2 actual_velocity = desired_velocity * terrain_factor;

// 上坡耗能
float energy_cost = base_movement_cost
                  + uphill_factor * dot(actual_velocity, slope_vector);
```

视野遮挡在 `VisualPerception`（如果已有）或在视线相关 capability 里调用 `terrain.isVisible()`。

### 山洞机制

```cpp
class CaveMask {
public:
    bool isCave(int x, int y) const;
    bool justEntered(Vec2 from, Vec2 to) const;
    bool justLeft(Vec2 from, Vec2 to) const;
};
```

进入/离开山洞触发事件：
```
EventType::AgentEnteredCave
EventType::AgentLeftCave
```

山洞内的特性：
- 从外部 raycasting 视线被山洞顶遮挡（`isVisible` 返回 false）
- 不被远程感知到（其它 agent 的 visual perception 看不到洞内 agent）
- 提供"庇护值"（在洞内能量消耗速率减半，可选机制）

### 2.5D 等距渲染

```
src/viz/renderer/
├── IsometricProjection.h / .cpp    世界坐标 → 屏幕坐标
├── TerrainRenderer.cpp              渲染地形高度图
└── CaveRenderer.cpp                 山洞顶层（半透明覆盖）
```

实现思路（不做真 3D）：
1. 高度图按 tile 分块（每 tile 4×4 像素一个高度值）
2. 等距投影公式：
   ```
   screen_x = (x - y) * cos(30°) + offset_x
   screen_y = (x + y) * sin(30°) - height(x,y) * z_scale + offset_y
   ```
3. 颜色按高度梯度（深蓝低谷 → 绿色平原 → 棕色山地 → 白色雪峰）
4. 山洞用深色覆盖层 + 半透明
5. **从后到前渲染保证遮挡正确**（painter's algorithm）

### 资源分布与地形耦合

`ResourceLayer` 升级支持地形敏感的食物分布：
```toml
[resource_distribution]
plain_density = 0.02       # 平原食物密度
mountain_density = 0.005   # 山顶稀有资源
cave_density = 0.01        # 山洞特殊资源
```

让不同地形上的食物有不同价值（高山资源 +50 能量，平原食物 +10），**逼出"爬山策略"的进化压力**。

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] TerrainLayer 编译并接入 World，无崩溃
- [ ] 高度图随机生成且查询稳定
- [ ] 个体在斜坡上速度变慢

### 完整 ✅✅⛳
- [ ] 山洞机制工作（进出事件触发，视野遮挡正确）
- [ ] 等距渲染设计与数据接口准备好（实际整合顺延到统一展示阶段）
- [ ] 资源按地形分布
- [ ] TerrainPerception 接入 NEAT 输入，跑 50 代不崩
- [ ] **M0 接口 0 修改**通过（IWorldLayer / IPerception 接口设计的胜利）

### 完美 ✅✅✅
- [ ] **观察到地形分化**：跑 100 代后，种群在不同地形（平原/山顶/山洞）的密度有显著差异（用 KS 检验或熵变化证明）
- [ ] 视觉验收项顺延到统一展示整合阶段
- [ ] 渲染性能验收顺延到统一展示整合阶段
- [ ] git tag `baseline-M5-done`

---

## 🧠 你的补充点（这是地形美感的灵魂）

| 项 | 你需要决定 |
|---|---|
| **地形生成器选择** | Perlin 噪声（默认，自然） / Voronoi（块状） / 手工地图（你画的） |
| **山的密度和高度** | Perlin 频率和振幅参数 |
| **山洞分布** | 多少个、多大、是否相连 |
| **资源-地形耦合** | 平原/山顶/山洞食物的能量值和密度比例 |
| **视觉配色** | 6 段地形颜色（深海/浅海/沙滩/平原/山地/雪峰）—— 影响演示美感最大的一步 |
| **是否做天气** | 默认无。你可以加日夜或简单的雾效（视觉锦上添花） |

---

## 🔌 接入点

- F-CAVE 模块扩展山洞机制（多层洞穴、洞穴内特殊资源、洞穴坍塌等）
- F-TOOL 模块允许个体改变地形（挖掘/堆积），需要 TerrainLayer 支持运行时修改
- F-DAY F-SEASON 等环境模块可叠加在地形上
- 答辩演示主屏幕从 M5 开始**视觉效果显著提升**——这是分水岭

---

## ⚠️ 易踩坑

1. **高度查询性能**：`heightAt()` 每帧每 agent 都调用，N=200 时每秒几万次。**双线性插值要用查找表**或 SIMD
2. **视线遮挡 cost**：朴素 raycasting 每对 agent 都查太慢。**优化**：只对邻近 agent 做、用 grid spatial hashing
3. **等距投影的 Z 排序**：painter's algorithm 在地形复杂时会有错误重叠。**简化**：把地图划分成 chunk，按 chunk 中心远近排序，chunk 内不做精确排序
4. **山洞与高度图的关系**：山洞应该在"高度低于周围 + 有顶盖"的位置。生成时要保证一致性。**简化**：cave_mask 完全独立于 height_field，避免几何耦合
5. **地形过陡导致行动不能**：如果某些区域坡度大到 terrain_factor ≈ 0，agent 永远卡住。**加 minimum velocity floor**

---

## 基线完成

M5 完美 = baseline 全部完成 = `baseline-complete` tag。

恭喜，**你现在拥有一个能跑、能看、能进化的神经进化生态箱**。
适应度上升 + 拓扑增长 + 物种分化 + 地形分化——四大基础涌现到位。

下一步打开 [docs/modules/](../modules/) 选择你想加的功能模块。
