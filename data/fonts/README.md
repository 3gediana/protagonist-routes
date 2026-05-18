# data/fonts

放置 UI 使用的字体文件。

## 必需字体

- **思源黑体子集**（Source Han Sans CN, Regular）—— 用于 raylib 渲染中文字符
- 文件名建议：`SourceHanSans-Regular.ttf`
- 推荐使用子集化工具（如 `fonttools subset`）只保留需要的字符，避免完整字体几十 MB

## 字体来源

- 思源黑体（Adobe + Google 联合开发，SIL OFL 许可）
  https://github.com/adobe-fonts/source-han-sans

## 子集化字符集（建议）

```
1. CJK 常用 3500 字
2. ASCII 全集
3. 项目术语（在 configs/i18n/zh.toml 出现的所有字符）
4. 数学符号 ± × ÷ ≤ ≥ ≈ ∞ ∑ ∏
5. 标点 ， 。 、 ； ：
```

## 占位说明

字体文件不入仓（见 .gitignore），首次构建时需要手动放入或从 CI 下载。

> M2 实现时，`I18n` 模块负责字体加载逻辑。
