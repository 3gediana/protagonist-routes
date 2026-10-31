# F-HEAT · 行为热力图

> 群体的"脚印记忆"。最直观的涌现可视化。

- **难度**：🟢 低
- **确定性档**：🟢 高
- **前置条件**：M2 完美完成
- **状态**：📋 待开发

---

## 一句话

把所有个体经过的位置频率叠加在地图上，热门路径亮、冷门区域暗。

---

## 你会看到什么效果

### 完美版
开局热力图是均匀噪点。50 代后开始出现"热点"——食物附近、山洞口、某条山脊。100 代后能清晰看到"文明小径"：连接几个高价值资源点的固定路线被踏出来了。
你可以选"全代累积" / "最近 N 代" / "本代实时" 三种视图。

### 标准版
热力图能跑、能更新、能切换视图。

### 降级版
基本不会失败 —— 这是纯统计可视化。

---

## 实现要点

### 数据结构

```
src/core/observer/
└── HeatmapRecorder.cpp           订阅 AgentMoved 事件
```

```cpp
class HeatmapRecorder : public IObserver {
    Grid<float> heatmap;          // 200×200 网格，每格记录访问次数
    std::deque<Grid<float>> history;  // 每代一份快照（用于"最近 N 代"视图）

    void onAgentMoved(const Event& e) {
        auto pos = std::any_cast<Vec2>(e.payload);
        heatmap[discretize(pos)]++;
    }

    void onGenerationEnded(const Event& e) {
        history.push_back(heatmap);
        if (history.size() > config.history_window)
            history.pop_front();
        heatmap = decay(heatmap, config.decay_rate);
    }
};
```

### 渲染

```
src/viz/renderer/
└── HeatmapRenderer.cpp
```

把 heatmap 网格转成颜色：
- 低频 → 透明蓝紫色
- 中频 → 黄绿色
- 高频 → 红色（接近不透明）

可在 ControlPanel 切换显示模式：
- [全代累积]
- [最近 10 代]
- [仅本代]

可调透明度（让用户决定热力图是覆盖还是叠加在地形上）。

### 物种分色叠加（高级）

为每个物种维护一份独立 heatmap → 渲染时同一位置各物种比例不同 → 用混色显示。
**展示物种地理分化的最强可视化**——可以直接看到"红物种主要在山顶，蓝物种主要在山洞"。

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] 单层热力图能渲染并更新

### 完整 ✅✅⛳
- [ ] 全代/最近 N 代/本代三种视图切换
- [ ] 颜色映射美观
- [ ] 叠加在地形上不影响地形可读性

### 完美 ✅✅✅
- [ ] 物种分色叠加可用
- [ ] 跑 100 代的热力图截图归档（**进化前后对比**）
- [ ] git tag `module-F-HEAT-done`

---

## 🧠 你的补充点

| 项 | 决定什么 |
|---|---|
| **网格分辨率** | 默认 200×200。粗 → 模糊但快，细 → 清晰但占内存 |
| **decay_rate** | 历史信息保留多久 |
| **配色方案** | viridis / inferno / 自选 |

---

## 🔌 接入点

- 订阅 `AgentMoved` 事件 → 不影响 core
- viz 加 layer

---

## ⚠️ 易踩坑

1. **历史快照内存**：每代 200×200 float = 160KB，1000 代 = 160MB。**用环形缓冲限制窗口**
2. **位置事件频率**：每个 agent 每 tick 都发 AgentMoved，可能压垮 EventBus。**改为每 10 tick 采样一次**
3. **物种 ID 着色**：物种数随机变化，颜色映射要稳定（用 hash 或固定调色板）

---

## 答辩话术

> "这层叠加是这个种群的'集体脚印记忆'。开局是均匀的混乱，但你看现在第 100 代，明显的'高速公路'被踏出来了——这条路连接着这个山洞和那片食物点。**没有任何代码告诉它们这是最优路径，但群体的脚印自动找到了它。**"
