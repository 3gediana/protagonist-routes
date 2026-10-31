# 资源上限与并发训练配置（机器实测）

> 给接手模型：这份记录本机（用户的 Windows 笔记本）实测出来的**并发训练上限**。
> 以后多假设并发筛参时，**照这个数跑，不要超**，否则会 GPU OOM 把训练进程崩掉。

## 1. 机器规格（实测）

| 项 | 值 |
|---|---|
| GPU | **RTX 5060 Laptop GPU，8GB 显存**（不是任何远程 A10/云卡） |
| CPU | 32 逻辑核 |
| 内存 | 32GB（训练时余量充足，不是瓶颈） |

查规格命令（经 chisel 隧道 SSH 进 Windows）：
```
nvidia-smi --query-gpu=name,memory.total,memory.used,memory.free,utilization.gpu --format=csv,noheader
```

## 2. 关键发现：绑定约束是【显存】，不是 CPU

最容易踩的坑：以为 DP 吃 GPU、PE 吃 CPU，两者井水不犯河水，可以各自拉满。**错。**

- **PE（`neural-eco-protagonist-batched`）也吃 GPU**：NEAT 的 brain 前向是 GPU-batched 的（配置里 `protagonist_dense_gpu_enabled = true`）。用
  `nvidia-smi --query-compute-apps=pid,process_name,used_memory --format=csv,noheader`
  能看到 PE 进程确实在 GPU compute 列表里。
- 所以 **DP 和 PE 抢的是同一块 8GB 显存**。判断"还能不能再开一个进程"要看 `memory.free`，不是看 CPU 空闲。
- WDDM 驱动下 `used_memory` 每进程显示 `[N/A]`，只能看**整卡** `memory.used/free` 的增量来估每个进程占多少。

## 3. 实测稳定并发上限：5 DP + 4 PE = 9 进程

| 进程组 | 数量 | 每个的关键参数 |
|---|---|---|
| DP 训练 `deep_protagonist_train` | **5** | `--n-envs 4`（故意调小以省显存，好多开几个） |
| PE 演化 `neural-eco-protagonist-batched` | **4** | 各一个 seed |

此配置下的占用（实测）：
- GPU：**≈6.1 / 8 GB 显存，≈84% 利用率**，留 ~1.75GB 安全余量。
- CPU：**≈91%**。

### 红线
- **不要超过 9 个训练进程**。再加第 10 个，`memory.free` 会跌破安全余量 → 某个 libtorch/PE 进程 GPU OOM 崩掉（而且往往是正在跑的那个，不是新开的）。
- 加进程时**一个一个加**，每加一个等 ~40s 让它分配完显存，再 `nvidia-smi` 看 `memory.free`，确认 >1.5GB 再加下一个。

## 4. 怎么并发起这些进程（一个 exe 多套配置）

### DP：env 变量调旋钮，不用编 N 个 exe
代码里把狩猎关键旋钮做成了 env 变量可覆盖（`AnimalSystem.cpp` 的 `env_f()`、`Environment.cpp` 的 `DP_SPEAR_REACH`）：

| env 变量 | 默认 | 含义 |
|---|---|---|
| `DP_SPEAR_REACH` | 3.0 | 长矛突刺有效距离(m) |
| `DP_RAB_STAM` / `DP_RAB_REST` / `DP_RAB_RECOVER` / `DP_RAB_TRIGGER` | 2.0/3.0/0.4/12 | 兔：冲刺秒数/喘息秒数/恢复速率/逃跑触发半径 |
| `DP_DEER_STAM` / `DP_DEER_REST` / `DP_DEER_RECOVER` / `DP_DEER_TRIGGER` | 2.5/3.2/0.4/16 | 鹿同上 |

启动脚本 `launch_variant.ps1`（在 `C:\Users\sans\`）：
```
powershell -File launch_variant.ps1 -tag vb_reach35 -envsets "DP_SPEAR_REACH=3.5" -nenvs 4
```
`-envsets` 用 `;` 分隔多个 `KEY=VAL`，会拼成 `cmd /c "set K=V&& ... exe ..."` 注入进程环境。每个变体独立 `runs\hunt_<tag>.{stdout,jsonl,pt}`，互不干扰。

### PE：克隆配置换 seed
PE 没有 CLI 旋钮，靠 toml 配置。`clone_pe.ps1` 复制可用的 `onepot_sD_warm_50002.toml`，只改三处（`random_seed` / `scenario_name` / `runs_dir`），共用同一个只读的 sC gen79 warmstart checkpoint，靠不同 RNG seed 跑出独立演化轨迹：
```
powershell -File clone_pe.ps1 -seed 50004
```

### ⚠️ 不能用的配置
- `onepot_sC_long_*.toml`（冷启动那批）**已过期**：会崩
  `action_dim does not match ... (configured=13 required=17 reserved=3)`
  ——配置是 action_dim=13 时代写的，当前二进制要 17。要冷启动得先把这些 toml 的 capability 配置补到 17，否则只能用 `sD_warm` 系列。

## 5. 监控

- 整机：`nvidia-smi --query-gpu=memory.used,memory.free,utilization.gpu --format=csv,noheader` + 自写的 `cpuchk.ps1`（数 `Win32_Processor` 平均 LoadPercentage + 列训练进程的 CPU/线程数）。
- DP 多变体对比：`analyze_all.ps1` 循环各 tag 的 stdout.log，输出每个变体的 `closest_prey MIN / ≤3m 回合数 / hunt_hits / KILLS / rest 窗口数`，一眼看谁先冒 kills。
