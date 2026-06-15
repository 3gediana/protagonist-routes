# Layer 7 R2 · Python Viewer (MVP)

最小可工作 viewer for `viewer_frames.jsonl`（commit 456e09b 引入）。

## 用法

```bash
# 启用 viewer_frames_enabled=true 跑训练
./build_ninja_cuda/bin/neural-eco-protagonist.exe --config tmp/layer1/layer5.toml

# 找到产出的 viewer_frames.jsonl
find data/runs/layer1/ -name viewer_frames.jsonl | tail -1
# e.g. data/runs/layer1/20260524-110409/trace/generation_0/episode_0/viewer_frames.jsonl

# 运行 viewer (uv 自动安装 pygame)
uv run routes/protagonist_ecology/viewer/python_viewer/view_frames.py \
    data/runs/layer1/20260524-110409/trace/generation_0/episode_0/viewer_frames.jsonl
```

## 控制

| 按键 | 行为 |
|---|---|
| Space | 暂停/继续 |
| ←/→ | -10/+10 frame |
| R | 重置到 frame 0 |
| +/- | 速度 ×1.5 / ÷1.5 |

## 视觉编码

### Agent 颜色 (archetype_tag)
- 0 绿 = Protagonist
- 1 黄 = LargeHerbivore (cow)
- 2 棕 = SmallForager
- 3 红 = ApexPredator (tiger)
- 4 灰 = PackHunter (wolf)
- 5 橙 = AmbushPredator (leopard)
- 6 青 = Scavenger
- 7 紫 = Omnivore (bear)
- 8 白 = Rabbit

Protagonist 半径=5，背景物种半径=3。
Juvenile 有白圈，Elder 有灰圈。

### 建筑颜色 (building_type)
- 0 灰方 = Wall
- 1 棕方 = Shelter (yurt)
- 2 深棕 = Storage
- 3 蓝方 = Lookout

完成 = 实心，建造中 = 空心。绿色 progress bar 显示建造进度。

### 天气背景 (weather)
- 0 浅蓝 = Clear
- 1 灰 = Cloudy
- 2 深蓝 = Rain
- 3 暗蓝 = Storm
- 4 黄灰 = HighWind
- 5 黑蓝 = Typhoon

## 下一步 (R3 - Godot)

参考 `docx/supporting/LAYER7_GODOT_VIEWER_SPEC.md`。Godot 4 + Kenney 模型给 user 完整 vision (Mongolian-yurt visual + 高光场景)。
