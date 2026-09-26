# test9 头部特写性能调查

日期：2026-09-26。目标为 `big_01`、`Seo Hyun G8.1F`、`Seo Hyun G8.1F (2)` 和 `lit2` 的头部特写。实际后端为 Cycles / OptiX；项目没有调用 NVIDIA Iray SDK。

## 用户复测后的更正

**尚不能称为完整修复。** 用户确认诊断过程中看到了明显加速，但交付程序对 Seo Hyun 仍约半秒刷新一次。此前的 114 ms 是关闭自适应采样、594 × 835 视口下的路径追踪墙钟均值，不能用作正常启动的呈现间隔承诺。下面保留首轮证据，不将其当成用户场景已通过验收。

在用户保持运行的 PID 32948 上，只读核对到程序 SHA-256 与首次交付一致；不是旧版本。其日志 `artifacts/editor-20260926-205619-986/events.csv` 的固定相机 epoch 10、1174 × 975 渲染尺寸，共有 591 个单样本批次、295 次呈现：采样耗时中位数 **308.3 ms**，呈现间隔中位数 **621.1 ms**。另一些相机记录出现秒级采样，不能把这些区段擅自归给 Seo Hyun；当时日志没有记录角色和相机坐标。进程同时有约 2.49 GiB 共享 GPU 内存，不能仅凭这个计数判定具体纹理访问成本。

源码确认诊断与产品存在一个影响反馈的差异：吞吐实验关闭自适应，产品默认阈值为 0.01。`RenderScheduler::work_need_update_display()` 在下一批采样**开始前**判断距离上次呈现的时间，没有计入即将执行的采样。因此在上一帧刚呈现、单个样本已经超过刷新目标时，仍跳过下一帧；日志中的 591／295 正符合该路径。修正让交互调度计入下一批的预计采样时长，保留自适应采样与全部材质参数。它修正额外呈现等待，**不降低单个样本的路径追踪成本**。

后续诊断默认与产品一样启用 0.01 自适应；基线只移动相机，不再无条件经过材质／可见性消融同步。新增 `--render-profile-start-empty` 可覆盖“先渲染空场景，再打开文件”的普通加载顺序。普通窗口每两秒写入 `render-state.json`，记录相机、实际尺寸、采样配置和设备／host-mapped 分配，同时刷新事件日志，便于在运行中复查。

本轮按用户要求保留所有既有程序，不要求独占 GPU。受限环境的小场景测试使用从 test9 提取的 Seo Hyun、512 像素纹理限制及 `CYCLES_CONCURRENT_STATES_FACTOR=0.125`；这些仅用于减少诊断进程的额外显存，不能作为完整场景的画质或吞吐验收。原始日志与修复检查记录在 `artifacts/render-performance/live-followup/`。

修正版本完成 `tools/build.ps1` 的 25 项检查，包括新增的真实 `RenderScheduler` 回归：500 ms 单样本、自适应保持开启时下一批需要呈现；快速样本仍节流；无自适应、headless 和未启用自定义目标的分支保持预期。首次 CTest 因新测试缺少运行时 DLL 搜索路径失败，补齐测试环境后完整重跑通过；没有将失败那次记为通过。

额外 GPU 测试 PID 5340 达到 480 秒超时仍停在初次 `Sample 0/4096`，未取得可计量样本。先请求关闭测试窗口；退出未完成后，只终止这个已核对命令行的诊断进程，未关闭用户 PID 32948 或其他原有程序。**本轮没有完成真实 GPU 的修复前后对照，也没有验证完整 test9 的最终收敛速度。** 首轮 114 ms 不可据此恢复为验收结论。

修正已部署 `out/DazFastViewer.exe`，SHA-256 为 `2351b37101539abfd7e820c6d49e8584d54e96c07f46f7bb7ec3ca51cef0628f`，清单中 203 个源码哈希一致。已运行的窗口仍使用旧映像，用户自行重启后才加载新版。验证摘要为 `live-followup/validation.json`；完整构建日志为 `live-followup/build-final-verified.log`。

## 测量方法

使用本机 RTX 4070 Ti SUPER（16 GB），直接读取 `H:/g1/Scenes/test9.duf`。新增 `--render-profile` 在一次场景加载内切换指定角色头部，保持每个角色的相机、594 × 835 渲染尺寸、种子 1337 和原有反弹参数一致。禁用降噪；吞吐实验关闭自适应采样，避免把已经停止采样的像素算成吞吐提升。

`events.csv` 区分路径追踪、显示上传、场景同步和实际呈现；新增采样索引后，可以计算各渲染批次的样本增量。这里的“每样本时间”是 `path_trace` 的 CPU 墙钟时间（包括 GPU 执行与等待），不是 GPU 内核计时器。加载、编译、导航预览和场景修改排除在稳定相机的指标外。

第一轮 27 个顺序消融实验使用原来的资源上传策略和调度。其作用是定位线索；修改材质会改变纹理占用和分配顺序，不能把这些消融比值直接视为某个内核的纯成本。后续独立进程对比隐藏资源裁剪和 2K 纹理，以区分内存因素。

早期 `baseline-lit2` 沿用旧 `--capture-head`，受到最小相机距离 0.35 m 限制，画面没有铺满头部，且尺寸不同；不能纳入正式头部特写的性能对比。第一轮 `Lee 8` 的自动构图也未铺满头部，不纳入角色性能比较。

## 原始画质对照结果

| 角色 | 原策略每样本 | 隐藏资源裁剪＋短批次 | 本次观测倍率 | 新版呈现间隔中位数 |
|---|---:|---:|---:|---:|
| big_01 | 729.2 ms | 74.0 ms | 9.85× | 46.7 ms |
| Seo Hyun G8.1F | 927.3 ms | 114.0 ms | 8.14× | 109.1 ms |
| Seo Hyun G8.1F (2) | 322.3 ms | 66.4 ms | 4.86× | 63.8 ms |
| lit2 | 625.0 ms | 77.0 ms | 8.12× | 77.9 ms |

证据为 `artifacts/render-performance/sweep-original/analysis.json` 和 `pruned/analysis.json`。原版按每视角 12 秒或至少 64 样本结束，新版为 15 秒或至少 128 样本；按完整批次统计，样本数可能超过目标。均关闭自适应，除相机外所有可见对象、完整纹理和材质参数不变。倍率是本机这几次运行的观测，不是所有场景／显卡的保证；样本窗口长度不同、首次纹理访问及系统负载会造成波动。

本轮新版的统计几何从 8,563,890 降到 7,963,058 个三角形，曲线从 35,638 降到 0。减少的是隐藏资源；原场景的 15 个隐藏对象包含 328,438 个基础三角形和全部 35,638 条曲线，经过细分后的 GPU 数量与基础数量不同。可见 `HYLongHair` 仍保留。

该进程 Cycles 自报 GPU 分配约 13.21 GiB、host-mapped 约 1.69 GiB，仍有系统内存访问空间可以继续优化。这里的 GPU 分配计数不等同于 WDDM 任一时刻实际驻留显存。

独立的“仅缩短批次、继续上传隐藏资源”复测仍有约 4.14 GiB host-mapped 分配，big_01 和第一位 Seo Hyun 甚至分别约 3.05、4.17 秒／样本，见 `scheduler-only/`。这说明不能把提速归功于单独调整显示刷新参数；显存紧张时更频繁的小批次也可能损害吞吐，必须和资源裁剪一起评估。该反例保留在证据中，没有用它替代较快的原始基线来放大收益。

## 已确认的问题

### 静止显示更新主动放宽到两秒

`RenderScheduler::guess_display_update_interval_in_seconds_for_num_samples_no_limit()` 将交互视口更新目标按累计渲染时间从 0.1 秒依次放宽到 0.25、0.5、1、2 秒。每批计算多个样本后才更新显示，因此“画面两秒动一次”和“每个样本两秒”是不同指标。

新增可控更新目标 `update_interval_seconds`；0 保留上游行为。它只是调度目标，单个样本本身过慢、批次取整和系统调度仍可能超过这个时间，不能保证固定 FPS。

### 场景已经超过显存舒适区

原版头部测试的 Windows GPU 进程计数记录了约 14.1 GiB 专用显存和 4.1 GiB 共享内存，总提交约 18.2 GiB，见 `artifacts/render-performance/original-memory.json`。专用和共享内存计数不能单独证明所有共享数据都在被频繁访问，但足以证明不能继续假设整个场景完全驻留显存。

Cycles 的 CUDA / OptiX 显存不足时会使用系统内存，这会影响性能，见 [Blender 官方 GPU 渲染说明](https://docs.blender.org/manual/en/3.5/render/cycles/gpu_rendering.html)。本项目还新增了 Cycles 自身的设备分配与 host-mapped 字节数记录，用来与 Windows 计数交叉核对。

### 隐藏对象仍进入 OptiX 加速结构

导入器正确继承 `Visible` / `Renderable` 以及父级显隐，适配器也把隐藏对象的光线可见性设为 0。问题在于同步阶段此前仍为它们创建网格、曲线、材质和纹理，而且不能假设零可见性已经把它们从所有求交工作中排除。

本项目固定源码中 `scene/object.cpp` 的 `Object::is_traceable()` 只排除灯和无效包围盒，没有检查可见性；`device/optix/device_impl.cpp` 创建 TLAS 实例时将低 8 位为零的 mask 改为 `0xFF`。`kernel/device/optix/bvh.h` 中 SSS 的 `scene_intersect_local()` 使用场景 TLAS 和 `0xFF`，之后才在 `__anyhit__kernel_optix_local_hit()` 中忽略曲线及非目标对象。**隐藏发丝可以不出现在最终画面中，却仍增加包围盒遍历、求交及过滤成本。** 这是本地源码确认的路径；本轮没有用内核级计数器将它与纹理内存成本分别量化。

`big_01` 的 `FE Low Ponytail Base Hair` 在 DUF 中确实隐藏，子级会继承；不能据此声称隐藏发丝仍直接接受光线着色。但是整场景仍创建了 35,638 条曲线，隐藏资源可能挤占其他可见对象需要的显存。`HYLongHair` 与 `Hua Hair Scalp` 是另一些对象，需要分别检查。

资源裁剪跳过完全隐藏对象的 GPU 几何，并为没有可见面／曲线引用的材质保留不含纹理的占位着色器。原始场景与资源索引保留；重新显示时通过完整增量同步恢复。共享实例只要仍有一个可见使用者，就保留其资源。GeoGraft 仍按组合处理。编辑器默认启用裁剪和 0.125 秒更新目标；诊断可以恢复原策略。

### 皮肤与透明头发都可能增加成本

头部特写使皮肤、重叠透明发片、眉毛和眼睫毛占据大部分像素。皮肤映射包含 Random Walk SSS、双高光和凹凸／法线；透明最大反弹为 32。第一轮中，`big_01` 去掉可见头发并没有变快，反而暴露了更大面积的皮肤。直接删除头发或统一降低透明反弹不足以解释、修复所有角色。

关闭 SSS、关闭凹凸／法线、隐藏头发、隔离角色和透明反弹降至 8 仅作为诊断组，不作为保持画质的优化。画面之外的对象仍可能参与反射、阴影、透射和间接照明，不能按相机视锥直接删除。

## 显存充分时剩余的成本

额外的 2K 纹理诊断将 CUDA 设备分配降至约 5.14 GiB，host-mapped 为 0。`big_01` 每样本进一步降到约 15.0 ms；另外三个角色分别约 115.3、69.1、79.6 ms，与保持原图的裁剪版接近。说明减少显存压力对部分角色仍有帮助，但不是剩余问题的统一答案。2K 会降低纹理细节，未设为默认。

在这个没有 host-mapped 分配的组中，关闭 SSS 后，Seo Hyun 第一位从约 115.3 降到 24.7 ms，lit2 从约 79.6 降到 24.9 ms；关闭凹凸／法线分别约 94.2、74.0 ms。此时皮肤 SSS 是更明确的优化重点，不能再简单归因于头发太复杂。取消 SSS 会改变皮肤，不是本轮正式修改。

后续保持画质的候选方向：先用 GPU 内核计时确认 SSS 局部求交占比，再验证能否对当前对象／GeoGraft 组合的 BLAS 直接查询，减少“整个场景查询后过滤其他对象”的成本。该方案需要正确处理坐标变换、实例和蒙皮更新，不是现成参数；当前没有实现或宣称收益。

## Tile 和异步方案

- Cycles GPU 已通过 `WorkTileScheduler` 按工作块分配路径。`use_auto_tile=false` 不代表 GPU 完全没有分块。
- 离线图像 tile 主要降低高分辨率输出缓冲占用；当前约 50 万像素的视口，主要资源压力来自全场景几何、纹理和路径状态。把输出切成小块不会自动释放场景纹理。官方对 tile 的定位及内存取舍见 [Blender 性能设置](https://docs.blender.org/manual/en/4.5/render/cycles/render_settings/performance.html)。
- 更值得研究的是纹理分块与按需驻留。当前所集成源码已经有 `.tx` / mip / image cache 路径，但项目为早期常驻纹理基准显式关闭了它。需要单独验证缓存生成、预算、视角切换缺页、画质和缓存失效；本轮没有把未经验证的纹理流送作为默认值。
- Qt、渲染工作线程、Cycles Session 和三槽 GPU 显示队列已经分离，正常显示使用 CUDA / OpenGL interop。再增加一个 CPU 线程不会解决显存不足或 GPU 路径过慢；应先减少资源压力和不必要的大采样批次。

## 复现入口

实验计划是 JSON 数组，每项包含 `target`，可选 `seconds`、`samples`、`disable_sss`、`disable_bump`、`hide`（对象标签数组）、`isolate`、`transparent_bounces`。所有消融只作用于诊断进程中的 Cycles 场景，不保存到 DUF / DUFEX。

```powershell
& .\out\DazFastViewer.exe --file 'H:\g1\Scenes\test9.duf' --render-profile configs/render-profile.example.json --sampling-settings configs/render-profile-sampling.example.json --output artifacts/render-performance/reproduce
python tools/analyze_render_profile.py artifacts/render-performance/reproduce
```

采样配置支持 `prune_hidden`、`update_interval_seconds` 和 `texture_limit`；`texture_limit=0` 保留原始纹理。2K 上限会改变纹理细节，不能称为无损加速。

复制示例采样配置，将 `prune_hidden=false`、`update_interval_seconds=0` 可恢复本轮原资源策略。示例现在与正式编辑器一样使用自适应阈值 0.01；需要重现上表的吞吐实验时，另外显式设为 0，并保持相同尺寸、相机和负载。不要把两种模式的样本计数或呈现间隔混为一谈。

## 验证与交付

- `tools/build.ps1` 完整构建、规定的 24 项工程检查和运行时部署通过，日志为 `artifacts/render-performance/build-final.log`。
- 新增普通对象隐藏／恢复并同时修改 Morph、完全隐藏原型但保留可见实例等检查。原策略和资源裁剪各跑 17 阶段 OptiX GeoGraft 回归，全部通过；17 对 PNG 逐像素相同。证据为 `graft-{baseline,pruned}/` 和 `graft-image-comparison.json`。
- 从 test9 只读提取 big_01 及挂接物，正式发布程序实际执行 `FE Low Ponytail Base Hair` 显示／隐藏往返，检查通过：保持一个 Session，仅一次几何恢复，最终曲线数回到 0；默认自适应 0.01、原始纹理、SSS 保留、降噪关闭。证据为 `visibility-release/`。提取前后原始 test9 的 SHA-256 相同。
- GPU 实验串行执行。工程编译／CPU 检查部分与首轮实验同时进行，因此没有把本次数字描述为隔离机器、多轮统计的专业 GPU 基准。
- 新版部署至 `out/DazFastViewer.exe`，SHA-256：`c55d10a1c2ee0a151b00b1dd86381edac2c8d69a0e679fb5ffe89dccd0799eb2`。部署清单中的 202 个源码文件全部一致。原始 DUF 未写入，未创建 Git 提交。

初次宽泛运行全部 CTest 时，上游 `cycles_version` 因没有构建独立 `cycles.exe` 被标记为 Not Run；随后按项目 `tools/build.ps1` 规定的 24 项测试完整通过。没有将未运行项宣称为通过。
