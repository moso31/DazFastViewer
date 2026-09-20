# 两份真实 DUF 的执行报告

日期：2026-09-20。实际使用独立 `out/CyclesViewportBench.exe`，没有启动 Blender、DAZ Studio 或 Iray。

## 用户手动验收

2026-09-20，用户通过样例入口进行手动验收，并明确反馈“验收通过了”，要求以中文提交并上传当前工程。此次确认覆盖当前角色与道具预览阶段，后文列出的未实现语义与严格 Visible FPS 状态保持不变。本次归档不再重复运行渲染或性能测试。

## 结果

- **角色**：Genesis 8.1 Basic Female，解析 4 份 DSON 文档，1 个基础网格，17 个材质、9 张已映射贴图，32,736 个三角形。最终 [副屏截图](../../artifacts/dson-character-final/viewport.png)、[资产报告](../../artifacts/dson-character-final/asset-report.json)、[运行报告](../../artifacts/dson-character-final/run.json)、[日志](../../artifacts/dson-character-final.log)。
- **道具**：ARK Food Plate with Fries，解析 3 份 DSON 文档，1 个网格，2 个材质、5 张已映射贴图，3,536 个三角形。[副屏截图](../../artifacts/dson-prop/viewport.png)、[资产报告](../../artifacts/dson-prop/asset-report.json)、[运行报告](../../artifacts/dson-prop/run.json)、[日志](../../artifacts/dson-prop-preview.log)。这份文件只有盘子和薯条。
- 两个窗口均记录 `monitor 2 / \\.\DISPLAY2 / 1600×900`，创建位置 `(2712,46)`。没有在主屏显示测试窗口。
- 角色相机检查通过，见 [camera-check.json](../../artifacts/dson-character/camera-check.json)：初始设置加三次相机更新共 4 次；网格创建数量始终为 2（人物 + 地板）；只有初始 epoch 2 标记静态场景脏；最终输入与呈现均为 epoch 5。
- `SceneIRTest` 通过；相关结果保存在本 Spec 的 `evidence/scene-ir-ctest.log`。

每个渲染产物目录包含当次 `build-manifest.json`，可区分源码和可执行文件版本。道具截图和相机检查早于下面的 sRGB 修复，最终角色截图晚于修复；未为更新同类证据再次运行道具或相机回放。

## 两个具体问题及修正

1. **预览灯光过曝**：首次道具运行能够加载但画面大片泛白。按 Cycles 归一化灯功率及资产尺度调整预览灯光；修正后盘子图案、薯条、阴影可辨认。原图保留为 `artifacts/dson-prop-overexposed.png`。
2. **角色肤色偏灰**：最初颜色贴图使用 `u_colorspace_srgb`。核对已解析材质、UV 和最终 Shader Graph 后，通过一次无窗口 Diffuse Color 输出确认颜色在贴图处理阶段偏亮。当前源码的该颜色空间进入 CPU 编码路径；应用固定在线性 Rec.709，应使用 `u_colorspace_scene_linear_srgb`，保留 sRGB 编码并由 SVM 解码一次。修正后的副屏截图恢复了肤色、眉毛等贴图细节。中间诊断保留在 `artifacts/character-shaders`、`artifacts/character-albedo` 和 `artifacts/dson-character-uv`，不能误作最终结果。

本轮仅做与实际缺陷相关的复查，没有重新跑整套性能基准。窗口关闭后会导出 PNG；无窗口诊断可以输出线性 EXR、sRGB PNG 和 Diffuse Color PNG。

## 兼容性结论

角色 20 条诊断：17 个材质仍有未映射通道，另有 SubD、HD Modifier、Skinning 诊断。道具 3 条诊断：2 个材质通道子集与 Skinning。两份资产的 `fully_supported` 均为 false；可以静态预览不代表完整 DAZ 求值或最终材质等价。

没有应用 SubD / HD、Morph、蒙皮 / Pose，也没有完成 SSS、Bump、Top Coat 或材质 Reference。当前相机与灯光为应用提供的预览环境。下一项有价值的工作是建立材质 Reference 和差异报告，先补齐角色皮肤 / 眼睛与道具法线 / 粗糙度的语义；之后按总体规范实现变形链路。

严格 Visible FPS 仍未验证。当前预览的 `benchmark=false`、`navigation_submission_fps=null`，不会将短预览误记为性能验收通过。

## 运行入口

```powershell
# 默认副屏，保持交互直至 Esc / 关闭窗口
powershell -ExecutionPolicy Bypass -File tools/view_sample.ps1 -Sample Character
powershell -ExecutionPolicy Bypass -File tools/view_sample.ps1 -Sample Prop

# 无窗口读取并输出兼容性报告
powershell -ExecutionPolicy Bypass -File tools/view_sample.ps1 -Sample Prop -Inspect
```

脚本每次创建独立的 `artifacts/view-时间戳` 目录，不覆盖以上验收证据。副屏不存在则报错，不回退主屏。
