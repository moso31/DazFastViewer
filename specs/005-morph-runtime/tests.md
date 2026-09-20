# 验证计划

- 核心测试覆盖叠加、负权重、clamp、恢复零值、重复设置不求值、不同对象隔离、非有限输入和错误拓扑拒绝。
- Loader 测试覆盖 UTF-8、gzip、Morph 目录覆盖、同名不同目标、公式 / HD 诊断。
- 使用本机 Genesis 8.1 角色和现有兼容 Morph，直接比较原始 DSF 差值与 Runtime 求值结果；DAZ Studio 最终变形 Golden 暂未提供，不伪造通过。
- 实测一次副屏 Qt 操作：中文标签与路径、选择对象、Morph 滑块、变换、还原；记录顶点更新、实例更新和网格创建计数。
- 不重跑 Spec 001 性能轨迹。新编译或确切故障才重跑必要检查。

已执行结果见 [执行报告](execution-report.md)：三项 CTest、真实 DSF 数值、副屏 Morph / 变换 / 还原 / 相机以及后台场景替换均通过。DAZ Studio Golden 与严格 VisibleFPS 保留 NOT_RUN / 未完成。

多内容库修订新增 `morph_catalog` 与 `project_settings` 两项回归，及 Genesis 8 / 8.1 实际参数核对。多段 gzip 修复后重跑受影响检查；新参数树仅做一次副屏交互验证。最新证据与限制见 [多库与参数发现修复报告](content-libraries-report.md)。
