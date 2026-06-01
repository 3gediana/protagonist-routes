# protagonist_ecology · 主角生态路线

> 这个 route 是当前项目的主开发线。一句话目标：**让 24 个 protagonist 在
> 600×600 的拟真生态里和 8 个背景物种共生，通过 NEAT+DNC 进化出建造、繁殖、
> 合作、抗灾的复杂行为，并可视化为 3D 场景**。

最后更新：2026-05-24

---

## 1 · Quick navigation

| 你想做 | 看这里 |
|---|---|
| 第一次进项目，搞清楚状态 | `docx/PROGRESS.md` + `docx/STATUS.md` |
| 知道未来要做什么、按蓝图推进 | `docx/FINAL_BLUEPRINT.md`（项目宪法） |
| Layer 1-8 各阶段设计 | `docx/supporting/LAYER{5,6,7,8}_*.md` |
| 跨会话交接当前进度 | `../../tmp/HANDOFF_LAYER5_TO_8.md` |
| 跑训练 | `cmd.exe /c "build_ninja_cuda\bin\neural-eco-protagonist.exe --config tmp\layer1\layer5.toml > tmp\layer1\run.log 2>&1"` |
| 看效果 (viewer) | `uv run viewer/python_viewer/view_frames.py <viewer_frames.jsonl>` |

---

## 2 · 当前完成度 (2026-05-24)

按 `docx/FINAL_BLUEPRINT.md` 的 Layer 1-8 依赖树：

| Layer | 状态 | 主要数据 | Commit |
|---|---|---|---|
| L1 体温+600×600 | ✅ | alive 0.45→0.79 | a0669f6, 7a098a6 |
| L2 6 种天气 | ✅ | avg_fit +167% | 202186b |
| L3 9 种物种 (含 Rabbit) | ✅ | avg_fit 6685, best 12390 | e3d2b8b |
| L4 生命周期+繁殖+谱系 | ✅ | 4 in-world births | da86050, 1377352 |
| L5 R1 建筑类型+reward shift | ✅ | builds 偶发涌现 | 3501070, da86050 |
| L5 R2 building distribution + Storage overflow | ✅ | 30-gen avg 4543 (6.2× v17) | 5824eb7 |
| L5 R3 Lookout 感知加成 | ✅ | 3-gen 跑通 | b8326ee |
| L6 R1 WorldRole borderland/main | ✅ (accessor only) | 字符串扩展 | b53a4e8 |
| L6 R2 MigrationScheduler | ⬜ | deferred | LAYER6_SPEC.md |
| L7 R1 viewer_frames.jsonl | ✅ | 200 frames/gen JSON | 456e09b |
| L7 R2 Python viewer | ✅ | pygame MVP | 617d5df |
| L7 R3 Godot 4 viewer | ⬜ | deferred | LAYER7_SPEC.md |
| L8 R1 Lamarckian Hebbian | ✅ | toml gate 跑通 | 4d57714 |
| L8 R2/R3/R4 DIAYN/WM/Comm | ⬜ | deferred | LAYER8_SPEC.md |

---

## 3 · 目录速查

```
routes/protagonist_ecology/
├── README.md                  ← 本文件
├── CMakeLists.txt             ← build 入口
├── main.cpp                   ← runtime 入口
├── brain/                     ← ProtagonistBrain (DNC) + NeatBrain + GPU
├── capability/                ← Chop/Craft/Hunt/Throw/Thermal/...
├── perception/                ← 自感知 + 周围感知 (15 个 perception)
├── runtime/                   ← Bootstrap/Evolution/Eco config/Trace
├── world/                     ← 16 个 layer (Base/Tree/Water/Worksite/Population/...)
├── viewer/                    ← Layer 7 viewer (python_viewer + godot_demo)
├── data/                      ← Kenney 资源 (动物/树/帐篷)
└── docx/
    ├── PROGRESS.md            ← route 进度
    ├── STATUS.md              ← route 交接现状
    ├── FINAL_BLUEPRINT.md     ← 项目宪法
    ├── DEVELOPMENT_TASK_PLAN.md
    └── supporting/
        ├── LAYER5_BUILDING_SPEC.md
        ├── LAYER6_THREE_WORLDS_SPEC.md
        ├── LAYER7_GODOT_VIEWER_SPEC.md
        ├── LAYER8_ADVANCED_MECHANISMS_SPEC.md
        ├── PRODUCTION_WORLD_LIFECYCLE_SPEC.md (17 invariants)
        └── ... (其他 spec)
```

---

## 4 · 重要决策 (FINAL_BLUEPRINT.md)

1. 600×600 世界 (vs 200×200 baseline)，9 物种 ecology cycle
2. R-001 完整生理 (体温/口渴/饥饿/疲劳/恐惧/孤独)
3. R-006 lifecycle (juvenile/adult/elder + 5 死亡分类)
4. **主世界永不 reset** (PRODUCTION_WORLD_LIFECYCLE 17 invariants)
5. 5 种建筑 (Wall/Shelter/Storage/Lookout + 未来 Campfire)
6. 全套算法 (NEAT+CTRNN+DNC+HRL+MAP-Elites+HER+Lamarckian)
7. Godot 4 + Kenney + Mixamo + AI procedural 视觉

---

## 5 · 给下次 AI 助手

请先读：
1. `../../AGENTS.md`（全仓 AI 协作规则）
2. 本 README
3. `docx/PROGRESS.md` + `docx/STATUS.md`
4. `docx/FINAL_BLUEPRINT.md`
5. 选你要做的 Layer → 读对应 `docx/supporting/LAYER{N}_*.md`

下一步候选（按 user value 优先级）：
- **L7 R3 Godot viewer**: user vision (Mongolian-yurt) 最直接
- **L6 R2 MigrationScheduler**: 主世界永不 reset 落地
- **L8 R2 DIAYN**: protagonist 风格分化可视化

每个都有专门 spec doc 详细描述工作量和接口。

---

## 6 · 常见陷阱

- **exec timeout ≤ 4min**（shell 缓存 5min，留 1min buffer）。长任务用 `cmd.exe /c "start /B ..."` 后台启动
- **Re-build 前先 kill exe 占用进程**：`taskkill /F /IM neural-eco-protagonist.exe`
- **toml table 不能重复**（e.g. 2 个 `[trace]` 会 parse error）
- **WSL stdout 编码**：用 `cmd.exe /c "... > file 2>&1"` 同步等待 + 落盘
- **Layer 4 reproduction 慢**（每 birth ~100ms NeatBrain reconstruction）：调 `max_spawns_per_tick=1` + `reproduction_check_interval=300`
