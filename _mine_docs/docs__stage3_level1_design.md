# Stage3 Level-1 预设计 — 造衣/御寒单层（oracle 决策树升级）

> 依据 decision_round_8 addendum（用户选 A：扩 ScriptedSettler 决策树 + DAgger，逐层升级，"理解了才扩"）。
> 本文件=预设计，**不实现、不开训**，等 bc_v10 过门后再动。新行为只走 oracle demo + DAgger，**绝不加直接奖励**（D-064 红线）。

## 1. 机制已存在但被低估（关键发现）
- 衣物机制**已在 sim 中可用且有意义**：
  - `VitalSystem.cpp`：夜晚/山地 `temp -= cold_rate·dt·(1−resist)`；mountain+night ×1.5；temp≤0 冻死。
  - `Wardrobe.cpp::cold_resist()`：GrassDress=0.10、FurCloak=0.30。→ 穿草裙慢冻 10%、穿皮袍慢冻 30%。
  - obs 已暴露：inv_dress(134)/inv_cloak(135)/worn_none(137)；动作 craft_grass_dress/craft_fur_cloak/wear_clothes 已在 obs 458-460。
- oracle Layer 6 **已实现**造衣：6a 草裙(worn_none&grass≥3)、6b 穿、6c 皮袍(fur≥2)、6d 矛。
- **问题**：Layer 6 只在「已 at_home + 穿过 Layers1-5 + 恰好有 ≥3 余草」才触发 → 造衣 demo 极稀疏 → clone r_craft≈0、几乎从不穿衣。御寒目前全靠"冻了冲回家"(Layer1c)被动应对。

## 2. Level-1 目标行为
让 oracle（进而 clone）在**寒冷世界（山地/夜冷）主动备衣御寒**：未着装时优先凑 3 草→造草裙→穿，再继续定居。
- 直接收益：山地/夜 temp 漂移降 10%，削"会建房但夜里冻死"的 night-terrain 脆性簇（D-110 定位的主因之一）。
- 不引入 attack（皮袍需 fur=狩猎，留 Level-2）；只用草（建造本就采草，材料零额外风险）。

## 3. 拟改动（最小、单层）——待 bc_v10 过门后实现
新增 **Layer 3.5 WARMTH-PREP**（插在 Layer3 vital-maint 之后、Layer4 construction 之前）：
```
// 仅在"寒冷世界且未着装"时触发，避免在温和世界抢建造（防跷跷板）
bool cold_world = mountain || (is_night) || (temp_v < 0.55f);   // 待定阈值, smoke 调
if (worn_none && cold_world && warmth_cooldown==0) {
    // 已有成衣在 inv → 穿
    if (inv_dress>0.005f || inv_cloak>0.005f) { a.wear_clothes=true; warmth_cooldown=8; return a; }
    // 够草 → 造草裙
    if (inv_g >= 3/20) { a.craft_grass_dress=true; warmth_cooldown=8; return a; }
    // 缺草 + 近处有草(≤12m, 不远漂) → 采
    if (find grass within 12m) { steer; if(close) a.collect=true; return a; }
    // 缺草且周围无草 → 不强求, 落到下层正常定居(下个白天再补)
}
```
关键护栏：
- **优先级低于** Emergency(1)/Survival(2)/Vital(3)，**不抢救命**；高于 construction 仅为"先穿再建"，但用 `cold_world` 门控 → 温和世界根本不触发 → camp/建造轴不动。
- 草必须 ≤12m 可见才去采（沿用 Layer7 不远漂纪律）→ 不会为造衣长途跋涉误建造。
- `warmth_cooldown` 防 spam；着装后 worn_none=false → 本层自动熄火。
- **暂不碰皮袍/狩猎**（fur 来自动物=Level-2）。

## 4. 验收门（沿用双门铁律）
- smoke：oracle settler 跑几局确认 craft_grass_dress/wear token 在山地世界稳定出现、loss 单调、行为合理（不卡造衣不建房）。
- DAgger 采样：仅全新冷世界 seed（2200100+ 之外的新段），**绝不碰**评测 bank(700k-1200k/1600k-1800k/1900k-2100k)。
- 全量 bc_v11：
  - 真理解门：全新未训冷 bank 上 worn 率↑、夜 temp 死↓、且无断崖。
  - 多轴不退：camp nShel=1.00、狼存活、夜宿、**attack 全程 0**、双/三全新 bank 中位不退。任一已赢轴退 → 回退 bc_v10。
- 每层 commit（`deep_protagonist:` 前缀）+ 决策记录 + 冠军 ckpt 备份。

## 5. 待实现时复核项
- 确认 `cold_resist` 确实由 Wardrobe.worn() 接入 VitalSystem.update（plumbing）。
- 确认 craft_grass_dress 在 Environment.cpp:809 真扣草/产出 dress、wear 真生效。
- 阈值（cold_world 的 temp、12m 采草半径、cooldown）smoke 时标定。
