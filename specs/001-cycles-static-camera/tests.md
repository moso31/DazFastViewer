# 基准与验收方案

状态：以下为原始验收标准。实际执行情况见 [执行报告](execution-report.md)，未记录通过的测试仍为 NOT_RUN。用户已要求停止重复性能测试，以下三轮矩阵不是继续自动重跑的指令。

## 场景

| 名称 | 内容 | 用途 |
|---|---|---|
| smoke | 地面、漫反射/金属测试物、面积光、固定相机 | 首次输出、颜色和设备正确性 |
| medium-v1（提案） | ≥100 万唯一三角形、约 200 万实例化后三角形、128 个实例、32 个材质、32 张 2048² RGBA8 纹理、4 个面积光 | 可重复性能基线 |

medium-v1 使用确定性的三角函数和棋盘纹理生成数据，渲染 seed=1337，纹理未压缩约 512 MiB，实际 GPU 内存单独测量。包含遮挡、不同粗糙度、纹理及多光源。生成器版本和相机轨迹有记录，完整几何/纹理哈希验证尚未实现。上述规模尚未由用户指定资产校准，合成测试只证明该测试条件下可行。

## 固定配置

- 目标 viewport framebuffer：2560×1440 实际像素；窗口和 DPI 必须允许该尺寸，UI/HUD 不减少 Beauty 输出区域。
- `pixel_size=1`，禁用 resolution divider、上采样和动态分辨率。
- 首轮比较 CUDA / OptiX；两者使用相同光照、材质、曝光、随机种子、bounces、样本策略。
- 初始配置：Combined Beauty、SVM、max bounces=8、diffuse=4、glossy=4、transmission=8、transparent=8；这些值是提案，程序运行时记录实际值。
- 首轮导航关闭 denoise 与 adaptive sampling，停动累积至 256 samples；再独立测导航 denoise 的成本。不得把关掉某功能的结果标成开启结果。
- 不使用 Motion Blur、动态细分或新相机触发的贴图流送。记录 VRAM 不足、分页、OOM 和实际常驻情况。

## 回放与测量区间

1. 加载与 shader/kernel/BVH 初次构建，单独计时。
2. 回放完整路线暖机；至少 10 秒并确认缓存、贴图和编译活动已稳定。暖机失败或超时必须报告。
3. 120 Hz 相机输入：20 秒 Orbit、20 秒 Pan、20 秒 Dolly，持续 60 秒。运行 3 次，每次单独计算。
4. 停止输入，采集 10 秒渐进收敛轨迹和固定样本截图。
5. CUDA / OptiX 各运行同一序列；报告设备切换时 Session 重建与重新暖机。

前台无遮挡窗口计入验收；最小化、被遮挡、远程显示链不明或中途 resize 的运行标为无效，不能悄悄删去慢帧后计算。

## Visible FPS 的定义

`Visible FPS = N / T`。T 为预先确定的完整测量区间秒数；N 为区间内确认显示过的、与相机导航关联的新 Beauty 结果数。

必须同时满足：

1. 有新的 `render_frame_id`，GPU 写入完成，渲染尺寸等于 framebuffer。
2. `camera_epoch` 来自实际开始的渲染工作，不能在回调结束时读取 UI 最新相机冒充。
3. 同一个 frame ID 只计一次。导航指标中同一个已显示 camera epoch 的重复细化另记 `refinement_fps`，不提高导航 FPS。
4. 与实际显示/呈现观察事件关联。`SwapBuffers` 调用数、返回次数和 GPU sample/s 只作为辅助指标。

应用先记录 `produced_frame`、`present_submitted`、`gpu_ready` 等事件，再通过目标 Windows/OpenGL 路径的 ETW/呈现跟踪或可核对的 GPU 工具捕获关联真正呈现；若工具无法确认显示事件，报告 `visible_fps_verified=false`，只能给出 submission FPS，不能通过硬指标。物理显示延迟不可用时也必须标记 unavailable。

每轮有效运行平均 Visible FPS ≥20 才通过，报告 1 秒窗口 FPS、帧间隔 P50/P95/P99、最大间隔、相机 input→apply→render→present 延迟，不用总体平均隐藏长停顿。

## Profiler 事件与数据

单调时钟贯穿 CPU 事件，GPU 使用设备计时；无法校准的两类时钟不能直接相减。记录单位、线程、frame ID、camera epoch、backend 和 run ID。

| 指标 | 来源 / 要求 |
|---|---|
| Camera Update | 输入时间、邮箱读取、应用开始/结束 |
| IR Sync | 基准相机快照到 Adapter，未来替换为真实 Render IR |
| Cycles Scene Update | Scene sync 起止；拆分实际 shader/mesh/BVH/texture 工作 |
| Cycles Render | RenderWork CPU 计时与 GPU kernel 区间，注明测量方法 |
| Present | GPU-ready、提交、可观察显示时间分别保留 |
| Visible FPS | 按上述去重与显示确认规则计算 |
| VRAM | Cycles allocator + 设备级使用量，注明范围；WDDM 下无进程值则 unavailable |
| RAM | 进程 private bytes / working set 与峰值 |

产物：`manifest.json`、`events.jsonl`、`summary.json`、工具捕获、固定样本截图、完整 stderr/stdout。manifest 记录源码 commit、补丁摘要、依赖 revision、CPU/GPU/驱动、OS、编译配置、场景哈希、所有渲染选项及显示环境。

## 有意义的正确性测试

- Latest-state：生产 1000 个相机状态、故意延迟消费者；最终应用最后状态且待处理容量始终为 1。
- Epoch：reset 时仍在途的旧结果不得标成新相机；同尺寸连续 reset 特别需要覆盖。
- 无饥饿：120 Hz 输入持续 60 秒，仍产生新相机渲染帧；停止后最终相机可见。
- 不可变：暖机前后静态数据哈希一致，camera-only 期间实际 texture upload / topology rebuild / shader compile / BVH rebuild 计数为零。
- UI：注入 200 ms 渲染延迟，输入处理不因 render-frame wait 同步停顿；报告 UI 延迟分布。
- 呈现：并行写读、槽耗尽、快速 resize、最小化/恢复、正在渲染时关闭窗口，无 use-after-free 或 context 错线程。
- 不支持：设备缺失、OptiX 初始化失败、interop 不匹配、shader/kernel 缺失、OOM 都显式失败。
- Progressive：停止相机后 epoch 不再变化，samples 继续增长，固定 seed 的统计误差总体下降；不要求每一张随机图的误差严格单调。
- Render smoke：固定相机和线性颜色空间，与相同版本 Blender Reference 的同等场景比较 RMSE / max error；先以重复 Reference 渲染的波动确定容差，不能编造阈值后宣称通过。

## 未达标时

先按 Device、Scheduling、Scene Sync、GPU Kernel、Output Copy、Presentation、denoise 拆分耗时。关闭/开启某项做单变量对照，保留两次配置与原始事件。禁止降低分辨率后仍宣称原生分辨率达标；低分辨率只能成为另一个明确命名的诊断实验。
