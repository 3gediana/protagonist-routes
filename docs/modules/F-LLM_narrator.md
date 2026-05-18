# F-LLM · LLM 实时解说员

> 答辩 surprise。让 AI 现场解说 AI。

- **难度**：🟡 中
- **确定性档**：🟢 高（接 API 而已）
- **前置条件**：M5 完美完成
- **状态**：📋 待开发

---

## 一句话

接 OpenAI 兼容 API（你的本地部署模型也行），让 LLM 订阅 EventBus 关键事件，实时生成中文解说，配合演示。

---

## 你会看到什么效果

### 完美版
演示中，屏幕侧边栏滚动显示解说：
> "第 47 代。我观察到 Species #3 开始分化出两个亚种：一个偏好山顶，一个偏好平原。"
> "Species #5 灭绝了，最后存活的成员是 Agent #142，它的拓扑有 23 个连接。"
> "刚才有 5 个 agent 围绕一个重型食物聚集——这是协同行为的雏形。"

评委哇一下。

### 标准版
LLM 输出基本流畅，偶尔重复或废话。

### 降级版
延迟太大或调用失败 → 切换到预设解说脚本，按事件类型查找模板。

---

## 实现要点

### 事件聚合器

```
src/runtime/narrator/
├── EventAggregator.cpp        每 N 秒收集一批事件，提取关键信息
├── PromptBuilder.cpp          组装成 LLM 友好的 prompt
└── LLMClient.cpp              调用 API
```

### Prompt 设计

```
你是一个数字生态学家，正在解说一个进化模拟实验。
当前世界状态：第 {generation} 代，存活 {n} 个个体，{species_count} 个物种。
最近 30 秒发生了：
- 5 次 AgentDied 事件
- 2 次 SpeciesExtinct: Species #3, Species #7
- 1 次 SpeciesFormed: Species #12 (parent: Species #5)
- 14 次 AgentEmittedSignal

请用 1-2 句话解说当前正在发生什么。语气专业但有趣，不超过 50 字。
不要使用废话开头如"现在让我们来看看"、"非常有趣的是"。直接陈述。
```

### API 客户端

支持 OpenAI 兼容协议（OpenAI / Anthropic / 本地 vLLM / Ollama 等都兼容）：
```toml
[F-LLM]
endpoint = "http://localhost:11434/v1/chat/completions"
model = "qwen2.5:7b"
api_key = ""              # 本地不需要
narration_interval = 5     # 每 5 秒一条
language = "zh"            # 跟随 i18n 切换
max_tokens = 80
```

### 兜底机制

API 调用失败时切换到模板脚本：
```cpp
class FallbackNarrator {
    std::string narrateExtinction(int species_id) {
        return T("narrator.species_extinct").replace("{id}", species_id);
    }
    // ... 各种事件的预设模板
};
```

模板的 i18n 资源：
```toml
[narrator.species_extinct.zh]
"Species #{id} 灭绝了。"
"Species #{id} 离开了进化舞台。"
"经过 {gens} 代，Species #{id} 没能延续下去。"
# 随机选一条
```

### UI 集成

侧边栏滚动 panel：
- 时间戳 + 解说文本
- 自动滚动到最新
- 评委可暂停滚动看历史

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] API 调用工作
- [ ] 解说文本能显示

### 完整 ✅✅⛳
- [ ] 事件聚合合理（不轰炸 API）
- [ ] 中英文切换跟随 i18n
- [ ] API 失败时降级到模板

### 完美 ✅✅✅
- [ ] 跑一个完整演示流程，解说自然流畅
- [ ] git tag `module-F-LLM-done`

---

## 🧠 你的补充点

| 项 | 决定什么 |
|---|---|
| **后端模型选择** | 本地 qwen2.5-7b（你说的 8G 占用就是它）/ OpenAI / Claude API |
| **解说风格** | 专业 / 幽默 / 戏剧化 |
| **解说频率** | 太密会废话连篇 |

---

## ⚠️ 易踩坑

1. **API 延迟**：本地 7B 在 5060 上首 token 约 200ms，整句约 1-2 秒。**异步调用 + 不阻塞主循环**
2. **重复废话**：在 prompt 里 few-shot 几个好范例，禁止套话
3. **隐私 / 网络依赖**：演示现场无网络时只能用本地模型。**演示前测好**

---

## 答辩话术

> "答辩亮点：不只是我自己讲，**模拟里跑的是数字生命，旁边讲解的是 AI 解说员**——它在实时分析这个生态系统。我没有写一行规则告诉它'看到灭绝就这样讲'，它根据事件流自己组织语言。"
