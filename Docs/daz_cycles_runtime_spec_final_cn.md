# DAZ 轻量级 Cycles Runtime —— 实现规范（含模型路由）

## 本地开发环境与版本基线（用户补充，2026-09-21 更新）

- 用户要求先以中文提交 006，再启动 **007 Formula / ERC / JCM**。006 已提交到本地 Git：`aa8fbc7`。007 完成后必须专项复测 005 Morph 模块，核对此前禁用的 Arms Length、Chest Scale、Eyes Closed、HS Sanny Shy、Flex Quad 等参数是否实际生效；不得仅去掉界面禁用状态。测试仍只在副屏进行，不重复性能回放。

- **007 本阶段实施与工程复测已完成并部署**：G8 上述五项参数已启用；G8.1 使用新版 Eye Blink，保留旧控制器空覆盖。七项工程检查、两代各 40 姿势、独立数学与副屏验证通过，DAZ Studio Golden 仍待完成。详情见 [007 执行报告](../specs/007-erc-jcm/execution-report.md)。下列 005 / 006 授权与能力描述按各自阶段理解。

- 用户已授权启动 **006 Skeleton / Skinning**，并要求同时支持用于摆姿势的 DUF。真实验证目录：`H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female`，共 40 个单帧 `preset_pose`。对选中 Genesis 8 角色应用姿势，验证骨骼层级、权重、蒙皮、恢复与 Morph → Skinning 顺序；继续检查现有 Genesis 8.1 样例的兼容性。详细边界见 [006 规范](../specs/006-skeleton-skinning/proposal.md)。Formula / ERC / JCM 在 007 实现，当前未应用的控制器必须在界面和报告中列明。

- 内容库必须支持多个可自定义根目录，并作为“项目 → 项目设置”持久保存。用户提供的查找顺序为 `H:\g1`、`H:\g3`、`C:\Users\Public\Documents\My DAZ 3D Library`、`C:\Users\xatia\Documents\DAZ 3D\Studio\My Library`。参数发现需包含目录子级、纯公式控制器及子节点别名，保留 DAZ 的 `group` 路径；发现完整度与求值支持程度分别报告，不仅统计带顶点差值的 Morph。相同资源按根目录优先级处理，G8.1 的空覆盖语义不能被依赖扫描绕过。

- 用户已授权进入 **Spec 005 Morph Runtime**，补充长期中文 UI（Explorer、菜单、加载角色、SceneHierarchy、Morph / 变换编辑）与 Genesis 8 服装 FitTo / IK 跟随需求。采用 Qt Widgets 应用外壳与独立 Runtime；FitTo 按蒙皮 / ERC / IK 顺序集成，DBZ 保留为兼容与参考选项。详细设计见 [长期规划](editor_and_genesis8_roadmap_cn.md)。Spec 004 材质 Golden 尚未通过，继续作为独立剩余项。

- 用户于 2026-09-20 确认指定角色与道具静态预览验收通过，代码已以中文提交到本地 Git（`afc7737`，无远端）；于 2026-09-21 明确授权进入 **Spec 004 材质参考与完善**，沿用当前模型。
- 材质参考使用本机 `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe`（5.2.2 LTS）与已安装 DAZ Importer 5.2.0；仅作为独立后台参考工具，产品运行时不依赖 Blender 或该插件。无需额外获取源码。

- 后续真实资产验证采用：
  - 角色：`C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8.1 Basic Female.duf`。
  - 道具：`H:\G3\Environments\Architecture\ARK Modern Modular Cafe\Props\Tabletop\ARK MM Cafe - Food Plate with Fries.duf`。实际文件引用盘子与薯条，未发现汉堡网格。
- **测试 demo 必须在第二块屏幕运行，不占用主屏。** 当前 `DISPLAY1` 为主屏 2560×1440，`DISPLAY2` 为右侧副屏 1920×1080、起点 `(2560,0)`。程序默认 `--monitor 2`；第二屏缺失时明确报错，不回退主屏。
- 用户已确认继续 Spec 002 场景数据层与适配器，并提供上述资产用于 Spec 003 最小加载。沿用已确认模型，不重复发起模型确认；Spec 001 严格 Visible FPS 证明仍保留未完成，不据此继续重复跑分。

- Blender 原始源码目录：`D:\Github\blender`。
- 研发优先基于 **Blender 5.2.2 LTS**；此前补充中的“daz 5.2.2”按本次用户说明明确为 Blender 版本。用户已授权切换到对应分支。
- 版本选择考虑 Diffeomorphic Import DAZ 的 Blender 5.2 兼容性；具体资产的兼容性仍通过 Reference / Golden Test 验证。
- **核心产品诉求优先于具体版本选择。** 可以根据实际源码与构建情况调整技术方案并记录 Design Gate，但不得削弱独立 Runtime、原生 DAZ 资产支持、Cycles Beauty Viewport、Camera-only ≥20 Visible FPS、Morph / Pose 等要求。
- CUDA 已安装：`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9`；已运行 `nvcc --version` 核实为 CUDA 12.9、NVCC 12.9.41。
- OptiX 源码目录：`D:\Github\optix-dev`；实际为 `v9.1.0`，commit `f1f6dd803f3159992d248178f6e09421c6eb8b6d`。后续以实际 checkout 检查 Cycles 兼容性与许可证，不把它记作此前建议的 8.0。

当前研发工作树：`D:\Github\blender-5.2.2`，本地分支 `dazfastviewer-blender-5.2.2`，基于标签 `v5.2.2`，commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`；检查时 `origin/blender-v5.2-release` 也指向该 commit。采用独立工作树是为了保留原目录的工作文件及索引状态；原目录仍为 main / 5.3.0 alpha。新工作树已跳过远端缺失的 Blender LFS 二进制资产，源码检出完整；相关资产若用于后续验证需另行获取。

实际版本、构建边界及许可证证据见 [Spec 001 Design](../specs/001-cycles-static-camera/design.md)，执行进度见 [STATUS](../specs/STATUS.md)。

> 状态：实现基线  
> 目标：构建一个轻量级 DAZ Runtime，支持原生 DAZ 场景、角色、Morph、Pose、材质与高质量路径追踪视口，同时避免依赖 DAZ Studio 或 Iray 作为运行时组件。  
> 推荐开发方式：OpenSpec 风格、规格驱动、分阶段模型路由、测试优先、Golden Validation、性能回归。

---

# 文档使用说明

本文件是项目唯一必要的总体实现规范。Agent 不应假设存在任何更早版本、迁移文档或历史设计说明。

如果本文件与后续实际源码、依赖版本或用户提供的测试资产发生冲突，应优先：
1. 保持本文件定义的产品目标与硬性验收指标；
2. 以用户当前实际 checkout 的源码与实际测试资产为技术事实来源；
3. 在 Design Gate 中记录必要的实现调整；
4. 不得自行推断不存在的历史上下文。

---

# 0. Agent 执行协议与模型路由

本文档预期被一次性提供给 Coding Agent。Agent 不得假设自己能够自动切换模型。

## 0.1 执行原则

项目采用“规格驱动 + 人工确认式模型路由 + 阶段 Gate + Golden Test + 性能回归”。

每当 Agent 即将进入新的 Spec、核心子模块、复杂 Debug 或重大重构时，必须：
1. 判断任务复杂度；
2. 推荐模型；
3. 推荐 Reasoning Effort；
4. 解释原因；
5. 明确任务边界；
6. 暂停代码修改；
7. 请求用户确认当前模型；
8. 用户确认后再开始执行。

禁止未确认模型就进入新的核心模块，也禁止假装已经自动切换模型。

## 0.2 标准模型确认格式

```text
【模型确认】

即将执行：
Spec XXX / 子模块名称

推荐模型：
GPT-6 Astra

推荐推理强度：
High / xhigh

原因：
- ...
- ...
- ...

本次范围：
- ...
- ...

请确认当前模型已切换到推荐模型。
确认后开始实现。
```

## 0.3 模型等级

### Tier A —— GPT-6 Astra

用于总体架构、Cycles 独立集成、GPU/Interop/Presentation、DAZ Runtime 总体语义、ERC/JCM、Skinning/TriAx/DQ 差异、Material Graph 总体映射、复杂性能问题、跨模块重构、许可证边界与多参考实现综合分析。

默认 High；复杂架构与 Debug 用 xhigh；极少数长期无法解决的问题再考虑 max。

### Tier B —— GPT-5.6 Sol

用于已确认架构后的 C++ 实现，包括 DSON Parser、Asset Resolver、Scene Node、Camera、Geometry、Morph Loader、Sparse Morph、FK、IK、FABRIK、PowerPose、Dirty Graph、Scene Delta、Cycles Adapter、UI、Logging、Profiler、Integration Test 与 Golden Test。

推荐 Medium / High。

### Tier C —— Terra / Luna

仅用于低风险、机械性任务，例如 Boilerplate、重复测试、Rename、Formatting、文档同步、简单 Config 和 CI 小调整。

发现未知设计问题必须升级模型。

## 0.4 自动升级规则

使用 Sol / Terra / Luna 时，如出现以下任一情况，必须停止继续猜测并建议升级 Astra：

```text
1. 同一核心 Bug 连续两轮修复失败
2. Golden Test 出现无法解释的差异
3. 需要改变 Runtime Data Model
4. 需要改变线程模型
5. 需要改变 Render Scene IR
6. 需要改变 Cycles Backend 边界
7. 性能结果与预期差异超过约 2 倍
8. DAZ / DSON 语义与已有假设冲突
9. 修改影响三个以上核心模块
10. 涉及 GPU Driver / CUDA / HIP / OptiX / Cycles 边界
11. Material 映射结果与 Blender Reference 明显不一致
12. 当前方案主要依赖猜测而不是证据
```

## 0.5 自动降级规则

Astra 完成 Architecture、Interface、Invariant、Test Plan 与 Acceptance Criteria 后，如果剩余工作主要是边界清晰的实现，应提示切回 GPT-5.6 Sol。

## 0.6 推荐模型分工

| 子系统 / Spec | Design | Implementation | Review |
|---|---|---|---|
| Repo / 总架构 | Astra High | Sol High | Astra |
| Cycles 独立集成 | Astra xhigh | Sol High | Astra |
| Render Scene IR | Astra High | Sol High | Astra |
| GPU Presentation | Astra xhigh | Sol High | Astra |
| DSON Runtime Model | Astra High | Sol High | Astra |
| DUF / DSF Parser | Sol High | Sol | Sol |
| Morph Runtime | Astra High | Sol High | Astra |
| ERC / Formula | Astra xhigh | Sol High | Astra |
| JCM | Astra xhigh | Sol | Astra |
| Skeleton / Skinning | Astra High | Sol High | Astra |
| FK | Sol | Sol | Sol |
| Two-Bone IK | Sol High | Sol | Sol |
| Pinning / Constraint Graph | Astra→Sol | Sol | Astra |
| PowerPose | Sol High | Sol | Sol |
| Material Architecture | Astra xhigh | Sol High | Astra |
| Shader 参数映射 | Sol High | Sol | Astra（复杂项） |
| Blender Reference Harness | Astra High | Sol | Astra |
| Golden Render Diff | Astra High | Sol | Astra |
| 性能 Regression | Sol High | Sol | Astra |
| 无法解释的 Cycles Stall | Astra xhigh | — | Astra |

## 0.7 状态文件

Repo 建议维护：

```text
/specs/STATUS.md
/HANDOFF.md
```

每次切模型前更新，以便新的 Agent 或新模型快速恢复上下文。

---

# 1. 项目定义

## 1.1 产品目标

开发一个独立的轻量级 DAZ Runtime，重点服务于：

```text
DAZ 原生资产
+
静态场景布局
+
高质量路径追踪预览
+
角色 Morph / Pose 调整
```

项目不追求成为完整 DCC。

核心工作流：

> 加载 DAZ 场景与角色资产，以 Cycles 作为主要高质量 Beauty Renderer，提供流畅的静态场景视口浏览，并以较低系统开销支持角色 Morph、FK、IK 与 PowerPose 风格控制。

## 1.2 核心要求

必须支持：
1. 指定 `.duf` 场景；
2. 指定 `.dsf` 模型 / Morph / Rig；
3. DAZ Content Library 路径解析；
4. 指定材质；
5. Cycles Beauty Viewport；
6. Camera Orbit / Pan / Dolly；
7. 静态场景 Camera-only 状态下目标 ≥20 Visible FPS；
8. Transform；
9. Morph；
10. 第三方 Morph；
11. ERC / Formula；
12. JCM；
13. FK；
14. Two-Bone IK；
15. Pinning；
16. PowerPose-like UI；
17. RenderDoc / Nsight 友好的应用层；
18. Golden Test 与性能回归。

---

# 2. 运行时边界

运行时架构：

```text
DAZ Runtime
    ↓
Render Scene IR
    ↓
Cycles Backend
```

产品运行时不依赖：
- DAZ Studio
- Blender GUI

Blender 主要作为 Reference Environment、Validation Environment、Material Comparison Environment 和 Source Reference，而不是必须的运行时宿主。

---

# 3. Blender / Cycles 源码参考原则

## 3.1 Agent 可以直接参考用户本地或 Git 获取的 Blender/Cycles 源码

后续开发时，用户会直接从 Git 获取 Blender / Cycles 对应版本源码。

Agent 被明确允许：
- 阅读用户当前 checkout 的 Blender / Cycles 源码；
- 阅读其构建系统；
- 分析 Cycles 的 Scene / Device / Session / Shader / Geometry / Camera / Output 等实现；
- 根据当前源码分析生命周期、数据流和性能路径；
- 参考 Blender 内部如何驱动 Cycles Interactive Viewport；
- 参考 Camera、Scene、Geometry、Shader 的更新方式；
- 参考 GPU Backend 与 Output Buffer 实现；
- 根据当前源码定位性能与同步瓶颈；
- 基于当前版本源码提出集成方案。

## 3.2 文档禁止绑定具体内部实现

本文档不写死：
- 具体 Blender/Cycles 源码文件路径；
- 具体私有类名；
- 私有内部 API；
- 某个 Blender 版本的固定函数调用链；
- 某个 Git commit 的内部组织方式。

原因是 Blender / Cycles 会持续迭代。

开工时必须：

```text
读取当前 checkout
→ 确认版本
→ 确认实际模块边界
→ 再制定具体实现方案
```

本规范只固定：
- 产品目标
- 架构边界
- 行为 Invariant
- 性能指标
- 测试方法
- 许可证约束

## 3.3 当前源码优先于历史实现细节

如果本文档中的概念描述与用户当前 checkout 的 Blender/Cycles 内部结构存在差异，应以当前源码为准。

但不得因此削弱本项目产品要求、性能指标和架构 Invariant。

---

# 4. 第三方参考生态

## 4.1 Blender / Cycles

用途：
- 核心 Path Tracing Renderer
- Interactive Viewport 参考
- GPU Device Backend 参考
- Shader Graph 参考
- Scene / Camera / Geometry / Light Backend 参考
- 性能与调度实现参考

产品优先考虑独立 C++ Application + Cycles，而不是把 Blender GUI 作为产品运行时。最终方案由 Phase 0 对实际源码的研究决定。

## 4.2 DAZ 官方 DazToBlender

用途：
- DAZ → Blender 资产映射参考
- Material Mapping 参考
- Rig / Object / Metadata 转换参考
- 官方 Bridge 行为验证

Agent 在复用源码前必须确认实际 checkout 的许可证与 NOTICE。

## 4.3 Diffeomorphic Import DAZ

用途：
- `.duf/.dsf` 行为研究
- Morph / Rig / JCM / ERC 参考
- Blender 导入结果验证
- 长尾 DAZ Asset Compatibility 参考

若当前版本采用 GPL 类许可证，而本项目保持非 GPL 商业闭源形态，则允许研究行为、阅读理解、比较输入输出、运行 Reference Test 和独立重新实现，但禁止直接复制 GPL 实现到闭源核心代码。

---

# 5. 第三方源码使用规则

Agent 准备复用第三方代码前，必须报告：

```text
Repository:
Revision / Tag / Commit:
File or Module:
License:
Copyright:
Planned Use:
是否复制源码:
是否修改:
是否进入发布二进制:
```

GPL、AGPL、未知 License 或混合 License 必须暂停并要求用户确认。

---

# 6. 参考验证链

## Reference A —— DAZ Studio

用于 Morph、Pose、ERC、JCM、Skinning 和 Parameter correctness。

## Reference B —— Blender + DazToBlender

用于官方 DAZ → Blender 转换结果、Material Mapping、Rig Conversion 和 Cycles 外观对照。

## Reference C —— Blender + Diffeomorphic

用于原生 DUF/DSF Import、Morph/Rig 边界情况和第三方资产参考。

## Reference D —— Blender / Cycles Source

用于 Cycles integration、Interactive Viewport、Camera Update、Scene Mutation、GPU Backend、Render Buffer 和性能路径研究。

## Implementation E —— 本项目

---

# 7. Render Golden Pipeline

推荐：

```text
Path A:
DAZ Asset
→ Blender Reference Import
→ Cycles
→ Reference Image

Path B:
DAZ Asset
→ Our Runtime
→ Render Scene IR
→ Cycles
→ Our Image
```

由于两条路径最终都使用 Cycles，可以比较 Pixel Diff、RMSE、SSIM、Per-channel Error 和 Histogram Difference。

---

# 8. 高层架构

```text
DAZ Content Library
        ↓
Asset Resolver
        ↓
DSON Loader
        ↓
Immutable Asset DB
        ↓
DAZ Runtime Scene
        ↓
Pose / Morph / Material Runtime
        ↓
Render Scene IR
        ↓
Cycles Adapter
        ↓
Cycles Backend
        ↓
GPU Output
        ↓
Presenter + Overlay
```

---

# 9. Render Scene IR

DAZ Runtime 不直接依赖 Cycles 内部数据结构。

建议保留逻辑对象：
- RenderScene
- RenderMesh
- RenderInstance
- RenderMaterial
- RenderTexture
- RenderCamera
- RenderLight
- RenderEnvironment
- RenderDelta

具体 C++ 类型、字段与内存布局必须在实际开发时结合当前 Cycles 源码和性能需求确定。

---

# 10. 推荐模块结构

```text
/src
  /core
  /daz_asset
  /daz_scene
  /daz_eval
  /pose
  /material
  /render_ir
  /cycles
  /viewport
  /validation
  /ui

/tests
  /unit
  /integration
  /golden
  /render_golden
  /performance
```

具体文件名不在本规范中固定。

---

# 11. 线程模型

建议逻辑层划分：
- UI Thread
- Evaluation Job System
- Render Scene Sync
- Cycles Render / Control
- Presentation

具体线程数量和所有权必须结合当前 Cycles 源码与 Profiler 决定。

硬性要求：

```text
UI 不得同步等待完整 Render Frame。
```

---

# 12. 核心性能原则

## 12.1 Camera-only Fast Path

静态场景中：

```text
Mouse
→ Camera State
→ Render IR Camera
→ Cycles Camera
→ Interactive Render
→ Latest Result
```

不得触发：
- DSON Parse
- Morph Evaluation
- Skinning
- Material Translation
- Texture Reload
- Mesh Rebuild
- Shader Graph Rebuild

## 12.2 Latest-State-Wins

Camera input 允许丢弃历史状态，禁止积压。

## 12.3 Load-time 优先

Material Conversion、Texture Processing、Morph Scan、Formula Compile、Shader Graph Build、Static Scene Build 和 Cache Build 应尽量前置到加载阶段。

---

# 13. 性能目标

硬指标：

```text
目标中型静态场景
+
原生 Viewport Resolution
+
仅移动 Camera
+
Cycles Beauty Viewport
=
>=20 Visible FPS
```

Camera 停止后继续 Progressive Refinement。

---

# 14. Cycles 集成策略

本规范不写死：
- Session API 名称
- Device API 名称
- Scene API 名称
- Camera Update 调用
- GPU Buffer API
- 当前 OptiX/CUDA/HIP 内部实现细节

Phase 0 开始时，Agent 必须先读取当前 Blender/Cycles checkout，确认真实可复用边界、Build System、Device/Scene/Session/Output 生命周期，并把结果写入 Spec 001 Design。

---

# 15. Device 策略

第一阶段优先支持目标用户机器实际使用的 NVIDIA GPU 路径。

具体 Backend 选择必须由当前 Cycles 源码和 Benchmark 决定，不允许根据历史版本经验写死。

---

# 16. DSON / DAZ 解析

支持：
- `.duf`
- `.dsf`
- Texture Reference
- Content Root
- URI Fragment

首版只保证测试资产需要的 Feature。

Raw JSON DOM 禁止直接成为 Runtime Data Model。

---

# 17. Property / Formula / ERC

建立 Property Graph，支持 Channel、Formula、Dependency、Dirty Propagation、Cycle Detection 和 Selective Re-evaluation。

---

# 18. Morph

使用 Sparse Delta，运行时维护 Active Morph Set，只处理非零 Morph。

---

# 19. JCM

JCM 进入统一 Evaluation Graph：

```text
Bone Rotation
→ Formula / ERC
→ JCM Weight
→ Morph Evaluation
→ Skinning
→ Final Vertex
```

DAZ Studio 是主 Golden Reference。

---

# 20. Skeleton / Skinning

按真实资产逐步实现 LBS、DQ、TriAx 和 Blend，不追求首版覆盖全部历史行为。

---

# 21. FK / IK / PowerPose

FK 直接作用 Runtime Skeleton。

Two-Bone IK 用于 Arm / Leg。

FABRIK 用于 Spine / Tail / Custom Chain。

Pinning 支持 Foot Lock、Hand Lock 和 Pelvis Move。

PowerPose 使用 Data-driven 2D Control，UI 不直接操作 Mesh 或 Cycles。

---

# 22. Material Runtime

不直接从 DAZ Material 写 Cycles Node。

先构建 Renderer-agnostic Semantic Material：

```text
DAZ Shader Semantics
→ Semantic Material
→ Cycles Shader Graph
```

具体 Shader Node 组织方式由开工时实际 Blender/Cycles 源码决定。

---

# 23. DAZ Material Compatibility

| Material Family | Priority |
|---|---:|
| Iray Uber Base | P0 |
| Base Color / Roughness / Metallic | P0 |
| Normal / Bump | P0 |
| Cutout Opacity | P0 |
| Emission | P0 |
| Dual Lobe | P1 |
| Top Coat | P1 |
| Transmission | P1 |
| PBRSkin | P1 |
| SSS | P1 |
| Complex Hair | P2 |
| Legacy 3Delight | Later |
| Custom Plugin Shader | 按需 |

---

# 24. Blender Material Reference Harness

建议建立开发工具，用于检查或导出 Blender Reference Scene 中的 Material Node Graph、Principled Parameters、Texture Binding、Normal/Bump Structure 和 Mixing Structure，再与 Our Semantic Material 对比。

具体导出方式由当时 Blender API 决定，不写死。

---

# 25. Blender 作为验证工具

开发阶段建议保留 Blender，作为 Visual Comparison、Node Graph Inspection、Rig Inspection、Morph Inspection、Reference Image Generation、Cycles Behavior Verification 和 Source-level Debugging Reference。

---

# 26. Golden Geometry Test

比较：

```text
DAZ Studio Final Vertex
vs
Our Runtime Final Vertex
```

可额外比较 Blender Reference。

统计 Max Error、RMS Error 和 Affected Count。

---

# 27. Golden Render Test

固定 Camera、Resolution、Samples、Lighting 和 Renderer，尽可能控制随机因素。

比较：

```text
Blender Cycles Reference
vs
Our Cycles Output
```

---

# 28. RenderDoc / Nsight

RenderDoc 用于应用自身 Presentation / Overlay / Graphics Interop。

Cycles GPU 路径根据实际 Backend 使用适合的调试工具。

不得假定历史版本调试流程对当前版本仍成立。

---

# 29. Profiling

至少记录：
- Visible FPS
- Camera Update
- Render IR Sync
- Cycles Scene Update
- Cycles Render
- Present
- VRAM
- RAM

动态编辑时增加 Formula、Morph、JCM、Skinning、IK 和 Geometry Sync。

---

# 30. Cache

允许：
- Asset Path Cache
- Parsed DSON Cache
- Morph Index Cache
- Formula Graph Cache
- Material Semantic Cache
- Shader Cache
- Texture Cache
- Scene Build Cache

第一版允许 Library Change 后通过重启重建。

---

# 31. Unsupported Feature

禁止 Silent Fallback。

必须明确记录 Unsupported Material Semantic、Modifier、Skinning Mode、Extra Data 与 Reference Mismatch。

---

# 32. Phase 0 —— Cycles Feasibility

第一优先级。

交付物：

```text
CyclesViewportBench
```

任务：
- 读取当前 Blender/Cycles 源码；
- 确认实际版本；
- 确认 Build Layout；
- 确认可集成边界；
- 选择目标 GPU Device 路径；
- 构建静态代表性场景；
- 实现 Camera Orbit / Pan / Dolly；
- 实现 Interactive Viewport；
- 实现 Output / Presentation；
- 实现 FPS HUD 与 Profiler。

验收：

```text
>=20 Visible FPS
```

如果失败，不得继续堆 DAZ Compatibility，必须先定位 Device Backend、Render Scheduling、Output Buffer、Presentation、CPU/GPU Sync、Sample Policy、Denoising、Camera Update、Scene Update 与 Acceleration Reuse。

---

# 33. Phase 1 —— Render Scene IR + Cycles Adapter

实现 Renderer-agnostic Render Scene。

验收：
- Static Scene Render
- Camera-only Update
- Cycles 与 DAZ Runtime 解耦

---

# 34. Phase 2 —— Minimal DAZ Scene Loader

只支持指定测试 Scene：
- Content Root
- DUF
- DSF
- Mesh
- UV
- Transform
- Camera
- Texture
- Minimum Material Subset

---

# 35. Phase 3 —— Material Reference Pipeline

建立：

```text
Blender Reference
Our Semantic Material
Cycles Shader
```

优先支持 Iray Uber Base 常用字段。

---

# 36. Phase 4 —— Morph Runtime

实现 Property、Morph Discovery、Sparse Delta、Active Set、UI Slider 和 Dirty Propagation。

---

# 37. Phase 5 —— Skeleton / ERC / JCM

实现 Skeleton、Formula、ERC、Skinning 和 JCM。

DAZ Studio 为主 Reference。

---

# 38. Phase 6 —— FK / IK / Pinning

实现 FK、Two-Bone IK、可选 FABRIK、Pinning 和 Constraints。

---

# 39. Phase 7 —— PowerPose

实现 2D Body Map、FK Node、IK Effector、Pin Marker 和 Pelvis Control。

---

# 40. Phase 8 —— Compatibility Expansion

真实资产驱动：

```text
Asset fails
→ Identify semantic
→ Check DAZ / Blender Reference
→ Write failing test
→ Implement
→ Regression
```

---

# 41. 性能 Invariant

P1：Camera Move 不解析 DSON。  
P2：Camera Move 不求 Morph。  
P3：Camera Move 不做 Skinning。  
P4：Camera Move 不重建 Material。  
P5：Camera Move 不重新上传静态 Texture。  
P6：Camera Move 不重建静态 Topology。  
P7：UI 不等待完整 Cycles Frame。  
P8：旧 Camera State 可丢弃。  
P9：Camera-only 操作中静态 Render Scene IR 保持 Immutable。

---

# 42. 主要风险

## R1 —— Cycles Standalone Integration
风险：Medium  
缓解：读取当前源码、Phase 0 独立完成、先最小静态场景。

## R2 —— Camera-only ≥20 FPS
风险：Medium  
缓解：Native Benchmark、Source-level Profiling、GPU Backend、Latest-State-Wins、必要时针对当前 Cycles 版本优化集成路径。

## R3 —— DAZ Material → Cycles
风险：Medium / High  
缓解：Blender Reference、Official Bridge Reference、Semantic Material IR、Golden Render。

## R4 —— DAZ Skinning / JCM
风险：Medium / High  
缓解：DAZ Studio Golden Vertex、Blender 辅助参考、测试驱动。

## R5 —— 第三方长尾资产
风险：Medium。

---

# 43. 当前工程信心

总体工程信心：

```text
约 90%
```

| 子系统 | 信心 |
|---|---:|
| DSON / DUF / DSF | 95% |
| Render Scene IR | 97% |
| Cycles 独立集成 | 88–92% |
| 静态 Camera ≥20 FPS | 85–90% |
| Standard Morph | 95% |
| Third-party Standard Morph | 90% |
| ERC / Formula | 88–92% |
| JCM | 85–90% |
| FK | 98% |
| IK | 98% |
| PowerPose | 95% |
| Material 基础映射 | 90% |
| 高级 Skin / SSS / Hair | 75–85% |
| 历史所有 DAZ Asset | 初期 <70% |

---

# 44. OpenSpec 目录建议

```text
/specs
  /000-project-bootstrap
  /001-cycles-static-camera
  /002-render-scene-ir
  /003-dson-minimal-scene
  /004-material-reference
  /005-morph-runtime
  /006-skeleton-skinning
  /007-erc-jcm
  /008-fk-ik
  /009-powerpose
  /010-golden-validation
  /011-performance-regression
```

每个 Spec 包含：

```text
proposal.md
requirements.md
design.md
tasks.md
tests.md
```

---

# 45. Spec Model Gate

| Spec | Design | Implementation | Review |
|---|---|---|---|
| 000 | Astra High | Sol High | Astra |
| 001 | Astra xhigh | Sol High | Astra xhigh |
| 002 | Astra High | Sol High | Astra |
| 003 | Astra High | Sol High | Astra |
| 004 | Astra xhigh | Sol High | Astra |
| 005 | Astra High | Sol High | Astra |
| 006 | Astra High | Sol High | Astra |
| 007 | Astra xhigh | Sol High | Astra xhigh |
| 008 | Sol/Astra | Sol High | Astra |
| 009 | Sol High | Sol | Sol/Astra |
| 010 | Astra High | Sol High | Astra |
| 011 | Sol High | Sol High | Astra |

---

# 46. Version 0.1 Definition of Done

```text
1. 独立程序启动。
2. 不依赖 DAZ Studio 运行。
3. 不依赖 Iray SDK。
4. Cycles 为主要 Beauty Renderer。
5. 指定 DAZ 测试场景可加载。
6. 静态 Camera Navigation >=20 Visible FPS。
7. 基础 DAZ 材质正确映射到 Cycles。
8. 指定角色正确加载。
9. 标准 Morph 可调。
10. 第三方 Morph 可调。
11. ERC / JCM 测试通过。
12. FK 可用。
13. IK 可拖动。
14. Pinning 可用。
15. PowerPose-like UI 可用。
16. DAZ Geometry Golden Test 通过。
17. Blender/Cycles Render Golden Test 通过。
18. Profiler 完整。
19. RenderDoc 可调应用 Presentation / Overlay。
20. Unsupported Feature 全部可诊断。
```

---

# 47. 第一项实际工作

第一次启动 Agent 必须从：

```text
Spec 001 —— Cycles Static Camera Baseline
```

开始。

推荐模型：

```text
GPT-6 Astra / xhigh
```

Agent 第一阶段只允许：
- 读取用户当前 checkout 的 Blender/Cycles 源码；
- 确认实际版本；
- 确认当前 Build Layout；
- 确认 Cycles 独立集成边界；
- 确认目标 GPU Device 路径；
- 设计最小 Benchmark；
- 定义测量方法；
- 检查相关源码 License / NOTICE。

不得：
- 先写完整 DSON；
- 先写完整 UI；
- 先写 Morph；
- 先写 DAZ Material 全兼容；

先证明：

```text
当前版本 Cycles
+
Representative Static Scene
+
Camera Navigation
+
>=20 Visible FPS
```

---

# 48. 最终架构原则

> DAZ Runtime 负责语义；Render Scene IR 负责隔离；Cycles 负责渲染。

> Blender 是参考环境与源码参考，不是必须的产品运行时。

> Agent 可以直接参考用户当前 Git checkout 的 Blender/Cycles 源码，但具体内部实现必须以开工时版本为准。

> 本规范不固定容易随 Blender 版本变化的内部 API、文件路径或类名。

> 没有变化的场景数据必须保持不可变。

> Camera-only 操作必须绕开 Morph、Skinning、Material Translation。

> 第三方开源实现可以用于验证和研究，但源码复用必须严格遵守其实际版本许可证。

> 任何兼容性功能都应由真实资产、Golden Test 和可重复证据驱动。

> 任何性能优化都必须通过 Benchmark 证明，而不能只凭直觉。
