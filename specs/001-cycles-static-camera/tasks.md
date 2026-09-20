# 任务清单

## 本次研究

- [x] 读取总体规范并遵守 Spec 001 优先顺序。
- [x] 记录用户模型确认；未自动切换模型。
- [x] 检查本地 Blender 版本、Git 状态、v5.2.2 tag 对象。
- [x] 核查 Session / Scene / Device / DisplayDriver 及实际相机更新路径。
- [x] 核查独立构建与 Blender 内部 standalone 的差别。
- [x] 定位 GPL 依赖链；获取独立仓库，核查对应宽松许可证来源。
- [x] 比较独立 Cycles 与 Blender v5.2.2 的实际 blob，记录差异。
- [x] 以 Blender v5.2.0 做三方分类：34 项补丁变更、11 项独立差异、1 项合并评审；记录 SPDX 与候选处理方式。
- [x] 获取 Windows 依赖元数据，确认 gitlink 和部分头文件许可证。
- [x] 执行 CMake 配置探测，保留缺失 OpenImageIO 的失败结果。
- [x] 定义线程边界、camera-only 路径、输出生命周期与性能测量规则。
- [x] 建立 STATUS 与 HANDOFF。

## 开始代码实现前

- [x] 确认 Blender 版本命名并建立固定 v5.2.2 的参考工作树；运行时组装来源仍按下项验证。
- [ ] 按 third-party.md 完成实际使用范围的 LICENSE / NOTICE 审核；有 GPL、AGPL、未知或混合许可时遵守总体规范第 5 节的确认规则。
- [ ] 补齐实际 binary 依赖和 NVIDIA 开发组件，验证版本与工具可用性。
- [x] 核实 CUDA 12.9 / NVCC 12.9.41 可运行，OptiX 9.1.0 位于 `D:\Github\optix-dev`；GPU kernel 编译和兼容性尚未验证。
- [ ] 根据已完成的差异分类生成可重建的 Cycles 组装/补丁清单，并验证最终构建。

## CyclesViewportBench 实现

- [x] CMake 构建入口、固定来源与启动诊断。
- [x] 设备枚举；请求后端失败时不自动回退 CPU。
- [x] 静态 smoke 与 medium 场景生成。
- [x] 相机控制与 latest-state 邮箱；基础 CTest 通过。
- [x] Session 控制与实际 render epoch 观测。
- [x] Win32/WGL Presenter、GPU interop、输出槽与正常关闭。
- [x] HUD、事件日志、报告分析和固定轨迹回放。
- [ ] 运行 tests.md 的正确性测试。
- [x] 同一配置分别测 CUDA / OptiX，默认选择 OptiX。
- [ ] 三轮原生分辨率 ≥20 Visible FPS，保存可追溯证据。
- [ ] 本 Spec 通过后才为下一个 Spec 发起模型 Gate。

用户已要求停止重复性能测试；旧矩阵不再自动执行。严格显示事件、GPU 细分计数、Reference 对照及完整错误路径测试仍未完成，见 execution-report.md。
