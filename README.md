# DazFastViewer

独立 DAZ Runtime，使用 Cycles 提供交互式路径追踪视口。当前已经能够直接加载用户指定的 Genesis 8.1 Basic Female 与 ARK Food Plate with Fries DUF；无需启动 Blender、DAZ Studio 或 Iray。

**当前阶段已于 2026-09-20 通过用户手动验收。** 验收范围为现有角色与道具预览，详细结果见执行报告。

已实现独立 Render Scene IR、Cycles Adapter、原生 DUF / DSF 静态加载、Mesh / UV / 基础材质、OptiX 视口以及相机旋转、平移、缩放。当前是基础网格预览，尚未实现 SubD / HD、Morph、蒙皮 / Pose 或完整 Iray Uber 材质。

## 打开真实样例

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
