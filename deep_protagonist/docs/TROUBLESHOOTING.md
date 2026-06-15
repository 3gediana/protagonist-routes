# Troubleshooting · 踩过的坑

> 每条都标"症状 → 真因 → 解法"。给未来的我 / AI 看。

## 编译类

### 1. ninja 报 `FAILED:` 但 obj/exe 实际生成了

**症状**：ninja 输出末尾 `FAILED: bin/deep_protagonist_train.exe`，但 `ls build/bin/` 看到 exe 已经生成、时间戳更新。

**真因**：上一次 build 失败的输出被 ninja 缓存到 stdout，混进了这次 build 的日志。看 exe 时间戳判断真实状态，不要被 "FAILED" 字眼骗。

**解法**：build 完检查 `stat -c "%y" build/bin/deep_protagonist_train.exe` 是不是最近时间。

### 2. 改了 `.hpp` 但 ninja 说 "no work to do"

**症状**：改 `include/policy/PPO.hpp` 后 ninja 不重编 obj。

**真因**：ninja 用 cl.exe 的 `/showIncludes` 输出生成依赖文件。MSVC 的 `/showIncludes` 在中文 locale 下打印 "ע��: �����ļ�:" 之类的乱码，ninja 解析失败 → 依赖丢失。

**解法**：手动删 obj 触发重编：
```bash
rm build/CMakeFiles/deep_protagonist_train.dir/src/main_train.cpp.obj
rm build/CMakeFiles/deep_protagonist_train.dir/src/policy/PPO.cpp.obj
```

### 3. LNK1104: 无法打开输出文件

**症状**：链接阶段报 `LNK1104: 无法打开文件 bin\deep_protagonist_train.exe`。

**真因**：上一次训练进程还在跑（即使 shell 看着已经退出，windows 句柄延后释放）。

**解法**：
```bash
cmd.exe /c "taskkill /F /IM deep_protagonist_train.exe"
sleep 3
rm -f build/bin/deep_protagonist_train.exe
```

### 4. cl.exe 报错信息看不到（ninja 吞了）

**症状**：build 失败但 stdout/stderr 都空，看不到任何 error。

**真因**：cl 的错误输出写到了 stdout，但 ninja 在管道里把它截掉了一部分。

**解法**：写 `.bat` 脚本直接调 cl.exe，重定向输出到文件：
```bat
cl.exe ... 1>cl_out.txt 2>&1
```
查 `cl_out.txt`，搜 `error C` / `warning C4267` 等。中文乱码可以忽略。

## 训练类

### 5. PPO entropy 一路掉到 < 1（policy collapse）

**症状**：训练几 万步后 entropy < 1，某个动作触发率 > 60%。

**真因**：见 D-039 的 wear_clothes 65% 故事。Bernoulli 或 Categorical 头都会塌缩到"廉价 noop"（按了无效果也无成本的动作）。

**解法**：先抓 `analyze_actions.py runs/xxx.jsonl 20` 看 last 20 ep 分布。如果某动作 > 30%，要么 (a) 调高 `ent_coef`、(b) 加 `trigger_cost`、(c) 给这个动作加 NOOP 检测。

### 6. r_water 暴涨到 +6000（reward hacking）

**症状**：某 episode reward = +6000，但 agent 全程站着不动。

**真因**：D-036 的经典 bug。`event_drink(35.0f)` 不管 thirst 满没满都按 +1.05 reward 给。

**解法**：`vitals.drink(s, ml)` 返回 `actual_delta`，调用方 `reward.event_drink(actual_delta)` 而不是 `event_drink(ml)`。已修复，留作经验。

### 7. agent 学会喝水但不学其他

**症状**：drink 占触发 30%+，r_milestone / r_shelter 一直 0。

**真因**：见 Q-023。drink 是 reward density 最高的稳定路径 ( +1.8/s )，PPO 锁死。

**解法**：D-040 调慢 thirst_decay。但要小心调过头 → agent 不需要喝水 → 又退化到 NOOP（D-040 v6 的故事）。建议 4-8/min 之间 sweep。

### 8. last 20 ep mean 看着不错但 trend 显示退化

**症状**：训练日志看每个 ep R=+15 都还行，但 `analyze_actions.py xxx trend` 显示 NOOP 22%→52%、milestone +0.8→0。

**真因**：mean 只是终态平均，看不到策略**演化方向**。

**解法**：长训完一定跑 trend，看前后对比。如果某些有奖励的行为退化（craft/shelter/milestone 下降），说明 reward shaping 没拽住主线。

## 文件 / 路径类

### 9. WSL 下 cmake 找不到

**症状**：`cmake: command not found`。

**真因**：项目在 Windows 上用 MSVC，WSL 没装 Linux 版 cmake。

**解法**：用 Windows 版 cmake：
```bash
"/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build "D:\path\to\build" --target deep_protagonist_train
```
或直接用 `ninja.exe`：`G:/ai/qtdownload/Tools/Ninja/ninja.exe -C build deep_protagonist_train`。

### 10. WSL 命令带很多引号 / 反斜杠不工作

**症状**：长命令 `cmd.exe /c "vcvars.bat && cl.exe -I... -I... /c xxx.cpp"` 在 bash 里运行后没产生输出文件。

**真因**：bash 提前解析了引号或者 `&` `\` 字符。

**解法**：写 `.bat` 文件，用 cmd 调用：`cmd.exe /c D:\path\to\build_test.bat`。bat 里所有命令一行写完不用转义。

## Polling / 时间管理类

### 11. 4 分钟 polling 阻塞前台

**症状**：训练后台跑 2M 步 ~45 min，每 4 分钟 poll 一次但中间被阻塞等不能做别的。

**真因**：`get_output(timeout=200000)` 阻塞整个 turn，没法并行。

**解法**：
- `get_output(timeout=2000)` 短查 2 秒，看到几行新输出就退出
- 间歇期做完全独立的工作（写文档/工具/分析脚本）
- 这是 AGENTS.md 的"token 缓存 5 分钟"规则真正要的方式
