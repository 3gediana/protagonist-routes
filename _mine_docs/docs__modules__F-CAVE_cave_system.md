# F-CAVE · 进阶洞穴系统

> 把 M5 的简单山洞升级成多层、可探索、可堵塞的复杂洞穴。

- **难度**：🟡 中
- **确定性档**：🟢 高（视觉效果保证）
- **前置条件**：M5 完美完成
- **状态**：📋 待开发

---

## 一句话

山洞从单层 mask 升级为：多个洞口、可能相连、内部有"深处"提供更高庇护、洞口可被物体堵塞（配合 F-TOOL）。

## 你会看到什么效果

### 完美版
配合 F-TOOL，演化出"堵洞防御"行为：捕食者来袭时，幸存者用石头堵住洞口。

### 标准版
洞穴系统视觉丰富，agent 倾向于在深处群居。

## 实现要点

```
src/core/world/layers/cave/
├── CaveSystem.h / .cpp           多层洞穴数据
├── CaveGenerator.cpp             生成连通的洞穴系统
└── CaveTraversal.cpp             agent 在洞内的移动规则
```

数据结构：
```cpp
struct Cave {
    std::vector<CaveCell> cells;
    std::vector<CaveCell*> entrances;     // 洞口
    std::map<int, std::vector<int>> graph; // 洞穴内部连通图
};
```

## 三档完美标准

- MVP：多层洞穴生成可见
- 完整：agent 能进出，洞穴提供差异化的庇护值
- 完美：配合 F-TOOL 实现"堵洞防御"

## 答辩话术

> "M5 的山洞是单层 mask。这一层把它升级成完整的洞穴生态——洞口、深处、连通。这让'空间策略'真正成为进化的素材。"
