# scripts

数据分析与辅助脚本（Python）。**不是项目主代码**，只用于离线分析训练产出的 CSV / JSONL 数据。

## 已实现脚本

| 脚本 | 作用 | 阶段 |
|---|---|---|
| `plot_fitness.py` | 画单次或多次运行的适应度曲线（CSV → PNG） | M1 |
| `run_m1_acceptance.py` | 批量跑多 seed 验收、汇总 CSV/JSON/Markdown、自动出图 | M1 |

## 计划脚本

| 脚本 | 作用 | 阶段 |
|---|---|---|
| `plot_diversity.py` | 画种群多样性曲线 | M1 |
| `analyze_neat.py` | NEAT 物种数量 / 拓扑复杂度演化 | M3 |
| `plot_phylogeny.py` | 物种谱系树（离线分析版，F-TREE 是在线版） | M3 / F-TREE |
| `signal_mutual_info.py` | 信号-情境互信息（验证 F-COMM 涌现） | F-COMM |
| `compare_runs.py` | 多次运行结果对比 | F-COMPARE |

## 依赖

```
pip install pandas matplotlib seaborn networkx scipy
```

## 数据来源

所有训练运行写入 `data/runs/<timestamp>/`。脚本读取此目录的 CSV / JSONL 产出图表到 `data/runs/<timestamp>/plots/`。
