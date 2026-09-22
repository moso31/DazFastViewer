# 实施状态

更新：2026-09-22。

最新原始采样复测：按用户要求关闭视口 / 离线降噪，采用蓝噪声采样；视口上限 4096，自适应阈值 0.01。15 秒原始画面有改善，用户要求的 Iray 无降噪收敛标准尚未验收，不能引用历史 3.43 秒降噪结果证明达标。13 项工程检查及离线输出通过，已部署，见[本轮记录](../Docs/raw_convergence_execution_cn.md)。

此前 test.duf 复测：HDRI 直接光采样、离线 PNG 色调、Fit To 位移、刚性表面附件、自身轴心旋转及无固定范围的数值编辑已修复。13 项工程检查和真实 DUF 跟随 / 恢复验证通过；完整 Iray Golden 和严格 FPS Gate 仍未验收。见[执行记录](../Docs/test_scene_repair_execution_cn.md)。008 未开始。

最新眼镜复查：补齐 Head 树层级、镜腿轴心及场景 Morph 限幅覆盖，修复 A 的 `Youth Morph=-0.2` 被错误截成 0 导致的头部 / 眼镜偏移。A / B 眼镜对用户新 DBZ 的 RMS 均约 0.0005 毫米，12 项回归和副屏层级 / 近景复核通过，已部署到 `out`，并按用户要求归档至本地 Git；详见[专项记录](../Docs/test3_glasses_followup_cn.md)。

最新：test3 黑皮肤、眼镜挂接、拖鞋贴合修复及环境 / 色调选项、分类参数面板、F / WASDQE 已部署。12 项 CTest、原始场景选项导航和异步 Morph 界面检查通过。局部穿插与完整 Iray 等价尚未验收，见[本轮报告](../Docs/test3_scene_repair_execution_cn.md)。008 未开始。

最新实际场景修复：`test.duf` 的 Visible 开关、35,638 根 StrandBasedHair、根层级 FitTo 拖鞋、头发 / 丝袜 / SSS / 薄壁眼部材质及 A 上衣碰撞已接入。10 项工程回归通过；完整材质 Golden 与任意姿势无穿插仍未通过，详见[验证报告](../Docs/scene_fidelity_execution_cn.md)。008 未开始。

最新视觉修复：真实场景的 ERC 抵消值、骨骼缩放补偿及基础形态恢复后，两参考人物头部与 DAZ DBZ 约 0.0002 毫米误差；服装跟随已改善，局部差异仍最高约 24 毫米。此前场景加载 PASS 不代表视觉验收，完整服装 Golden 仍未通过。见[形变修复报告](../Docs/scene_deformation_execution_cn.md)。

最新追加：模型 / 灯光删除、空场景、资源回收及复合 DUF 内容分派；真实 `3.duf` 补上刚性 Local / 冗余 scale 图兼容和内嵌几何 `source` 的 Morph / 骨架继承。见[删除与 DUF 执行报告](../Docs/scene_lifecycle_execution_cn.md)。008 未开始。

当前插入工作：007 结束、008 未开始；按用户授权实施[场景工作流优化](../Docs/scene_workflow_revision_cn.md)。独立记录于[执行报告](../Docs/scene_workflow_execution_cn.md)，不将这次 UI / DUF / 场景优化计为 008 FK / IK。

最新：007 后场景工作流、服装 Morph / 骨骼跟随、头部三级选取及绑定服装射线过滤已实现并部署，按用户授权归档到本地 Git；验证和剩余边界见[场景执行报告](../Docs/scene_workflow_execution_cn.md)及[服装跟随报告](../Docs/conform_follow_execution_cn.md)。008 未开始。

此前：007 基线已中文提交 `da5d4f8`，两批参数兼容性修复已提交为 `9a4d0fa`。G8 / G8.1 相对基线新增可编辑 476 / 614 项，第二批各追加 122 项；8 项轻量检查、真实参数 / 姿势、30 组数学对照与副屏流程通过。条件型条目及剩余问题见 [兼容性报告](007-erc-jcm/compatibility-report.md)与[完整清单](007-erc-jcm/remaining-morphs.md)。不再重复性能或全库测试。

| 项目 | 状态 | 证据 / 剩余项 |
|---|---|---|
| 用户授权与模型 | 先中文提交 006，再实施 007 并复测 005 | 已执行；沿用当前模型与 Qt Widgets |
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
| Spec 006 Skeleton / Skinning / Pose | **已中文提交 `aa8fbc7`** | [执行报告](006-skeleton-skinning/execution-report.md)；G8 / G8.1 各 40 姿势、独立数学与副屏验证通过 |
| Spec 007 Formula / ERC / JCM | **基线已中文提交 `da5d4f8`，后续两批兼容性修复已部署并按授权中文归档** | [兼容性报告](007-erc-jcm/compatibility-report.md)；Ren Yao / BGM 已修复；DAZ Studio Golden 待完成 |
| 已穿戴物 FitTo / AutoFollow | 007 后插入修复：原生 Morph、表面转移和骨骼跟随 | [跟随报告](../Docs/conform_follow_execution_cn.md)；跨代 AutoFit、平滑 / 碰撞另行实现 |
| FK / IK | 008 未开始 | [长期规划](../Docs/editor_and_genesis8_roadmap_cn.md)；G8.1 旧眼睛空覆盖保留，不强行套用 G8 控制器 |
| License / 发布依赖审计 | 部分完成 | 新增 JSON 为 MIT、已保留许可；整体发布审计仍未完成 |

用户要求停止重复性能测试，demo 只能在第二屏。本轮仅围绕实际资产和具体显示缺陷验证，不重跑整套基准；`tests/runtime_checks.py` 完整流程仍未执行。

现有材质参考与差异报告继续保留未完成项；005 已中文提交为 `ee2ba05`，006 为 `aa8fbc7`，007 基线为 `da5d4f8`。通用参数兼容性修复及插入优化已部署于 `out`，无需新增第三方源码。已使用 `H:/G1/Scenes/test2.duf` 完成四角色、八件服装的跟随与选取验证；服装平滑 / 碰撞等剩余边界见执行报告。
