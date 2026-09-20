# Spec 005：Morph Runtime 与可扩展中文编辑界面

实施状态：**已完成本阶段范围**，2026-09-21 通过[阶段完成核查](completion-review.md)。完整 Formula / 骨骼求值和整体产品 Golden 保留为后续工作。

用户于 2026-09-21 授权执行本阶段，并补充 Genesis 8 服装 FitTo、角色选择、Morph 与变换参数及长期编辑器需求。沿用当前模型，不重复请求确认。Spec 004 未完成的材质 Golden、严格 VisibleFPS 等仍保留未完成。

本阶段实现原生稀疏 Morph 发现、属性、Active Set、定向脏传播、顶点 / 实例 Delta，并提供中文 Qt Widgets 编辑器：菜单、内容浏览器、场景对象选择、Morph 搜索和滑块、位移 / 旋转 / 缩放。

Windows 原生控件方案经用户补充长期需求后调整为 Qt Widgets。使用本机 `C:\Qt\6.10.3\msvc2022_64`，与现有 MSVC ABI 一致；`C:\Qt\6.11.2` 为 MinGW 套件，不能与现有 MSVC 库混用。Cycles 视口保留独立渲染线程与 GPU 互操作，Qt 负责应用外壳与编辑操作。

标准基础网格 Morph 优先；HD、公式驱动、骨骼联动、服装自动跟随等能力不冒充本阶段已实现功能。后续计划见 [长期规划](../../Docs/editor_and_genesis8_roadmap_cn.md)。
