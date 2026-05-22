# Protagonist Ecology · 开发任务计划

> 位置：`routes/protagonist_ecology/docx/DEVELOPMENT_TASK_PLAN.md`
>
> 作用：这份文档只管任务，不讲大段设计。开发顺序与理由见 `DEVELOPMENT_EXECUTION_GUIDE.md`。
>
> 更新时间：2026-05-22
>
> 规则：
> - 做完一个就打 `[x]`
> - 正在做用 `[~]`
> - 明确延期用 `[-]`
> - 阻塞用 `[!]`
> - 不在这里记宏大愿景，只记可执行事项

---

## A. 当前主线：Memory V2 -> Credit -> Communication

### A1. Memory V2 收口

- [x] 为 `water_return` 重写 spatial 证据链：cascade 解耦后 v4-v7 四轮均 `no_spatial=-78%~-100%`、`no_goal=-64%~-100%`，两通道独立，旧 `-44%` 是 cascade 假阳性（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 7.3）
- [~] 为 `signal_response` 重写完整证据链：v3-v7 五轮 `signal_response_events` 全为 0；perception dim 18-21 已专门化为 `peer_signal_heard` readback，TraceRecorder 已加 4 列归因，但根因诊断为 reward 全局比例失衡（§ 7.4），不是单点缺口
- [x] 为 `signal_response` 增加 perception-driven shaping reward，避免只有进 halo 才有梯度（已加 `signal_response_dense_reward` 字段 + dense gradient `(1-dist/companion_radius)*dt`，但 v4-v7 数据显示量级仍不够撬动 NEAT 30/60 代探索；见 § 7.5 后续放大方向）
- [x] 为 `signal_response` 增加 trace 字段，单独记录 listener 的 signal 强度、朝向变化与响应轨迹（`agent_trace.csv` 新增 `recent_signal_dx/dy/salience/age_seconds` 4 列）
- [x] 为 `water_return / signal_response / worksite_return` 三个任务输出统一对照表，明确 primary metric 和 secondary metrics（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 1）
- [x] 把 `MemoryCognitionPerception` 当前 44 维重新审计一遍：标出哪些维度真被任务使用，哪些只是挂着（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 2，结论：OK 9 / DEAD 14 / WEAK 5 / UNKNOWN-DNC 8 / 混合 entangled 2）
- [x] 为 `Spatial / Episodic / Social / Goal` 四类 memory 分别补一条“最小有效 query”清单（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 3）

### A2. Credit / EventLineage 收口

- [ ] 把建造 credit、围猎 credit、signal credit 统一整理成同一套 route-local event lineage 口径
- [ ] 为 `BuildCompleted / AgentDied / signal_response` 三条链输出统一 share-based reward 日志
- [ ] 新增 `contribution fairness` 汇总指标，区分真实贡献者与路过者
- [ ] 为 signal 线补 `producer / listener / responder` 三种角色贡献统计
- [ ] 补 route 测试，覆盖“路过者不吃大分”“listener 真响应后才记 credit”

### A3. Communication semantics 收口

- [ ] 新增 `communication_metrics.csv`
- [ ] 新增 `signal disabled / listener blind / one-channel only / signal noise increased` 四组 ablation
- [ ] 实现 `scout-worker` 最小任务
- [ ] 实现 `danger warning` 最小任务
- [ ] 实现 `build request` 最小任务
- [ ] 用 trace + ablation 证明至少一条通信任务满足“关闭信号后明显下降”
- [ ] 为 communication 线写单独结果说明，避免再把 goal 变化误读成 signal 语义成立

---

## B. 第二主线：鲜明物种生态角色（R-005）

### B1. 角色定义

- [ ] 固定四个第一批角色：独居顶级掠食者 / 群猎者 / 食腐者 / 协作人类系
- [ ] 为每个角色写一页 route-local 证据链模板：食性、社会性、空间性、风险偏好、资源利用
- [ ] 审计当前 `BootstrapPopulation` 中各 archetype 与目标角色的差距

### B2. 生态位落地

- [ ] 独居顶级掠食者：加入 territory / home range / 同类排斥规则
- [ ] 群猎者：加入 shared target / spacing / surround 规则
- [ ] 食腐者：加入 carcass-first / producer-scrounger / 高空巡航规则
- [ ] 人类系：加入 signal-assisted group converge / assist / hunt coordination

### B3. 验收与展示

- [ ] 为四类角色分别补 1 组行为指标
- [ ] 为四类角色分别补 1 个短 replay / trace 样例
- [ ] 确认用户只看屏幕几分钟就能区分四类角色

---

## C. 第三主线：生命周期与繁殖（R-006）

### C1. 生命周期状态

- [ ] 给活体补 `juvenile / adult / elder` 阶段状态
- [ ] 把年龄接入 movement / combat / reproduction / recovery
- [ ] 引入 natural death，不再只有外伤或饥渴死

### C2. 尸体与疾病闭环

- [ ] 死亡后留下 corpse，而不是直接消失
- [ ] 尸体可被食腐者消费
- [ ] 尸体可作为 disease / contamination source

### C3. 繁殖闭环

- [ ] 设计最小 mating condition：年龄、距离、状态、冷却、物种兼容性
- [ ] 背景物种补双亲融合路径
- [ ] protagonist 路线补 parent ids / generation / lineage 持久记录
- [ ] 后代入世，并参与后续竞争与演化

### C4. 验收

- [ ] 用结果证明群体结构会因老病死改变
- [ ] 用结果证明后代不是单亲复制
- [ ] 用结果证明谱系变化会影响行为或分布

---

## D. 第四主线：自动晋升闭环（R-007）

### D1. 数据对象

- [ ] 定义 `Incident`
- [ ] 定义 `Challenge`
- [ ] 定义 `Lineage`
- [ ] 定义 `PromotionDecision`

### D2. 回流与试运行

- [ ] 生产问题自动落 incident archive
- [ ] incident 自动映射成 challenge task
- [ ] challenger lineage 自动进入试运行世界
- [ ] 支持 canary rollout
- [ ] 支持自动 rollback

### D3. 学习型控制器（后置）

- [ ] scheduler 只负责 task prioritization，不取代硬门槛
- [ ] promotion scorer 只做风险排序，不直接决定上线
- [ ] archive controller 只做保留/再孵化建议，不覆盖 baseline 保护

---

## E. 第五主线：自长技能与开放式增殖（R-008）

### E1. Quality Diversity

- [ ] 把 `MapElitesArchive` 从离线报告接进在线 reproduction
- [ ] 明确 behavior descriptor
- [ ] 把 archive coverage 纳入训练报告

### E2. World Model

- [ ] 先用现有 trace dataset builder 训练最小 predictor
- [ ] 产出 world model eval report
- [ ] 证明 latent feature 对 eval 有增益

### E3. Goal System

- [ ] 在 rule-based goal loop 稳定后，再设计 manager/worker 双头
- [ ] 补 goal progress metrics
- [ ] 做 goal ablation

### E4. 自长机制放大器

- [ ] 确认哪些能力适合通过 richer environment 自动长出，而不是人工先写死
- [ ] 为 R-008 设计 speciation / archive / diversity 保留策略
- [ ] 再评估是否进入 POET / environment genome

---

## F. 展示与正式长训（最后做）

- [-] 正式 overnight / multi-day 长训（在 A-D 未收口前不启动）
- [-] 3D replay panel（在机制链未收口前不启动）
- [-] live production world 正式上线（在自动晋升/回滚未完成前不启动）

---

## G. 文档与清理

- [x] 新建 `DEVELOPMENT_EXECUTION_GUIDE.md`
- [x] 新建 `DEVELOPMENT_TASK_PLAN.md`
- [x] 更新 `AGENTS.md` 的 route 文档导航，加入两份新文档并降级旧模块路线文档
- [x] 更新 route `PROGRESS.md` 顶部状态与下一步建议，改为引用新开发文档
- [x] 更新顶层 `PROGRESS.md` 最近会话摘要与下一步建议
- [x] 清理 `docx/` 下明显垃圾文件：`NUL`、异常字符空文件、误生成的空目录/脏文件
- [x] 将 `HANDOFF_NEXT_AI_2026-05-21.md` 归档或删除，避免继续作为主入口
- [x] 将 `MODULE_DEVELOPMENT_PLAN.md` 从主导航中降级为历史参考，避免与新路线抢口径
