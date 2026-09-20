# 实施状态

更新：2026-09-20。

| 项目 | 状态 | 证据 / 剩余项 |
|---|---|---|
| 用户授权与模型 | 已确认继续 Spec 002 / 指定资产的 Spec 003 | 不重复询问模型、Blender 分支或已经提供的依赖 |
| 版本基线 | Blender 5.2.2 对应独立工作树 | `D:\Github\blender-5.2.2`；原 checkout 保持原状 |
| Cycles 构建与运行 | Release 可运行 | 独立 Cycles v5.2.0 + 35 个 Blender 5.2.2 文件 + 项目适配；CUDA / OptiX |
| Spec 001 基准 | 已有历史结果，严格性能 Gate 未完成 | [执行报告](001-cycles-static-camera/execution-report.md)；PresentMon ETW 权限阻碍显示事件验证 |
| Spec 002 IR / Adapter | 静态加载与相机增量已验证 | [验证](002-render-scene-ir/tests.md)；材质 Delta 图像验收、完整上传计数待做 |
| Spec 003 指定 DUF | 两资产静态预览已跑通 | [执行报告](003-minimal-daz-loader/execution-report.md)；完整相机 / 变换 / 材质 Reference 尚未完成 |
| 副屏限制 | 已实现并实测 | `DISPLAY2`，窗口从创建开始在副屏，不抢主屏焦点 |
| 轻量回归 | `scene_ir_dson` PASS | JSON / gzip、UTF-8 URI、实例、UV、变换、非法数据拒绝 |
| 角色导航 | PASS | 相机更新 4 次、网格只创建 2 个、最终 epoch 一致 |
| 用户手动验收 | **已通过** | 用户于 2026-09-20 确认当前角色与道具预览通过；不扩展为未实现功能或严格 FPS 验收 |
| Spec 004 后续材质 Reference、Morph / Skeleton | 未实现 | 继续依据总体规范推进，不能据本次预览宣称完成 |
| License / 发布依赖审计 | 部分完成 | 新增 JSON 为 MIT、已保留许可；整体发布审计仍未完成 |

用户要求停止重复性能测试，demo 只能在第二屏。本轮仅围绕实际资产和具体显示缺陷验证，不重跑整套基准；`tests/runtime_checks.py` 完整流程仍未执行。

下一阶段应建立材质 Reference 和差异报告，明确皮肤 / 眼睛、法线 / 粗糙度等差异，再逐项补齐。当前不需要用户再获取额外第三方源码。
