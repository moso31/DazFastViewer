# test.duf 关闭降噪与原始采样收敛复测

日期：2026-09-22。007 后修复，008 未开始。本批修复按用户要求以中文说明归档到本地 Git，提交记录见 `git log`。

用户的验收目标是：不启用降噪，相机稳定后 15 秒内肉眼几乎无噪点，仅个别区域残留可接受。此前擅自开启 OptiX 降噪偏离了要求；历史报告中的 3.43 秒降噪画面不能作为原始采样收敛证据。本轮已关闭视口与离线渲染的降噪，并部署到 `out`。当前蓝噪声采样有改善，但尚未通过用户描述的 15 秒质量目标。

## 已实施

- 编辑器和基准 / 离线入口均显式设置 `use_denoise=false`，不再指定降噪设备。诊断配置也不提供开启降噪的选项。
- 编辑器默认使用 `BLUE_NOISE_FIRST`；正常离线输出使用 `BLUE_NOISE_PURE`。这是 Cycles 的采样序列，不是渲染后的滤波。本机 Blender 5.2.2 的 `intern/cycles/blender/sync.cpp:439–448` 对自动模式也按交互 / 后台渲染作此选择；独立入口此前遗漏了该映射。
- 编辑器最大累计样本数由 1024 增加至 4096，避免原始画面仍有噪点时因上限停止。提高上限只允许继续计算，不是提速。自适应最低 32 样本、阈值 0.01 保持不变。
- `--raw-sampling` 和固定性能回放保留原采样序列并关闭自适应，便于等样本对照；所有模式均关闭降噪。
- 新增 `--capture-seconds` 和 `--sampling-settings` 诊断入口，记录相机设置后的实际时间、已呈现样本、分辨率与运行时采样参数。`editor-render.json` 从最终 Integrator / DeviceScene 读取降噪开关、HDRI MIS 和采样表尺寸。

本轮没有减少光线反弹、移除 SSS / 发丝、降低视口分辨率，也没有增加图像平滑或能量截断。

## 实测方法与结果

原生输入为 `H:\g1\Scenes\test.duf`，不读取 DBZ。硬件为 RTX 4070 Ti SUPER，OptiX；视口为 774 × 894，像素倍率 1。逐次启动程序，在副屏框选 A / B / C 并设置相同前视角，从该次相机设置开始计时；15 秒保存原始显示画面，再继续到 4096 样本或 60 秒。资产加载不计入相机稳定后的时间。所有配置均禁用降噪。

| 配置 | 15 秒截图实际时间 | 已呈现样本数 | 继续采样结果 |
|---|---:|---:|---|
| 原 Tabulated Sobol，阈值 0.01 | 15.062 秒 | 576 | 60.061 秒，3856 样本 |
| Blue Noise First，阈值 0.01 | 15.063 秒 | 1392 | 27.937 秒，4096 样本 |
| Blue Noise First，阈值 0.003 | 15.000 秒 | 1088 | 49.937 秒，4096 样本 |

原始图片：[原采样 15 秒](../artifacts/raw-convergence/baseline-15s/timed.png)、[蓝噪声 15 秒](../artifacts/raw-convergence/blue-noise-15s/timed.png)、[更严格阈值 15 秒](../artifacts/raw-convergence/tight-adaptive-15s/timed.png)。配置、逐帧时间和采样参数汇总在 `artifacts/raw-convergence/sampling-comparison.json`。

蓝噪声画面的皮肤、浅色服装细噪点较少，仍能看见残留。收紧阈值增加了计算量，在本次 15 秒限制下没有进一步改善，故没有采用 0.003 作为默认值。上述样本数是显示批次的累计采样索引；自适应会让不同像素提前停止，不能把样本数比值当成有效光线吞吐量倍率，也不能把达到 4096 样本等同于收敛。

辅助统计用两种配置继续采样图的平均值作共同参照。A 皮肤区域的 8 位 RGB RMSE 为原序列 2.82、蓝噪声 1.80、更严阈值 2.07；浅色上衣区域分别为 2.19、1.43、1.53。区域坐标、平均色差及其他区域在 `interim-noise.json`。参照含有相关样本且自身也有噪声，不是独立真值，不能据此宣称绝对噪声下降比例。

这些是本机单次串行测量，未进行多轮随机种子统计。用户的 DAZ / Blender 未关闭；检查时主要 GPU 运算来自 Viewer，但整卡显存占用较高。本轮相机构图与用户 Iray 图片并非像素对齐，材质和色调也仍有既有差异，因此没有宣称与 Iray 同画质性能等价。

## 已排除的方向与下一步方案

1. **环境光 MIS 已实际生效。** 运行时 `background_mis=1`、CDF 尺寸为 4096 × 2048；不能继续把剩余噪声归因于没有开启环境采样。Light Tree 在当前 Cycles 中也已默认开启。
2. **静止后的场景重建不是本次主要耗时。** 原序列测试中几何 / 实例增量均为 0，`scene_sync` 事件合计约 1.74 毫秒，渲染工作事件合计约 60.25 秒。后者包含工作与 GPU 等待，不能当成纯 GPU 内核时间，但足以说明继续削减场景同步无法解决主要收敛问题。
3. **下一项应测光线与着色内核成本。** 分别统计阴影 / 透明穿透、表面着色、SSS 和发丝的耗时及高方差区域，再调整采样分配、重复着色和光路终止策略。最小反弹次数的变化可能降低方差，也会增加每样本成本，必须用相同时间的原始图验证，当前没有未经测试改成默认值。不能仅靠更小的自适应阈值解决。
4. **环境采样表有初始化与显存优化空间。** 目前对较小 HDRI 仍建立较大的 CDF；可独立验证按纹理有效分辨率生成分布表的收益。该方向主要减少构建 / 内存开销，尚无证据能显著改善静止后的 15 秒画质。DAZ 的 Environment Lighting Resolution 指极角高度，不能直接将其数值当成 Cycles 的表宽度，二者用途也不完全相同。定义见 [DAZ 官方环境文档](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/interface/panes/render_settings/engine/nvidia_iray/environment/start)。
5. **不能把 GPU Path Guiding 当作现成开关。** 当前 Cycles 的 GPU 渲染尚不支持该功能，见 [Blender 官方 GPU 限制](https://docs.blender.org/manual/ka/latest/render/cycles/gpu_rendering.html)。Iray Photoreal 另有引导采样机制，见 [NVIDIA 官方手册](https://raytracing-docs.nvidia.com/iray/manual/manual.260114.A4.pdf)，但这不能证明用户开启了该选项。若后续要引入类似策略，需要单独实现并验证，不能替代本轮测量。

## 验证与复现

最终构建 `DazFastViewer` / `CyclesViewportBench` 成功，13 项现有工程检查全部通过。离线 64 样本 OptiX 冒烟输出 PNG / EXR 成功，`offline-final/manifest.json` 记录 `denoise=false` 和 `blue_noise_pure`；该小场景仅验证输出路径，不代表真实场景的收敛性能。

最终部署程序不带采样覆盖配置再次运行真实场景，15.000 秒实际呈现 1232 样本，31.812 秒达到 4096。运行时确认降噪关闭、Blue Noise First、自适应阈值 0.01、HDRI MIS 开启、几何 / 实例更新均为 0。最终原始图见 [15 秒画面](../artifacts/raw-convergence/final-default-15s/timed.png)和[继续采样画面](../artifacts/raw-convergence/final-default-15s/reference.png)；流程检查 PASS 不代表视觉收敛达标。两次相同蓝噪声设置存在运行时间波动，进一步说明不能把单次样本索引比值作为稳定提速倍率。

`out` 与 `build/bin/Release` 两个可执行文件的 SHA-256 已核对一致，记录于 `artifacts/raw-convergence/final-evidence.json`。测试进程均已退出。

```powershell
& .\out\DazFastViewer.exe --file 'H:\g1\Scenes\test.duf' --capture-test --capture-target A --capture-target B --capture-target C --capture-front --capture-seconds 15 --capture-samples 4096 --output artifacts/raw-convergence/final-default-15s
```

对照时追加 `--sampling-settings artifacts/raw-convergence/baseline.json` 或 `blue-noise.json` / `tight-adaptive.json`。日志在 `artifacts/raw-convergence/build-final.log`、`ctest-final.log` 和 `stage-final.log`。

早期三组实验的 `editor-check.json` 留有旧的硬编码 `denoise: OPTIX` 标签，真实渲染开关已是关闭，以同目录 `editor-render.json` 中读取到的 `sampling.denoise=false` 为准。最终代码已更正这一诊断标签；保留原始实验文件，不倒改测量证据。
