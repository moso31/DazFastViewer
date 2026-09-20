# 设计与源码研究

状态：首轮设计完成，尚未构建渲染程序。以下 API 结论仅适用于记录的 commit，不改变总体规范的版本无关原则。

2026-09-20 后续更新：用户明确允许采用 Blender 5.2.2，版本命名歧义已消除。已在 `D:\Github\blender-5.2.2` 建立干净的独立工作树，分支 `dazfastviewer-blender-5.2.2`、commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`。原工作目录保持原状。远端 LFS 笔刷资产 404，检出时跳过二进制资产；这些文件不是当前 Cycles 源码研究的依赖。

CUDA 已核实为 `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9`，NVCC 12.9.41 可运行。OptiX 位于 `D:\Github\optix-dev`，实际 v9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d`；保留用户提供版本，先验证兼容性。下文环境表与 CMake 日志为首次研究时的记录，缺失状态以本段更新为准；没有据此声称已经编译或运行 GPU 渲染。

## 1. 来源与实际环境

| 来源 | 实际值 |
|---|---|
| 用户 Blender 目录 | `D:\Github\blender` |
| 当前分支 / HEAD | `main` / `b2e052b7172ac80708d1c5205dfc155365ce5d4f` |
| 工作目录版本头 | Blender 5.3.0 alpha |
| 用户确认的参考版本 | Blender 5.2.2 LTS，已建立独立工作树 |
| 本地 v5.2.2 对应 commit | `d13f752e3b9c4f8c261cda552b1021f8bcc0382c` |
| 独立 Cycles 研究来源 | `https://github.com/blender/cycles.git`，`v5.2.0` |
| 独立 Cycles commit | `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba` |
| Windows 依赖 gitlink | `60d6e96b917568278d400a4024c98da0fb777338`，两份来源相同 |
| 硬件 | Ryzen 7 5800X，约 64 GiB RAM；RTX 4070 Ti SUPER，16376 MiB，驱动 591.86 |
| 显示环境 | NVIDIA 输出报告 2560×1440、180 Hz；同时存在虚拟显示设备 |
| 编译环境 | VS 2022 17.14.23，实际 MSVC 19.44.35222，Windows SDK 10.0.26100.0，CMake 3.31.6-msvc6，Ninja 1.12.1 |

证据：[source-audit.json](evidence/source-audit.json)、[environment.json](evidence/environment.json)、[cmake-preflight.log](evidence/cmake-preflight.log)。显示器模式不等于实际窗口 framebuffer；程序必须记录实际像素尺寸与 DPI。

本地 Blender index 没有跟踪条目，`git diff --cached` 显示 20638 个删除项，工作文件实际存在。原因尚未确认。研究采用只读工作文件及 `git show v5.2.2:<path>`，不执行 reset / clean / checkout 覆盖用户目录。

Diffeomorphic 官方发布说明称插件 5.2.0 在 Blender 3.6 与 5.2 上测试，主要变更是 Blender 5.2 兼容性；这不能替代指定资产在 Blender 5.2.2 上的实际测试。[官方说明](https://diffeomorphic.blogspot.com/2026/08/diffeomorphic-add-ons-version-520.html)

## 2. 集成边界

Blender 根构建支持 `WITH_BLENDER=OFF` 和 `WITH_CYCLES_STANDALONE=ON`，对应配置在 `build_files/cmake/config/cycles_standalone.cmake`。`intern/cycles/app/CMakeLists.txt` 创建 `cycles`，链接 device、kernel、scene、session、bvh、subd、graph、util 等目标。

但是这条构建链仍会加入 `intern/guardedalloc`、`intern/sky` 等模块。已有 GPL 标记，详见 [第三方记录](third-party.md)。因此产品候选方案采用独立 Cycles 仓库的构建边界及其明确标注许可证的 third_party 来源，禁用 Blender 桥接与 guardedalloc。

**不能直接把独立 Cycles v5.2.0 当作 Blender 5.2.2。** 比较 `intern/cycles/` 与 `src/`：889 个同路径文件，843 个 blob 相同，46 个不同。差异包括 GPU shadow-path 调度、贴图缓存、闭包、材质与构建适配；完整列表在审计 JSON。独立仓库 `util/version.h` 甚至报告 5.3.0，而 Blender v5.2.2 内对应头报告 4.5.0，所以软件标签、渲染源码 commit 和补丁摘要必须分别记录。

候选组装方式：

1. Blender v5.2.2 的 `intern/cycles` 作为渲染算法基线，逐文件确认许可。
2. 独立仓库提供构建支持和 third_party；保留其宽松许可证文件，不能用 Blender 内同名文件覆盖。
3. 对 46 个差异逐一分类，保留必要的独立构建适配，例如 MSVC allocator 保护条件；记录补丁、来源与 SHA-256。
4. Blender 5.2.2 Reference 与组装后的 Cycles 跑相同场景、相机和采样设置，验证图像及数值差异。未完成前状态是候选，不能声称版本等价。

已进一步加入 Blender v5.2.0 做三方 blob 比较：**34 项为 Blender 5.2 补丁版本变更，11 项是独立仓库差异，1 项两侧都变化**。逐文件 blob 与 SPDX 记录在 [source-differences.json](evidence/source-differences.json)。独立差异中 `scene/mesh.cpp` 仅注释变化，`util/colorspace.cpp` 与 `util/nanovdb.h` 为旧依赖兼容条件，`util/guarded_allocator.h` 含 MSVC 容器代理保护。需要合并评审的 `util/image_maketx.cpp` 同时包含独立版 OIIO 兼容代码和 Blender 5.2.2 的 64 位乘法修复；固定当前 OIIO 3.1.13.1 后可优先采用 5.2.2 实现，但需补齐 fmt 依赖。以上分类不等于补丁已经实施或通过编译。

编译采用 C++20、MSVC x64。独立 CMake 部分设置仍出现 C++17 编译旗标，生成后检查最终命令行，不能只看 `CMAKE_CXX_STANDARD`。基准优先独立 Win32/WGL 窗口与 OpenGL Presenter，禁用上游 standalone GUI，从而避免将上游 SDL 窗口线程模型直接带入。OpenGL 函数加载可使用已核实的 libepoxy 依赖。

## 3. Device / Scene / Session 生命周期

以 v5.2.2 下的 `device/device.h`、`session/session.{h,cpp}`、`scene/scene.{h,cpp}` 和 `app/cycles_standalone.cpp` 为证据：

1. 枚举设备，匹配显式请求的 NVIDIA 后端和物理 GPU。OptiX 为优先实验项，CUDA 为同条件对照项；不自动改为 CPU。
2. 创建图形上下文与 Presenter 所需资源，再创建 `Session`；Session 拥有 Device、Scene、PathTrace 和内部控制线程。
3. 在渲染启动前构建静态 Scene、Shader、Geometry、Light、Camera 和 Combined pass，设置 DisplayDriver。
4. 调用 `reset(params, buffers)` 后 `start()`。`reset()` 发布 delayed reset 并请求取消当前 work，不表示这一刻 GPU 已应用新相机。
5. 控制线程通过 Scene mutex 修改 Camera，发布 reset，保持 Session 和静态场景生命周期不变。参照 Blender view_draw 的 `try_lock` 思路，但 UI 本身不承担同步操作。
6. 退出时先停输入与控制提交，终止/等待渲染工作，再释放 DisplayDriver 的 CUDA/GL interop，最后销毁 GL context 和窗口。不能先销毁上下文再销毁 Session。

`set_navigating()` 在这个版本只转发给 CacheEvictionManager，不能把它当作完整的交互采样开关。SceneParams 和 SessionParams 的 `background` 默认不同，初始化必须显式设置一致。

## 4. 线程与所有权

| 执行者 | 所有权与职责 | 等待规则 |
|---|---|---|
| UI thread | HWND、鼠标/键盘、相机控制状态、最新相机邮箱 | 不等待 Scene mutex 或完整 render frame |
| Render control | Session 生命周期、应用相机快照、reset | 可尝试获取 Scene mutex，失败保留最新状态 |
| Cycles 内部线程 | Scene device sync、GPU work、DisplayDriver 更新 | 遵守上游锁顺序 |
| Presentation thread | Present context、已完成纹理、HUD、SwapBuffers | 独立于 UI，不等待未完成渲染结果 |

基准输入类型限于 `CameraState`、不可变 `StaticSceneDescription`、`RenderConfig` 和 `FrameMetadata`。应用侧接口不暴露 `ccl::*`；后端独立编译单元负责翻译。这里不提前实现完整产品 IR。

相机邮箱包含单调递增序号和输入时间戳。消费者取快照并立即释放邮箱锁，不能持有邮箱锁调用 Cycles。绘制只消费最新已完成帧；停止输入时保留最后相机，让当前 Session 继续收敛。

持续每个输入都 reset 可能造成渲染饥饿。控制提交需依据 `ready_to_reset()` 与实测节奏合并输入，在保留最新状态的同时给 GPU 产生首张新结果的机会；不能用固定 20 Hz 提交频率掩盖后端上限。

## 5. Output / Presentation

`session/display_driver.h` 是交互输出接口，OutputDriver 用于离线输出，不能按完整离线帧回读实现视口。

- `update_begin` / `update_end` 标记一次显示数据更新。
- `GraphicsInteropBuffer` 接受 OpenGL PBO；CUDA interop 位于 `device/cuda/graphics_interop.*`。OptiX 的实际 interop 可用性必须在对应 Device 上验证。
- `map_texture_buffer` 是无 interop 时的诊断回退路径，必须记录 CPU readback 字节数和原因，不得隐藏。
- 上游 OpenGL 例子使用 context mutex 和 GL sync；用于理解协议，不能直接据此证明 UI 无阻塞。

候选实现使用 3 个固定容量输出槽，记录 EMPTY / WRITING / READY / PRESENTING 状态。生产者写入的 PBO 不得与消费者正在读取的资源重叠；无槽时可跳过显示更新并统计。GPU fence 未完成则继续呈现上一张完整图，重复呈现不计新帧。具体 CUDA map/unmap 与 GL fence 生命周期必须在原型中验证，不预先声称零拷贝达标。

**帧与相机的归属是必须解决的测试点。** DisplayDriver 回调没有 camera epoch，不能在 `update_end()` 读取最新 UI 序号后把旧渲染结果错误归到新相机。需要在 backend 实际应用 reset、开始 RenderWork 与完成 display update 的位置建立可追踪 epoch；如需小型上游观测补丁，只修改已审计 Apache 文件，记录补丁摘要。归属无法证明时，延迟/FPS 验收无效。

## 6. 原生分辨率与 Camera-only 路径

- `SessionParams.pixel_size=1`、`use_resolution_divider=false`；DisplayDriver 每帧检查有效宽高与目标 framebuffer 相等。
- 暖机前创建所有材质图、几何、贴图与加速结构；Camera-only 阶段只写 Camera、重置累积。
- 禁用自动贴图缓存生成与 tiled streaming 基准路径，使用完整加载的常驻纹理；源码 SceneParams 默认 `use_texture_cache=true`，不能忘记显式配置。实际上传计数必须证实 P5。
- 暂不启用相机依赖的自适应细分、Motion pass 和 Temporal Denoising；它们可能引入额外 geometry 更新。
- Scene manager 调用本身不等于资源重建：需要区分 dirty 检查和实际上传/重建。

RenderScheduler 初始显示间隔启发式含 0.1 秒值，首帧、reset 和导航路径另有规则。因此既不能推断必定被限制在 10 FPS，也不能推断必能到 20 FPS。第一轮 profiling 需要测 reset→first work→display update→present 全链路，必要时单独设计调度补丁再做 A/B 比较。

## 7. 构建结论（更新）

早期 OpenImageIO 缺失已解决。独立程序已完成 Release 构建、双后端 smoke 和原生分辨率窗口基准。`tools/prepare_cycles.py` 从固定 Git 对象生成 `.deps/cycles`，不修改用户提供的源码目录；具体结果见 [执行报告](execution-report.md)。

CUDA 12.9.41、OptiX 9.1.0 已验证可编译和渲染。使用 sm_89 目标与本机 RTX 4070 Ti SUPER；不将本机结果推广到其他设备。

实现采用独立输入/控制/呈现线程，Cycles 保留自身渲染线程。控制线程在 Scene 锁内消费最新快照；上一已应用相机产生结果后再 reset，避免高频输入取消所有工作。观测补丁在 Scene 锁内记录实际 RenderWork 的 epoch，不能使用回调结束时的最新 UI epoch。两个跨呈现/渲染线程的状态改为原子变量。

呈现采用稳定 PBO 与三个 RGBA16F 纹理槽，fence 未完成时保留旧画面。PBO→纹理有 GPU 内部复制。swaps.csv 记录 QPC、swap 序号、frame ID 与 epoch，供后续外部显示事件关联；仅 SwapBuffers 返回不足以通过 Visible FPS 验收。

## 8. 设计 Gate 与未决项

- Blender 5.2.2 参考来源、独立运行时组装与补丁清单已落地。
- 核实全部实际链接依赖的 LICENSE / NOTICE；仅 root LICENSE 不足以判断整套二进制。
- 剩余验证以执行报告为准；用户要求停止重复基准，不再按旧计划自动增加轮次。
- Medium 场景已实现为确定性合成数据，尚无用户指定 DAZ 资产，不能据此认证真实 DAZ 场景性能。
