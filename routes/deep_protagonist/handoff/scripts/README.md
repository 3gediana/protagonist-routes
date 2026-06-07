# handoff/scripts — 傻瓜单命令入口

所有脚本默认值写死、带 `Get-Help`/注释。跑法（在你的 VM 上，经 SSH）：
```bash
SSH="ssh -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -i ~/.ssh/devin_key -p 2222 sans@localhost"
HS="D:/claude-code/c++/routes/deep_protagonist/handoff/scripts"
$SSH "powershell -NoProfile -ExecutionPolicy Bypass -File $HS/monitor.ps1"
```
看任意脚本的帮助：`powershell -Command "Get-Help <path>\eval_frozen.ps1 -Detailed"`。

| 脚本 | 干啥 | 关键参数 |
|---|---|---|
| `build.ps1` | 编译 deep_protagonist_train.exe | （无） |
| `eval_frozen.ps1` | 冻结评测一个 ckpt、出涌现谱数据 | `-Ckpt runs\ft_bc10.pt -Secs 150 -Tag eval_frozen` |
| `analyze_spectrum.py` | 把 eval 的 jsonl 读成 23 动作/10 里程碑面板 | `python analyze_spectrum.py runs/eval_frozen.jsonl` |
| `train_hunt.ps1` | 随机出生+两阶段狩猎 PPO 微调（D-135） | `-Tag bc05 -Load runs\bc_hv2.pt -HuntShap 0.5 -Steps 8000000` |
| `bc_then_ppo.ps1` | 全链路：录示范→BC→PPO（分 stage 跑） | `-Stage record\|bc\|ppo -Tag hv2` |
| `monitor.ps1` | GPU + 进程 + 每个 ft_*.jsonl 的近期击杀趋势 | `-Recent 40` |
| `stop_and_save.ps1` | 停掉全部训练（检查点自动存，安全） | （无） |
| `cleanup_logs.ps1` | 只删日志/jsonl/trace，保留 *.pt/*.csv | （无） |

## 典型流程
1. 连机器：见 `../CONNECT.md`。
2. 看现状：`monitor.ps1`。
3. 想复现当前最好策略的谱：`eval_frozen.ps1 -Ckpt runs\ft_bc10.pt` → `analyze_spectrum.py runs\eval_frozen.jsonl`。
4. 想继续训练：`train_hunt.ps1 -Tag <新名> -Load runs\bc_hv2.pt`，然后 `monitor.ps1` 盯 prey_kills。
5. 用户要电脑：`stop_and_save.ps1`（要腾盘再 `cleanup_logs.ps1`）。

## 注意
- 这些脚本里的路径写死成 `D:\claude-code\c++\routes\deep_protagonist`。换机器要改路径。
- `kills=` 是奖励不是击杀数；真击杀看 `prey_kills`（见 `../PITFALLS.md`）。
- 后台训练用 `Invoke-CimMethod Win32_Process Create` 启动 → SSH 断了也不死。
