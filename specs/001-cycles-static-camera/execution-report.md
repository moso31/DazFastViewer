# Spec 001 执行报告

日期：2026-09-20。已完成独立程序构建、设备识别、离线渲染、窗口交互和原生分辨率性能采集。用户观察效果正常并要求停止重复测试，已停止自动测试流程。

## 可用产物

- 程序：`out/CyclesViewportBench.exe`，默认 OptiX。
- 构建：`tools/build.ps1`；固定来源生成器：`tools/prepare_cycles.py`。
- 离线输出：`artifacts/smoke-optix/smoke.png`、`smoke.exr`；早期 CUDA smoke 位于 `artifacts/smoke-cuda/`。
- 交互：右键旋转、Shift + 右键平移、滚轮缩放、Esc 关闭。
- 性能原始证据：`artifacts/baseline-{cuda,optix}-*/`，包含 command、manifest、构建哈希、events.csv、swaps.csv、显存采样、截图和 run.json。

程序使用独立 Cycles v5.2.0、经逐文件审核接入的 35 个 Blender 5.2.2 文件，以及本项目构建和观测补丁。不是 Blender 完整应用，也不宣称与全部 Blender 5.2.2 源码完全相同。不会启动 Blender / DAZ Studio / Iray。

## 实测

RTX 4070 Ti SUPER，驱动 591.86。2560×1440 原生渲染，pixel_size=1，关闭 resolution divider / denoise / adaptive sampling。固定 seed=1337，max bounces=8。场景包含 1,048,578 个唯一三角形、2,097,154 个实例化后三角形、128 个物体实例加地面、32 张 2048² RGBA 贴图和 4 个有效面积光。

每轮暖机 10 秒，120 Hz 输入执行 60 秒 Orbit/Pan/Dolly，停止后累积 10 秒。

| 后端 / 运行 | 导航结果提交 FPS | 显示环境记录 |
|---|---:|---|
| CUDA 01 | 21.60 | 前台状态变化，不能用于验收 |
| CUDA 02 | 22.08 | 前台状态变化，不能用于验收 |
| CUDA 03 | 23.00 | 前台、尺寸、最小化检查通过 |
| OptiX 01 | 28.70 | 前台、尺寸、最小化检查通过 |
| OptiX 02 | 28.70 | 窗口检查通过；部分手动输入缺少日志，延迟追溯不完整 |
| OptiX 03 | 27.42 | 前台状态变化，不能用于验收 |

这些数字是新相机结果的提交帧率，**不是已经验证的 Visible FPS**。用户要求停止时最后一轮已生成完成记录；没有再安排后续轮次。默认后端选择 OptiX，依据是已有相同场景对照结果。

已检查的运行中，GPU 图像回读为零；最终提交 epoch 与最终输入一致；场景数据 dirty 事件在导航区间为零。完整矩阵的样本统计以各运行文件为准。`scene_data_dirty=0` 是更新条件观测，尚不能替代 shader / BVH / texture 的独立上传重建计数。

## 实现与验证边界

- 已通过相机邮箱并发快照及相机矩阵稳定性 CTest。
- 实际运行中完成设备枚举、Combined 渲染、OpenGL interop、渐进累积和正常关闭。首次 Combined 通道缺失及默认灯光强度为零的问题已修复，早期诊断数据不作为上述基准结果。
- 采用单容量相机邮箱、独立输入/控制/呈现线程、稳定 PBO 和三纹理槽。保留旧画面直到新纹理 fence 完成；不会重复计入旧 frame ID。
- PBO 到纹理存在 GPU 内部复制；“无 CPU 回读”不等于完全零拷贝。
- 矩阵测试之后增加了两个跨线程状态的原子保护与可选渲染延迟诊断参数。最终版本完成编译检查；遵照用户要求，没有重新跑整套性能矩阵。历史结果对应各自保存的构建哈希。
- 整理已有记录时发现 OptiX 02 的部分已应用相机没有输入事件：原代码只记录自动回放，未记录鼠标操作。已将输入记录统一到相机发布入口；这些历史缺失不能补造，分析报告单独列出 missing_input_event_epochs。
- `tests/runtime_checks.py` 为后续有需要时执行的错误路径、200 ms 延迟及窗口生命周期检查；本次未执行，不能记为通过。

## 仍未完成

1. 严格显示事件关联：PresentMon 2.5.1 启动 ETW 返回 `access denied`。当前账户没有所需权限，未修改账户组或提升权限。原始错误为 `artifacts/presentmon-probe.log`。其 [官方命令行文档](https://github.com/GameTechDev/PresentMon/blob/v2.5.1/README-ConsoleApplication.md) 说明了显示跟踪选项；当前结果保持 `visible_fps_verified=false`。
2. GPU kernel 独立计时、静态资源实际上传/重建计数、与同版本 Blender Reference 的图像误差对照。
3. 最终发布制品的完整静态传递依赖 LICENSE / NOTICE 审计。当前 `out/` 为本机开发暂存，不作为完成审计的发行包。
4. DAZ 加载、Render IR、Morph / Pose 等后续 Spec。本阶段只证明独立渲染链路可运行，不证明真实 DAZ 资产兼容性已实现。

已有证据足以推进后续问题分析；不因尚缺严格验收证明而反复运行同一性能用例。
