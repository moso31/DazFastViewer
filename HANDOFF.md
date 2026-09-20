# 交接记录

更新：2026-09-20。当前已实现独立 Cycles 视口和两个指定 DUF 的静态预览，不要将项目描述成尚未构建或不支持任何 DAZ 加载。

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

实际角色导航通过：4 次相机设置（初始 + 旋转、平移、缩放），后端网格创建仍只有人物与地板 2 个；静态场景脏事件只在初始 epoch 2，最终输入与显示 epoch 5 一致。没有运行新的性能轨迹。

修复预览灯光过曝与 sRGB 编码重复。当前固定线性 Rec.709，颜色贴图必须使用 `u_colorspace_scene_linear_srgb`，数据贴图用 `u_colorspace_data`；别改回此版本执行 CPU 编码的 `u_colorspace_srgb`。最终人物截图在 `artifacts/dson-character-final`；旧灰色截图及 Shader / Albedo 诊断保留作失败证据。

启动入口 `tools/view_sample.ps1 -Sample Character|Prop`，默认副屏并生成新的输出目录。`--inspect` 无窗口 / 无 GPU；`--dump-shaders` 无窗口导出材质图与绑定；`--smoke --file` 可离线导出 Combined EXR / PNG 与 Diffuse Color PNG。`tools/build.ps1` 只自动运行轻量 CTest，不启动 demo 或性能回放。

新增 nlohmann/json 3.12.0，MIT，来源及哈希位于 `third_party/nlohmann/SOURCE.md`，许可复制到 `out/licenses/nlohmann-json`。没有复用 Diffeomorphic / DAZ Studio 实现代码。

## 事实边界与下一步

当前是静态基础网格 + 基础 PBR，未实现 SubD / HD、Morph、Skinning / Pose、ERC / JCM、完整 Iray Uber、完整 DAZ 变换 / 相机 / 可见性。自动取景和三盏灯是应用预览环境。报告 `fully_supported=false` 不能隐藏或改成完全兼容。

Spec 002 材质 Delta 接口已有，但图像与资源行为尚未单独验收。GPU 字节级上传计数仍未完成；主机网格创建计数不是显存上传证明。

Spec 001 历史 CUDA 提交帧率 21.60–23.00、OptiX 27.42–28.70；严格 Visible FPS 因 PresentMon ETW 权限问题尚未验证。不要为了补结论重复跑分，也不要把提交 FPS 改成 Visible FPS。历史数据须按各自 build-manifest 区分，颜色空间等改动影响可比性。

`SceneIRTest / scene_ir_dson` 已通过，`CameraMailboxTest` 前一阶段已通过。本轮没有重跑 `tests/runtime_checks.py` 的延迟注入 / 最小化流程；相机验证仅复用其窗口定位辅助函数。

下一阶段优先建立材质 Reference 和差异报告，再补皮肤 / 眼睛、法线 / 粗糙度等语义，随后按总体规范推进变形链路。当前无需新增第三方源码。

## 构建环境与保护事项

- CMake / MSVC：VS 2022 Community，C++20；构建 `build`、程序 `out`。
- Blender 原目录 `D:\Github\blender` 的 main / 5.3 alpha 与空 index（20638 staged deletions）是已有状态，**禁止 reset / clean**。
- 独立工作树 `D:\Github\blender-5.2.2`，commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`，分支 `dazfastviewer-blender-5.2.2`；跳过缺失的 LFS 笔刷二进制，不影响已使用源码。
- 独立 Cycles `.research/cycles-v5.2.0`，commit `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba`；Windows 库 `.research/windows-libs-metadata`，commit `60d6e96b917568278d400a4024c98da0fb777338`。
- `tools/prepare_cycles.py` 生成 `.deps/cycles`，移植 35 个 Blender 5.2.2 文件（34 个补丁与 image_maketx），其余保留独立来源；保留 BSD sky / 独立 allocator，不链接 Blender GPL guardedalloc。不要直接修改生成树，应用项目补丁并记录来源。
- CUDA 12.9.41；OptiX `D:\Github\optix-dev`，v9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d`；RTX 4070 Ti SUPER、驱动 591.86。
- 已按用户要求初始化主工程 Git 仓库，默认分支 `main`，提交说明使用中文。构建、运行产物、研究源码和用户模型均不加入版本管理；`out/` 仍是开发暂存，整体发布许可与依赖审计未完成。

详细阶段状态见 [specs/STATUS.md](specs/STATUS.md)，本轮实测见 [Spec 003 执行报告](specs/003-minimal-daz-loader/execution-report.md)。
