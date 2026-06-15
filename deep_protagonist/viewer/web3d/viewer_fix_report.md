# DP 3D Viewer — locator ring / worn cape / sitting grounding

Test world: seed 3000000, protagonist standing on a real forest hillside (relief world,
77 m max relief). Trees layer hidden, tall beacon line hidden, 3/4 follow camera.

## Test 1 — Locator ring conforms to terrain (PASS)
The ring is an independent mesh whose vertices are re-projected onto the heightfield each
frame (bilinear sample) and drawn with `depthWrite:false` + high `renderOrder`, so on a
slope it reads as a clean tilted ellipse lying on the ground instead of a flat disc that
gets buried by the hill.

![ring conforming on slope](/home/ubuntu/screenshots/screenshot_8ec626dea3214203b94cf53ce085fdfc.png)

## Test 2 — Agent grounded when sitting (PASS)
Grounding now samples the lowest of `footL`/`footR` world Y each frame and shifts the model
so the actual foot rests on the terrain. Previously a standing-root assumption made
sitting/fishing poses float. Frame 71 (Drinking / Fishing) — feet planted inside the ring,
rod prop in hand, no float.

![fishing grounded](/home/ubuntu/screenshots/screenshot_d272e5acdc4249c5bdf3e9eada602ea8.png)

## Test 3 — Cape worn on the body, not a frustum (PASS)
The cape is anchored to the **Torso bone** every frame (position snapped to the bone's
world position) and its profile is authored in the torso's local frame (collar at the neck,
hem at the hips). Frame 335 (Wearing clothes) — the garment drapes neck→hips over the body,
legs visible below, no barrel/棱台 placeholder.

![wearing clothes standing](/home/ubuntu/screenshots/screenshot_a36941e0e5374cd6a2bbb1a3f4164523.png)

## Test 4 — Cape stays worn when the body sits (PASS)
Because the cape follows the torso bone, when the body lowers into a seated pose the cape
lowers with it and drapes the lap — it no longer detaches and floats behind the agent.
Frame 466 (Sleeping, seated) — cape stays on the torso/lap, body grounded inside the ring.

![sleeping seated with cape](/home/ubuntu/screenshots/screenshot_b11bddcb808e49199ad682678ceccd8e.png)

## Test 5 — Cape follows the body during actions (PASS)
Frame 362 (Planting seed) — hoe prop raised, torso leaning; the cape moves with the torso
and stays worn throughout the clothed action sequence (plant → water → feed → tend → sleep).

![planting with cape and hoe](/home/ubuntu/screenshots/screenshot_c60e73add8f2428b91a3956f4b75d3e9.png)

---

## 2026-06-02 — 拟真对齐 PE（地形/水/草/洞穴）现状

目标：把 DP viewer 的可视化拟真拉到 PE 同档（按生态磕巴地形 / 不规则水 / 真草 / 洞穴）。

- **地形（DONE）**：斑驳噪声色 + 山谷压暗 + 岩石/雪顶覆于真实起伏之上；按生态调磕巴幅度（沙漠/水域平滑、山地嶙峋、森林中等）。
- **草（DONE）**：实例化草丛 14k→34k，密度对齐 PE。
- **水（DONE）**：不规则岸线 + 盆地开挖（不再薄圆盘/穿模/飘空）。
- **洞穴（未完成，已暂停）**：
  - 数据：`world.json` 6 个椭圆足迹（半径约 15×10m）。
  - 失败方案：散落 boulder（散架感）；半球 dome+隧道（被否：像蒙古包、平地冒大包、6 个雷同、洞口黑管子里露石头）。
  - 正确方向（待做）：移植 PE 的 cell 化小灰岩聚簇 + 边缘暗黑洞口（`pe_web3d/src/main.js` L692–725），不抬大 knoll、不做 dome。
  - `raiseCaveHills()`（`src/main.js` L131–149）抬丘过高，移植时应压低/去掉。
- **待清理**：`window.__view` / `window.__noFollow` 调试代码。

完整项目现状见 `/home/ubuntu/PROJECT_STATUS.md`。训练（DP s5_v3 + PE 旧世界保活 seed）已于本日**全部停止**。
