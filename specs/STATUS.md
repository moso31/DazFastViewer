# 实施状态

更新：2026-09-21。

| 项目 | 状态 | 证据 / 剩余项 |
|---|---|---|
| 用户授权与模型 | 已确认进入 Spec 006，补充姿势 DUF 蒙皮验证需求 | 沿用当前模型与 Qt Widgets；Bed Hogs II 共 40 个姿势 |
| 版本基线 | Blender 5.2.2 对应独立工作树 | `D:\Github\blender-5.2.2`；原 checkout 保持原状 |
| Cycles 构建与运行 | Release 可运行 | 独立 Cycles v5.2.0 + 35 个 Blender 5.2.2 文件 + 项目适配；CUDA / OptiX |
| Spec 001 基准 | 已有历史结果，严格性能 Gate 未完成 | [执行报告](001-cycles-static-camera/execution-report.md)；PresentMon ETW 权限阻碍显示事件验证 |
| Spec 002 IR / Adapter | 静态加载、相机增量及材质 Delta 已验证 | [Spec 004 验证](004-material-reference/execution-report.md)补齐材质 Delta；GPU 上传字节仍待做 |
| Spec 003 指定 DUF | 两资产静态预览已跑通 | [执行报告](003-minimal-daz-loader/execution-report.md)；完整相机 / 变换 / 材质 Reference 尚未完成 |
| 副屏限制 | 已实现并实测 | `DISPLAY2`，窗口从创建开始在副屏，不抢主屏焦点 |
| 轻量回归 | `scene_ir_dson` PASS | JSON / gzip、UTF-8 URI、实例、UV、变换、非法数据拒绝 |
| 角色导航 | PASS | 相机更新 4 次、网格只创建 2 个、最终 epoch 一致 |
| 用户手动验收 | **已通过** | 用户于 2026-09-20 确认当前角色与道具预览通过；不扩展为未实现功能或严格 FPS 验收 |
| Spec 004 材质 Reference | 首批交付完成，阶段进行中 | [执行报告](004-material-reference/execution-report.md)；结构一致，材质 Golden 尚未通过 |
| Spec 005 Morph / 中文 UI | **当前阶段实施范围已完成** | [完成核查](005-morph-runtime/completion-review.md)；六项核心要求、Qt 中文 UI 与多库设置齐备，整体 Golden 仍未完成 |
| Spec 006 Skeleton / Skinning / Pose | **本阶段实施范围已完成，待用户手动验收** | [执行报告](006-skeleton-skinning/execution-report.md)；G8 / G8.1 各 40 姿势、独立数学与副屏验证通过 |
| ERC / JCM / FitTo / IK | 后续能力，尚未实现 | [长期规划](../Docs/editor_and_genesis8_roadmap_cn.md)；姿势中的眼睛 ERC 非零值明确报告未应用 |
| License / 发布依赖审计 | 部分完成 | 新增 JSON 为 MIT、已保留许可；整体发布审计仍未完成 |

用户要求停止重复性能测试，demo 只能在第二屏。本轮仅围绕实际资产和具体显示缺陷验证，不重跑整套基准；`tests/runtime_checks.py` 完整流程仍未执行。

现有材质参考与差异报告继续保留未完成项；005 已中文提交为 `ee2ba05`，006 骨架 / 蒙皮和单帧姿势已实现并部署。Qt 6.10.3 MSVC 套件本机可用，无需补充源码。进入 Genesis 8 服装绑定验证前，再收集同代服装 DUF 与内容库依赖。
