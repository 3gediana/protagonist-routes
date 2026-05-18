# M2 · 可视化接入

> 把 M1 的黑箱训练变成"看得见的进化"。这一阶段让调试体验起飞。
> **执行顺序更新**：按 [ADR-0005](../adr/0005-defer-visualization-until-mechanics-stabilize.md)，当前项目先走 `M1 -> M3 -> M4 -> M5` 的 headless-first 机制路线，M2 暂时作为**后期统一展示层整合阶段**保留，而不是当前的下一步。

---

## 目标

接入 raylib，让 M1 的训练过程实时可见。
**核心铁律**：viz 模块只通过订阅事件读取 core 数据，**绝不直接调用 core 的修改方法**。

---

## 前置条件

- M5 核心机制稳定（NEAT / async / terrain 已 headless-first 跑顺）
- raylib 已通过 vcpkg 或 FetchContent 接入

---

## 关键产出

### viz 模块结构

```
src/viz/
├── Viewport.h / .cpp           主窗口管理
├── i18n/                       国际化（ADR-0003）
│   ├── I18n.h / .cpp
│   ├── strings_loader.cpp
│   └── (语言资源在 configs/i18n/)
├── renderer/
│   ├── WorldRenderer.cpp       世界渲染（食物、个体）
│   ├── AgentRenderer.cpp       个体细节渲染（朝向、能量条）
│   └── BackgroundRenderer.cpp  背景（地图边界等）
├── ui/
│   ├── HUD.cpp                 顶栏：FPS、当前代数、种群大小
│   ├── ControlPanel.cpp        左侧控制：开始/暂停/加速
│   ├── StatsPanel.cpp          右侧统计：当前最优 fitness、平均 fitness
│   └── LangSwitcher.cpp        语言切换按钮
└── plot/
    ├── LinePlot.cpp            实时折线图
    └── FitnessChart.cpp        适应度曲线专用
```

### 主循环（InteractiveRuntime）

```
InteractiveRuntime.run():
  initWindow(1280, 720, "Neural Eco")
  loadFonts()                  // 加载 CJK 字体（ADR-0003）
  loadLanguageResources()
  initCore()                   // 复用 M1 的初始化逻辑
  setupViewportSubscriptions() // viz 订阅 core 事件

  while (!shouldClose) {
    handleInput()
    coreTick()                 // 调用 World.tick()
    BeginDrawing()
      WorldRenderer.draw()
      HUD.draw()
      ControlPanel.draw()
      StatsPanel.draw()
      LangSwitcher.draw()
    EndDrawing()
  }
```

### 中英文一键切换

按钮 "EN / 中" 在顶栏。点击后 `I18n::setLanguage()` 触发 UI 全部重绘。
**所有 UI 字符串走 `I18n::t("ui.xxx")`，零硬编码字符串**——通过编译期检查（如自定义宏 `T()`）保证。

资源示例（`configs/i18n/zh.toml`）：
```toml
[ui]
hud.generation = "第 {gen} 代"
hud.population = "种群 {n}"
panel.start = "开始"
panel.pause = "暂停"
panel.speed = "倍速"
panel.species_tree = "物种谱系"
stats.avg_fitness = "平均适应度"
stats.best_fitness = "最优适应度"
lang.switch = "EN / 中"
```

英文版同理（`configs/i18n/en.toml`）。

### 控制面板交互

| 控件 | 功能 |
|---|---|
| ▶ / ⏸ | 暂停 / 继续 |
| 🐢 / 🐇 | 1x / 4x / 16x / 64x 倍速 |
| ⏭ | 单步进 |
| 🔄 | 重置当前代 |
| 📷 | 截图（保存到 `data/screenshots/`） |

### 实时数据可视化

- **个体绘制**：小圆点，颜色按 fitness 渐变（红→黄→绿）
- **食物绘制**：绿色小方块
- **轨迹叠加**（可关闭）：每个个体最近 N 帧的位置淡化连线
- **适应度曲线**：右侧面板实时滚动，X 轴是代数，Y 轴是 fitness

### 字体处理

raylib 默认字体不含中文。处理：
1. 在 `data/fonts/` 放一份开源 CJK 字体（思源黑体子集即可，约 2MB）
2. 启动时 `LoadFontEx()` 加载，传给所有 `DrawTextEx()` 调用
3. 可视化层封装一个 `Text::draw(key, x, y)` 函数，自动调用 i18n 和正确字体

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] 启动 `--mode interactive` 后窗口打开
- [ ] 能看到个体在地图上移动
- [ ] 中英切换按钮生效

### 完整 ✅✅⛳
- [ ] 所有 UI 字符串走 i18n（grep 全代码无硬编码字符串）
- [ ] 控制面板所有按钮工作
- [ ] 适应度曲线实时更新
- [ ] 个体颜色按 fitness 渐变
- [ ] 截图功能可用
- [ ] FPS 稳定 ≥ 30（M1 数据规模下）

### 完美 ✅✅✅
- [ ] 视觉验收：截图归档到 `data/screenshots/M2_milestone.png`
- [ ] CJK 字体清晰渲染（无锯齿、无方框）
- [ ] 64x 倍速下不卡顿（说明渲染解耦做对了）
- [ ] **不读 core 私有数据**：viz 只通过 IObserver 订阅事件获取数据，验证方法是把 core 模块改成静态库后 viz 仍能编译
- [ ] git tag `baseline-M2-done`

---

## 🧠 你的补充点

| 项 | 你需要决定 |
|---|---|
| **配色** | 个体颜色策略（fitness 渐变 / 物种染色 / 自选）—— 影响视觉品味 |
| **窗口布局** | 默认 1280×720。但比例你可以调（地图占多少、统计面板占多少） |
| **字体选择** | 我默认推荐思源黑体。你可选自己喜欢的 CJK 字体 |
| **倍速档位** | 默认 1x/4x/16x/64x。你可以加 256x 或慢动作 0.25x |
| **轨迹长度** | 默认显示最近 60 帧轨迹。你可以调 |
| **i18n 文案润色** | 我会先填一版功能性翻译，你可以润色得更好听 |

---

## 🔌 接入点

- M3 NEAT 来了之后，会增加"网络结构图"侧栏（订阅 `SpeciesFormed` 事件画拓扑），M2 不需要预留
- M5 地形来了之后，需要切换为等距投影渲染——**这就是 M2 接口压力测试**：渲染层能否承载从平面到等距的切换？
- F-TREE 物种树会作为新的 panel 加入
- F-GOD 上帝按钮会扩展 ControlPanel

---

## ⚠️ 易踩坑

1. **i18n 太晚做**：很多人想"先硬编码英文，反正能跑了再国际化"。**这条路是死路**。一旦硬编码满项目散布，事后清理是地狱。**M2 第一天就上 i18n 系统**
2. **直接读 core 数据**：图省事在 viz 里 `extern World g_world` 这种写法。**严格禁止**。所有数据通过 EventBus 流到 viz
3. **raylib 主循环阻塞 core**：默认 raylib 是同步主循环。core 的 tick 必须放在主循环里，但**不能让单次 tick 阻塞渲染**——倍速时一帧内跑多个 core tick，每跑一定数量就让出
4. **CJK 字体内存爆炸**：完整思源黑体几十 MB。**只加载子集**（CJK 常用字 + 技术术语 + 数字标点），约 1-3MB

---

## 下一阶段

→ baseline 机制路线完成后，进入模块库或统一展示整合
