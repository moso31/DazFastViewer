# DazFastViewer

换装和细分更新已改为复用渲染会话，更新期间保留最后画面。模型参数只保留“渲染细分等级”，同时作用于当前预览与最终渲染；0 为基础网格，超出网格预算的等级会保留原值并显示原因。实现、实测与限制见[优化记录](Docs/scene_rebuild_optimization_execution_cn.md)。

新增自动穿戴与角色附件挂接：选中兼容角色后，在内容库双击服装、头发或 GeoGraft，自动解析当前角色并建立 Fit To／骨骼挂接。支持整套解除和更换目标、宿主遮盖面重算及 HD Nipples 基础资产入口；兼容性和脚本边界见[使用与实现记录](Docs/attachment_import_execution_cn.md)。

独立 DAZ Runtime，使用 Cycles 提供交互式路径追踪视口。当前已经能够直接加载用户指定的 Genesis 8.1 Basic Female 与 ARK Food Plate with Fries DUF；无需启动 Blender、DAZ Studio 或 Iray。

**Spec 003 角色与道具静态预览已于 2026-09-20 通过用户手动验收。** Spec 005 中文编辑器与直接 Morph 已通过工程验证，尚待用户手动验收；详细结果见各阶段执行报告。

已实现独立 Render Scene IR、Cycles Adapter、原生 DUF / DSF 加载、Mesh / UV / 基础材质、OptiX 视口、稀疏 Morph、Genesis 8 骨架 / 双四元数蒙皮、Formula / ERC / JCM、单帧姿势 DUF、OpenSubdiv 细分及相机操作。尚未实现 DAZ HD Morph 或完整 Iray Uber 材质，公式与变形的完整 DAZ Studio Golden 仍待验收。

最新 test3 修复已部署：HDRI / ToneMapper 导入与设置保存、左右分类参数面板、F 聚焦及 WASDQE 导航；同时修正黑皮肤、眼镜挂接和拖鞋形变。验证与近似边界见[本轮报告](Docs/test3_scene_repair_execution_cn.md)。

## Formula、ERC、JCM 与参数复测（Spec 007）

保存场景的脸型 / 服装修复：保留 ERC 的负数抵消输入，修正骨骼缩放补偿及基础 Morph 载入，改善服装生成位移的连续性。真实双角色的脸部已与 DAZ DBZ 对齐，服装仍有局部差异；[数值证据与边界](Docs/scene_deformation_execution_cn.md)。

**最新兼容性修复已部署。** 007 基线已中文提交为 `da5d4f8`，随后两批修复使 G8 新增 476 个可编辑条目、G8.1 新增 614 个；本轮追加恢复各 122 个。Ren Yao、BGM Ava、BGM Big Girl Base、Yuki、Mariko 和 EasyFeet 代表项通过实际变形与副屏操作。完整计数、条件型参数和仍受阻原因见 [兼容性报告](specs/007-erc-jcm/compatibility-report.md)。本次按用户授权，将修复与未生效参数清单以中文归档到本地 Git。

006 已中文提交为 `aa8fbc7`。007 归档时通过七项工程检查、G8 / G8.1 各 40 个姿势、独立数值对照与副屏滑块验证。G8 的 Arms Length、Chest Scale、Eyes Closed、HS Sanny Shy、Flex Quad Left 已启用且产生实际变形；当时默认可见可编辑条目从 720 增至 2,687，后续兼容性计数以上述最新报告为准。G8.1 使用新版 **Eye Blink** 闭眼，旧控制器空覆盖继续保留。

在工程目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Genesis8`，选中角色后在右侧搜索参数并拖动滑块。面板区分手动输入与最终 ERC 值；JCM 随姿势变化自动求值。详情见 [007 执行报告](specs/007-erc-jcm/execution-report.md)与[预览说明](specs/007-erc-jcm/tests.md)。HD、未解析依赖及其他限制仍显示具体禁用原因。

## 骨架、蒙皮与姿势（Spec 006）

006 本阶段实施范围已完成并部署，等待用户手动验收。用户提供的 Bed Hogs II 目录共 40 个姿势，在 Genesis 8 / 8.1 上均通过基础骨骼蒙皮检查；独立数学对照与副屏应用 / 恢复也已通过。详情见 [006 执行报告](specs/006-skeleton-skinning/execution-report.md)。

```cmd
powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Genesis8 -Pose "H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female\BH2 Basking.duf"
```

选中角色后，通过“文件 → 添加 / 应用 DUF…”或内容浏览器双击应用姿势、形态或材质预设；“恢复载入姿势”只恢复骨骼，保留 Morph。“预设应用详情”列出未应用参数；骨骼层级可在左侧展开。007 已接入 ERC / JCM 和带缩放的 DQS，后续已补上穿戴物跟随；完整 DAZ Golden、多帧动画及 IK 仍待完成。

## 中文编辑器与 Morph（Spec 005）

**005 当前阶段实施范围已完成并通过工程验证。** 核查依据与后续边界见[阶段完成核查](specs/005-morph-runtime/completion-review.md)；不代表整个产品的 Golden / 性能验收已经完成。

新增 Qt Widgets 编辑器，支持菜单、多个内容库、后台加载 DUF、场景对象选择、参数分组 / 搜索与滑块、变换参数及重置。参数目录同时发现直接 Morph、纯公式控制器、子节点别名与 HD 项；可编辑数量由当前内容库和目标角色决定。006 接入蒙皮，007 接入 Formula 骨骼控制器与 JCM，随后接入穿戴物跟随；HD 和不满足求值条件的项显示原因。

在工程目录运行：

```cmd
powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Character
```

道具使用 `-Sample Prop`，Genesis 8 Female 使用 `-Sample Genesis8`；默认 `Character` 仍是 Genesis 8.1 Female。也可直接打开 `out\DazFastViewer.exe`，通过“文件 → 添加 / 应用 DUF…”加载资产；“打开场景（替换）”用于替换当前场景。编辑器初始窗口只在第二屏打开，缺少第二屏会在输出目录记录错误并退出。

先在场景树选择对象，再在属性面板左侧选分类、右侧搜索和调整参数；每行提供滑块和数值输入。可显示隐藏项、收藏参数，悬停名称查看来源、最终 ERC 值及限制。位置、旋转、总体缩放和 XYZ 缩放归入 General / Transforms；重置恢复载入状态。视口右键旋转、Shift＋右键平移、滚轮缩放；F 聚焦当前对象或身体部位，点击视口后用 WASDQE 移动，Shift 加速。Environment Options 与 Tonemapper Options 也可从场景树编辑，并通过文件菜单保存 / 载入 `.dfv-render.json`；当前不回写原 DUF。

选择整个模型后按 Delete，或使用场景树右键 / “编辑 → 删除选中对象及其子对象”。删除人物会包含子对象、骨骼附件与绑定穿戴物；单独删除衣物保留人物。灯光也可删除，“文件 → 新建空场景”清空当前文档。复合 DUF、内嵌几何源资产继承与资源回收的验证见[本轮执行报告](Docs/scene_lifecycle_execution_cn.md)。

“项目 → 项目设置…”支持添加、移除、上移和下移内容库，并保存至工程根目录的 `DazFastViewer.project.json`。本机已写入用户指定的四个根目录；靠前的库优先解析相同资源路径。保存后重新扫描当前场景，并按稳定 ID 保留仍然兼容的 Morph 值与变换。内容浏览器顶部可以切换各个库。

项目配置不提交 Git；新检出可复制 [配置示例](configs/project.example.json)，或通过菜单建立配置。`--project <文件>` 可指定另一份配置，`--content-root` 临时追加到最高优先级；缺失目录会保留并标注。首次多库扫描可能耗时约一分钟，期间 UI 保持响应。G8.1 对部分旧 G8 表情使用空文件屏蔽，因此两代的参数清单不完全相同。具体核查见[多库与参数发现修复报告](specs/005-morph-runtime/content-libraries-report.md)。

- [实际编辑器截图](artifacts/editor-check/editor-morph.png)
- [Spec 005 执行报告](specs/005-morph-runtime/execution-report.md)
- [UI / Genesis 8 / FitTo 长期规划](Docs/editor_and_genesis8_roadmap_cn.md)

构建需要本机 Qt MSVC 套件（当前 `C:\Qt\6.10.3\msvc2022_64`，可用 CMake `DFV_QT_ROOT` 指定）。`tools/build.ps1` 同时构建编辑器和原基准工具，运行十项轻量 CTest；`stage_runtime.py` 调用 Qt 官方部署工具复制 DLL、插件、中文翻译及许可证记录。

## 原视口预览与后台工具

在工程根目录执行以下任一命令。程序默认只在第二块屏幕显示，1600×900；缺少第二屏时明确失败，不自动回到主屏。

```powershell
powershell -ExecutionPolicy Bypass -File tools/view_sample.ps1 -Sample Character
powershell -ExecutionPolicy Bypass -File tools/view_sample.ps1 -Sample Prop
```

右键拖动旋转，Shift + 右键拖动平移，滚轮缩放，Esc 退出。按需点击副屏窗口开始键盘交互；启动时不主动抢主屏焦点。每次输出保存到 `artifacts/view-时间戳`，包含截图和兼容性报告。指定的道具文件实际只有盘子和薯条。

```powershell
# 无窗口解析与报告
powershell -ExecutionPolicy Bypass -File tools/view_sample.ps1 -Sample Prop -Inspect

# 自定义 DUF，可重复指定 --content-root
.\out\CyclesViewportBench.exe --file 'D:\Assets\scene.duf' --content-root 'D:\Assets' --monitor 2

# 源码改变后构建；运行轻量 CTest，不自动跑渲染或性能测试
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

程序保留 `--devices`、`--device CUDA`、`--smoke` 离线渲染、`--dump-shaders` 无窗口材质图导出等诊断入口。`--preview-seconds 4` 可让预览在首帧后自动关闭。

## 材质参考与增量验证

2026-09-21 已进入 Spec 004，完成首批后台参考工具、凹凸与法线叠加、非透射 IOR 修正。完整 Iray 材质兼容仍在进行中，凹凸距离目前可能采用带诊断的近似值。

```powershell
# 两份指定资产的结构检查、后台渲染与对照页面；只在需要新对照时运行
python tools/reference/run_reference.py

# 不执行渲染，仅导出场景和 Blender 参考材质、检查顶点 / UV / 材质绑定
python tools/reference/run_reference.py --export-only

# 如需重新验证材质增量：同一个 Session 内修改道具材质并检查图像变化
python tools/reference/run_reference.py --sample prop --verify-delta
```

脚本使用已安装 Blender 5.2.2 与 DAZ Importer 5.2.0，全程后台，不保存用户偏好。默认 640×480、64 samples、OptiX；只作材质对照，不作性能测试。已有结果可直接打开：

- [道具对照页面](artifacts/material-reference/prop/comparison.html)
- [角色对照页面](artifacts/material-reference/character/comparison.html)
- [Spec 004 执行报告](specs/004-material-reference/execution-report.md)

原生 `--export-scene --file …` 无需创建 GPU 设备即可导出 `scene.json`。参考检查通过只证明约定的基础几何 / UV / 材质绑定一致；图像和复杂材质差异仍按报告保留，不自动认定 Golden 验收通过。

## 实际结果与文档

- [角色最终截图](artifacts/dson-character-final/viewport.png)、[道具副屏截图](artifacts/dson-prop/viewport.png)
- [真实 DUF 执行报告与限制](specs/003-minimal-daz-loader/execution-report.md)
- [总体规范](Docs/daz_cycles_runtime_spec_final_cn.md)、[当前状态](specs/STATUS.md)、[交接记录](HANDOFF.md)
- [Render Scene IR 设计](specs/002-render-scene-ir/design.md)、[DSON 加载设计](specs/003-minimal-daz-loader/design.md)
- [构建依赖](specs/001-cycles-static-camera/dependencies.md)、[第三方使用记录](specs/001-cycles-static-camera/third-party.md)

用户已要求停止重复性能测试。Spec 001 历史提交帧率见 [报告](specs/001-cycles-static-camera/execution-report.md)，严格 Visible FPS 仍未通过验证。本轮短预览不属于性能验收；不同颜色空间或材质版本的结果也不能当作同一性能基线。

`out/` 是本地开发暂存目录。用户资产保持原位、只读引用，不随工程分发；研究源码和生成树分别位于 `.research/`、`.deps/`。

Git 仓库保存源码、脚本、规范、验收记录及少量文本证据；不提交 `artifacts/`、`out/`、`build/`、`.research/`、`.deps/`。本文及执行报告中的截图和运行产物链接指向本地文件，远端仓库不包含这些文件。新检出需按构建依赖文档准备本地依赖和测试资产。
