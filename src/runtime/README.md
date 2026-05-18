# runtime

> 组装层。决定"用什么 core 配置 + 是否带 viz"。

## 子目录

| 目录 | 职责 | 实现阶段 |
|---|---|---|
| `headless/` | 训练运行时，无渲染 | M0（骨架）→ M1（实事） |
| `interactive/` | 演示运行时，带 viz | M0（骨架）→ M2（实事） |

## 入口

`src/main.cpp` 解析 CLI 参数 `--mode {headless,interactive}` 后分发。

```
neural-eco --mode headless --config configs/default.toml
neural-eco --mode interactive --config configs/default.toml
```

## 铁律

1. **headless 不链接 viz**（CMake `if(NEURAL_ECO_BUILD_VIZ)` 保护）
2. **runtime 才是"组装"的地方**，core 模块永远不知道运行时是 headless 还是 interactive
3. F-FORK / F-COMPARE 等模块通过新增子目录的方式接入，不动既有 runtime
