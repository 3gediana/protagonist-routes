# viz

> 可视化层。**依赖 core，绝不反向**。

interactive 运行时使用 viz；headless 运行时**不链接** viz（保证训练 0 渲染开销）。

## 子目录

| 目录 | 职责 | 实现阶段 |
|---|---|---|
| `i18n/` | 中英切换的字符串系统 | M2（**第一天就要上**） |
| `renderer/` | 世界 / 个体 / 地形 / 信号场 渲染 | M2 → M5 加 isometric |
| `ui/` | HUD / 控制面板 / 统计面板 / 上帝面板 | M2 → 各 F-* 模块扩展 |
| `plot/` | 适应度曲线 / 物种树 / 热力图 | M2 → F-TREE / F-HEAT |

## 铁律

1. **只通过 IObserver 订阅事件读 core 数据**，不直接调用 core 的修改方法
2. **所有 UI 字符串走 I18n::t("key")，零硬编码**（详见 ADR-0003）
3. **每个 panel 是独立 widget**，不与其它 panel 隐式耦合
