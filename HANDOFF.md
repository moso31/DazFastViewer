# 交接记录

## 最新状态：007 两批参数兼容性修复

原 007 已中文提交为 `da5d4f8`。用户随后要求修复一版，并追问 354 项之外的空间；已补充声明资产 URI 恢复，最终 G8 / G8.1 新增可编辑 476 / 614 项，本轮各追加 122 项，无原可编辑条目退化。本次按用户授权，将兼容性修复、验证证据和未生效参数清单以中文归档到本地 Git；提交号见 `git log`。已按用户明确授权关闭旧版窗口并覆盖部署 `out`，不要再次请求关闭许可。

最终证据只认 `artifacts/spec007-compatibility/final`，第一批 `validated` 为中间记录。8 项 CTest、两代各 40 姿势、新增条目取样 / 恢复、30 组数学对照、一次副屏六参数流程已通过，无需继续重复测试。参考脚本补齐多形态及默认控制器贡献，初次失败记录已保留；部分权重来自 Runtime，不是整图独立求值或 DAZ Studio Golden。G8 / G8.1 新增项中 71 / 63 个条件型条目在当前独立取样下无明显位移，未视为视觉验收通过。

实现：有符号 `-1` 计数、基于 SkinBinding 的节点 / 网格确认、同目录唯一内部 ID 恢复、声明 `asset_info.id` 与内部 ID 的唯一匹配、控制器下游诊断。空覆盖、代际屏蔽、歧义、HD 和资产锁定语义不变。当前资源中的 G8.1 `CTRL Vo Xiao Mei` 默认值 1 会影响初始外观，不可偷偷归零。

剩余优先方向：16 个骨骼属性别名结合 008；错误地址的可审查项目映射；睫毛 / 服装跨对象控制结合 FitTo / AutoFollow；真实缺失依赖按清单补包；HD / SubD 单独实施。无需用户全库解压或新增第三方源码。详见 [兼容性报告](specs/007-erc-jcm/compatibility-report.md)与[证据摘要](specs/007-erc-jcm/evidence/compatibility-summary.json)。以下是历史记录。

## 最新状态：Spec 007 / Formula、ERC、JCM 与 005 复测

用户要求先提交 006 再执行 007，并复测此前不能启用的 Morph。006 已中文提交为 `aa8fbc7`。007 当前实施范围已完成、部署并通过工程验证；用户已自行确认大量参数启用、姿势 DUF 工作正常，本次按新授权中文归档。后续修复 Ren Yao 的计数 / 目标绑定及 BGM 的旧文件名引用，不能把此前工程检查视为所有商业参数或 DAZ Studio Golden 已通过。

新增 `src/runtime/formula.*`、`deformation.*`、`src/daz/formulas.*`，统一 Formula → Morph / JCM → Skinning。支持 sum / mult、RPN / 样条、bool 默认值、Alias、循环与脏传播；基础骨架的节点公式也必须读入。两阶段 DQS 接入缩放及绑定中心变化。面板显示最终值，非法编辑保留上一有效状态。

G8 的 Arms Length、Chest Scale、Eyes Closed、HS Sanny Shy、Flex Quad Left 通过非零 / 归零检查与副屏滑块操作；默认可见可编辑从 720 增至 2,687。G8.1 从 745 增至 2,260，使用 FACS Eye Blink；旧 Eyes Closed / 眼睛方向的空覆盖继续保留。不可为了点亮旧项修改资产或绕过覆盖。别名、JCM 开关以及 G8 / G8.1 各 40 个姿势均通过。

最终七项 CTest、12 组独立数值对照、副屏流程通过。独立最大顶点差 `4.631754e-7 m`。源文件 / 程序哈希与结果见 [007 证据摘要](specs/007-erc-jcm/evidence/summary.json)，原始证据只认 `artifacts/spec007/validated`；早期目录是开发中的中间结果。当前无需继续扫描、demo 或性能回放。

剩余边界：DAZ Studio Golden、完整 TCB 资源对照、HD / SubD、TriAx / Blend、指向骨骼属性的面板 Alias、缺失 / 覆盖依赖、FitTo、FK / IK。下一规划为 008 FK / IK，FitTo 实测前再收集 G8 同代服装。详见 [007 报告](specs/007-erc-jcm/execution-report.md)与[预览命令](specs/007-erc-jcm/tests.md)。以下各阶段文字为历史记录，状态以本节为准。

## Spec 006 / 骨架、蒙皮与姿势 DUF 历史记录

用户授权启动 006，并提供 `H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female` 的姿势用于蒙皮检查。当前实施范围已完成并部署；用户已明确要求将 006 以中文归档到本地 Git。此归档不代替完整 DAZ Golden。005 已提交为 `ee2ba05`。

新增 `src/daz/skeleton.*`、`src/daz/pose.*`、`src/runtime/skeleton.*`：G8 / G8.1 各 171 节点、General DualQuat，按节点所属范围匹配姿势 name / id。`SkinningRuntime` 缓存 Morph 输出，应用 / 恢复姿势不累积变形。Qt 新增姿势菜单、Explorer 分派、骨骼树、取景与诊断，`tools/edit_sample.ps1` 新增 `-Pose`。

六项工程 CTest、G8 / G8.1 各 40 姿势、三种姿势的独立 mathutils 数值对照及副屏端到端均通过；最大参考顶点差约 `6.431e-7 m`，恢复误差 0。第一轮副屏发现同名控制器 / Alias 范围问题，修复后复验通过；不要继续无目的重复扫描、demo 或性能回放。当前部署 EXE SHA-256 为 `08feda362e646fb07239785dcf78d9945e52fc96147ee341f07718efed489ad8`。

**边界：** ERC / JCM、体型驱动骨骼中心、骨骼缩放的 DQS、多帧动画、TriAx / Blend、FitTo / IK 和 DAZ Studio Golden 尚未完成。目录中眼睛上下 / 左右两个非零控制器各出现于 33 个文件，明确报告未应用。下一阶段为 007 ERC / JCM；服装样例在 FitTo 验证前再收集，不影响本次姿势验证。详见 [006 执行报告](specs/006-skeleton-skinning/execution-report.md)及[预览命令](specs/006-skeleton-skinning/tests.md)。

## Spec 005 / Qt 编辑器历史记录

**阶段归档：** 用户本轮要求检查 005 完成后以中文提交。已对照总规范第 36 节确认当前阶段实施范围完成，并复核源码 / 部署 / 证据哈希一致。本次提交归档 005；核查过程没有再次运行 demo、全库扫描或性能回放。后续 Formula / Skinning / FitTo / IK 和整体 Golden 保留未完成，见 [阶段完成核查](specs/005-morph-runtime/completion-review.md)。

**最新修订：多内容库与控制器 / 别名发现。** 用户已提供四个根目录，写入本地（Git 忽略的）`DazFastViewer.project.json`；“项目 → 项目设置”可持久修改顺序。内容库配置用于场景依赖和 Morph 发现，脚本不再把样例限制为单库。`-Sample Genesis8` 新增 G8 基础角色入口，`Character` 仍是 G8.1。

参数发现补齐纯控制器、子节点 Alias、外部引用、原始分组，修复多段 gzip 的 57 个资源读取失败。G8 发现 6,012 个参数条目（720 个默认可见且可直接编辑），G8.1 为 6,799 / 745；均有 2 份 Neko count 不一致诊断。四个指定参数在 G8 已找到，但因公式 / 骨骼求值未实现仍只读；旧 Eyes Closed 控制器在 G8.1 被空覆盖屏蔽。新参数树仅为选中项创建编辑控件，支持分组与显示隐藏项。

五项轻量回归、新目录真实核对、新面板副屏编辑验证通过；最后布局 / 搜索计数小改只编译。最新证据见 [多库修复报告](specs/005-morph-runtime/content-libraries-report.md)。以下 225 / 23 / 16 和三项 CTest 是早期单库记录，不再代表当前全库清单。全库首次扫描可能约一分钟，后续应实现目录索引缓存；不要为了计时重复扫描或跑性能测试。

用户已授权进入 005，并补充长期 Explorer、菜单、加载角色、SceneHierarchy、Morph、FitTo 需求；最终 UI 采用 **Qt Widgets**，不再采用 Windows 原生控件作为应用框架。现有 WGL 视口嵌入 Qt，仍独立渲染与 GPU 互操作。Qt 套件为 `C:\Qt\6.10.3\msvc2022_64`；6.11.2 安装是 MinGW，不能与现有 MSVC Cycles 混链。

新增 `out/DazFastViewer.exe` 和 `tools/edit_sample.ps1 -Sample Character|Prop`。中文菜单、内容浏览、选择对象、直接 Morph、变换 / 重置、后台场景切换可用。UI 不直接改网格，带文档世代号的快照交给工作线程求值。当前场景树仅包含可渲染对象，视口固定 960×720。

Morph 发现位于 `src/daz/morphs.*`，纯 C++ 求值位于 `src/runtime/morph.*`；Render IR Delta 增加顶点和实例变换。G8.1 使用经过逐三角形编号验证的 G8 同性别兼容桥，空覆盖文件不会被忽略。当前角色发现 225 个稀疏 Morph，23 个可直接求值、16 个默认可见；其余 Formula / HD 项不可用。支持载入保存的直接 Morph 权重，目标 URI 不匹配不能仅凭顶点数套用。

三项 CTest、真实 Bodybuilder Size 0.5 / 1 / 0 数值（10,905 偏移、误差 0）、副屏 Morph / 变换 / 重置 / 相机及角色换道具均 PASS。相机不求值 Morph，几何始终 2 个；验证未重跑性能基准。证据见 [Spec 005 报告](specs/005-morph-runtime/execution-report.md)。

FitTo / 蒙皮 / ERC / JCM / IK、HD、保存 DUF、完整 Undo、动态视口尺寸和完整骨骼层次尚未实现。[长期规划](Docs/editor_and_genesis8_roadmap_cn.md)定义绑定关系、额外骨骼、体型跟随、菜单与验证顺序。服装样例在进入 006 / FitTo 时再向用户收集，005 不因此阻塞。Spec 004 材质 Golden 和严格 VisibleFPS 仍保留未完成。

Spec 004 中文提交基线为 `be0dbd1`。005 本次按用户新授权归档为中文本地提交，具体提交号见 `git log`；仓库未配置远端。

以下为已验收预览及 Spec 004 的历史记录，若与上述阶段进展不同，以最新状态为准。

更新：2026-09-21。当前已实现独立 Cycles 视口和两个指定 DUF 的静态预览；已获授权进入 Spec 004，完成首批后台材质参考及差异修复，不要将项目描述成尚未构建或不支持任何 DAZ 加载。

用户已于 2026-09-20 明确确认手动验收通过，并要求以中文提交、上传当前工程；验收记录已补充至 Spec 003。归档不需要重跑 demo 或性能测试。

## 用户约束

- 用户已要求实际执行所有开发与验证流程，已确认模型切换并授权继续本阶段；不要重复确认已有授权。
- 停止反复性能回放。只在新改动或具体故障需要时做针对性验证。
- **所有测试 demo 在第二块屏幕运行**。主屏 `DISPLAY1` 为 2560×1440，右侧副屏 `DISPLAY2` 为 1920×1080、起点 `(2560,0)`；默认窗口 1600×900，不抢主屏焦点，缺副屏明确失败。
- 角色：`C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8.1 Basic Female.duf`。
- 道具：`H:\G3\Environments\Architecture\ARK Modern Modular Cafe\Props\Tabletop\ARK MM Cafe - Food Plate with Fries.duf`，实际只有盘子和薯条，无汉堡。
- 用户资产只读引用，不复制进源码、不修改原文件。

## 已完成

`src/render_ir` 为纯 C++ 场景数据层；`src/daz` 解析 JSON / gzip DSON、内容库 URI、Mesh / UV / 常用静态变换与基础材质；`src/cycles` 执行后端映射及相机 / 材质 Delta。原 `src/bench/scene.*` 已由 IR Fixtures 替代并删除。

两个真实文件已无窗口 inspect，且在副屏显示。角色 16,556 顶点 / 32,736 三角形 / 17 材质 / 9 贴图；道具 1,794 / 3,536 / 2 / 5。渲染统计另外加一张地板（2 三角形）。兼容性诊断分别 20 / 3 条。

上述为 Spec 003 记录。Spec 004 接入 Bump 后，角色 / 道具贴图为 16 / 7，诊断为 36 / 5（新增凹凸距离近似提示，不能消除提示来伪装完整兼容）。非透射材质 IOR 修正为 1.5。

`tools/reference/run_reference.py` 以本机 Blender 5.2.2 + DAZ Importer 5.2.0 后台导出节点、检查独立基础顶点 / UV / 材质绑定，再共用三角网格渲染。两资产结构 PASS，已有对照在 `artifacts/material-reference/{prop,character}/comparison.html`，无需重跑查看。完整版图像 Golden 尚未通过，角色仍有显著散射 / 高光差异；详见 [Spec 004 报告](specs/004-material-reference/execution-report.md)。

实际角色导航通过：4 次相机设置（初始 + 旋转、平移、缩放），后端网格创建仍只有人物与地板 2 个；静态场景脏事件只在初始 epoch 2，最终输入与显示 epoch 5 一致。没有运行新的性能轨迹。

修复预览灯光过曝与 sRGB 编码重复。当前固定线性 Rec.709，颜色贴图必须使用 `u_colorspace_scene_linear_srgb`，数据贴图用 `u_colorspace_data`；别改回此版本执行 CPU 编码的 `u_colorspace_srgb`。最终人物截图在 `artifacts/dson-character-final`；旧灰色截图及 Shader / Albedo 诊断保留作失败证据。

启动入口 `tools/view_sample.ps1 -Sample Character|Prop`，默认副屏并生成新的输出目录。`--inspect` 无窗口 / 无 GPU；`--dump-shaders` 无窗口导出材质图与绑定；`--smoke --file` 可离线导出 Combined EXR / PNG 与 Diffuse Color PNG。`tools/build.ps1` 只自动运行轻量 CTest，不启动 demo 或性能回放。

新增 nlohmann/json 3.12.0，MIT，来源及哈希位于 `third_party/nlohmann/SOURCE.md`，许可复制到 `out/licenses/nlohmann-json`。没有复用 Diffeomorphic / DAZ Studio 实现代码。

## 事实边界与下一步

当前是静态基础网格 + 基础 PBR，未实现 SubD / HD、Morph、Skinning / Pose、ERC / JCM、完整 Iray Uber、完整 DAZ 变换 / 相机 / 可见性。自动取景和三盏灯是应用预览环境。报告 `fully_supported=false` 不能隐藏或改成完全兼容。

Spec 002 材质 Delta 已在 Spec 004 道具同一 Session 验证：2 次材质更新、22.233% 像素改变、网格 / 三角形计数与几何指针不变。GPU 字节级上传计数仍未完成；主机网格创建计数不是显存上传证明。

Spec 001 历史 CUDA 提交帧率 21.60–23.00、OptiX 27.42–28.70；严格 Visible FPS 因 PresentMon ETW 权限问题尚未验证。不要为了补结论重复跑分，也不要把提交 FPS 改成 Visible FPS。历史数据须按各自 build-manifest 区分，颜色空间等改动影响可比性。

`SceneIRTest / scene_ir_dson` 已通过，`CameraMailboxTest` 前一阶段已通过。本轮没有重跑 `tests/runtime_checks.py` 的延迟注入 / 最小化流程；相机验证仅复用其窗口定位辅助函数。

下一步继续 Spec 004：独立估计凹凸距离（不可写死样例数值），修正折射粗糙度 / 盘子高光，再对齐皮肤散射与复杂材质；尚不可跳过材质差异直接宣布 Golden 通过。当前无需新增第三方源码。已验收静态预览的基线提交为 `afc7737`；Spec 004 本批改动按用户要求以中文提交到本地 Git，具体版本见提交记录。

## 构建环境与保护事项

- CMake / MSVC：VS 2022 Community，C++20；构建 `build`、程序 `out`。
- Blender 原目录 `D:\Github\blender` 的 main / 5.3 alpha 与空 index（20638 staged deletions）是已有状态，**禁止 reset / clean**。
- 独立工作树 `D:\Github\blender-5.2.2`，commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`，分支 `dazfastviewer-blender-5.2.2`；跳过缺失的 LFS 笔刷二进制，不影响已使用源码。
- 独立 Cycles `.research/cycles-v5.2.0`，commit `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba`；Windows 库 `.research/windows-libs-metadata`，commit `60d6e96b917568278d400a4024c98da0fb777338`。
- `tools/prepare_cycles.py` 生成 `.deps/cycles`，移植 35 个 Blender 5.2.2 文件（34 个补丁与 image_maketx），其余保留独立来源；保留 BSD sky / 独立 allocator，不链接 Blender GPL guardedalloc。不要直接修改生成树，应用项目补丁并记录来源。
- CUDA 12.9.41；OptiX `D:\Github\optix-dev`，v9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d`；RTX 4070 Ti SUPER、驱动 591.86。
- 已按用户要求初始化主工程 Git 仓库，默认分支 `main`，提交说明使用中文。构建、运行产物、研究源码和用户模型均不加入版本管理；`out/` 仍是开发暂存，整体发布许可与依赖审计未完成。

详细阶段状态见 [specs/STATUS.md](specs/STATUS.md)，本轮实测见 [Spec 003 执行报告](specs/003-minimal-daz-loader/execution-report.md)。
