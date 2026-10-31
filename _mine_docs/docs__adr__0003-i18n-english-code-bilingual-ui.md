# ADR-0003 · 代码英文 + UI 中英双语切换

- **状态**：已采纳
- **日期**：2026-05-18

---

## 决策

- **代码层（src/）**：所有标识符（类名、函数、变量、配置 key）使用英文
- **代码注释**：英文为主，复杂逻辑可用中文注释解释（不影响编译）
- **文档（docs/）**：中文
- **UI 层**：所有展示给用户的字符串走 i18n 系统，**支持中英文一键切换**

---

## 为什么这样决定

### 用户的明确指令
> "代码英文，不看代码所以注释看不看无所谓。前端演示中英切换。"

### 这样安排的好处

- 代码可以直接拿出去给别人看 / 放 GitHub
- 文档保持中文，作为课设报告素材
- 演示时可以用中文给老师看，提问时可切英文显示术语

### i18n 系统设计

#### 简单实现
不引入 ICU / gettext 等重型库。自己写一个轻量字符串表：

```cpp
// src/viz/i18n/strings.h
enum class Lang { EN, ZH };
class I18n {
public:
    static void setLanguage(Lang lang);
    static const char* t(const char* key);
};

// 资源文件 configs/i18n/en.toml
[ui]
fitness_curve = "Fitness over Generations"
species_tree = "Phylogenetic Tree"
god_panel = "Intervention Panel"

// configs/i18n/zh.toml
[ui]
fitness_curve = "适应度曲线"
species_tree = "物种谱系树"
god_panel = "干预面板"
```

UI 渲染时调用 `I18n::t("ui.fitness_curve")` 而不是硬编码字符串。

#### 切换按钮
顶栏一个按钮 "EN / 中" 切换，立即重绘所有 UI 文本。

---

## 影响

- M2 可视化阶段必须先把 i18n 系统搭好，**不允许任何 UI 字符串硬编码**
- 新增 UI 文本必须同时更新两份语言资源
- 配置文件、文档、代码注释三处的语言策略明确分离

---

## 字体相关

raylib 默认字体不支持中文。需要在 viz 启动时加载支持 CJK 的字体（如思源黑体或思源宋体的子集）。

字体文件放在 `data/fonts/`。

> 详见 M2 阶段文档。
