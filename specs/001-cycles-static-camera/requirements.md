# 需求与不可变条件

## 功能

| 编号 | 要求 | 验证 |
|---|---|---|
| R001 | 独立 exe 直接控制 Cycles；无需 Blender GUI、DAZ Studio、Iray | 检查进程及运行依赖 |
| R002 | 显式选择设备，记录设备 ID、UUID（可用时）、驱动、后端 | 请求 OptiX 失败必须报错，不能退回 CPU |
| R003 | Orbit / Pan / Dolly，支持确定性回放和实时输入 | 相机轨迹与矩阵测试 |
| R004 | 静态场景只创建一次；相机导航只修改相机状态和必要的累积缓冲 | 计数器、静态摘要、GPU trace |
| R005 | UI 不同步等待完整渲染帧 | 输入事件时间戳、延迟注入测试 |
| R006 | Latest-State-Wins；待应用相机最多一个 | 高频输入下无 FIFO 积压 |
| R007 | 相机停止后继续累积，不能永久停在导航样本数 | 固定随机种子截图与 sample 轨迹 |
| R008 | 原生 framebuffer 尺寸 ≥20 Visible FPS | 禁止用低分辨率放大帧通过验收 |
| R009 | 重复呈现旧图、HUD-only 帧、空白帧不计入有效 FPS | 帧 ID 与呈现事件去重 |
| R010 | 失败、未知能力、数据不可用均明确诊断 | 输出结构化 reason，不把缺失计数记作零 |

## P1–P9 映射

- P1–P3：相机路径无 DSON、Morph、Skinning 依赖。本 Spec 尚未链接这些模块，报告为 `not_linked`；未来 Spec 的运行时回归仍需重测。
- P4–P6：暖机后材质翻译、Shader 编译、静态 Texture 上传、Mesh/Topology 重建和 BVH 重建的实际工作计数保持零。进入 manager 函数但立即返回不能算重建。
- P7：UI 只写相机邮箱，不获取 Scene mutex、不等待 CUDA/GL fence、不调用 `Session::wait()`。
- P8：消费者取最新相机快照，旧版本可覆盖；呈现队列也有固定容量。
- P9：本 Spec 的静态输入保持不可变。产品 Render Scene IR 的不可变性在 Spec 002 再实现和验证。

## 首版渲染配置

Beauty 使用 Combined pass、SVM 材质、透视相机和固定光照。初始性能试验不启用自适应细分、Motion Blur、Temporal Denoising 或动态贴图流送；分别记录功能是否启用。停动累积上限和导航采样策略必须写进报告，不把降质参数隐藏起来。

窗口 resize / DPI 改变属于独立测试区间，需要重建输出尺寸，不能混入 camera-only 计时。
