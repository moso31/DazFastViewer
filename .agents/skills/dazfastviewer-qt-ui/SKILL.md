---
name: dazfastviewer-qt-ui
description: Implement or review Qt parameter panels in DazFastViewer. Apply the user's persistent rules for click-selected wheel editing and a single outer scrollbar whenever adding or changing project UI. Not a general Qt styling guide or a rule for unrelated projects.
---

# DazFastViewer Qt 参数面板

用户固定要求（2026-10-06）：

- 数值框统一沿用 `src/editor/ground_panel.h` 的原生 Qt 格式。未选中、点击选中、悬停和获得焦点时，上下调节箭头的布局和绘制风格必须一致；不能因选中态 QSS 边框触发两个独立按钮。选中提示优先放在行标签，避免修改 spin box 的边框、内边距或 subcontrols。交付前对照地面对齐检查实际控件，覆盖选中前后状态。

- 支持滚轮调参，但必须先通过点击选中对应参数模块／行。悬停、自动获得焦点、Tab 焦点、打开面板均不等于选中。只有已选中行的数值控件或下拉框可以接受滚轮；其他位置的滚轮传给最外层参数面板，用来滚动页面。不要简单禁用所有滚轮调参。
- 除非用户明确要求，不做“滚轮中的滚轮”。参与对象等表格／列表按内容展开，放入外层参数布局，由最外层滚动条处理超出屏幕的内容。仅隐藏内层滚动条却仍截断内容或吞掉滚轮不算完成。普通临时下拉选项弹窗不属于嵌套参数面板。

实现时先检查已有参数面板的选中和事件转发方式（`src/editor/parameters.cpp`、`src/editor/ground_panel.h`）。点击其他行或面板外部、隐藏面板、切换对象时清除旧选择；同一对象数据刷新应保留有效选择。覆盖 spin box 的内部编辑器、标签、表格内下拉框及空白区域，避免子控件绕过限制。选中状态应可见。

用真实 Qt 事件验证：未选中时滚轮不修改数据且外层页面移动；点击后滚轮可调当前行；滚动其他行不改值；切换／隐藏后旧行不能继续滚轮改值；长列表最后一行可通过外层滚动条访问，内层不存在独立滚动范围。不要用只匹配源码或说明文本的测试代替行为验证。
