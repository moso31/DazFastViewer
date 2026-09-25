# PowerPose 布局与模板页继承

> 后续紧凑布局调整见文末；本节哈希属于布局继承功能首次交付。

2026-09-25。正常关闭程序后，下次启动继承 PowerPose 的停靠位置、可见尺寸、浮动状态、显隐、前台标签页及当前 Body／Hands／Head 模板页。

停靠布局继续使用已有的 `window/geometry`、`window/docks` 保存与恢复。此前缺少模板页持久化，本次新增 `powerpose/template`，按模板名称保存；旧设置没有此键时默认 Body，无效名称不会启用 Face。读写与主窗口布局一起发生，自动验证模式不写用户偏好。

用实际 `PowerPosePanel` 在两个独立的离屏进程中完成保存／恢复检查，覆盖：PowerPose 前台、属性面板前台且选中 Hands、隐藏且选中 Head、浮动位置与大小、移至左侧停靠及尺寸。五项均通过；使用临时 INI，没有改动用户设置。隐藏或非前台 Dock 的 Qt 内部离屏坐标不作为可见位置比较。

工程规定的 20 项 CTest 通过；本次没有修改渲染或姿态计算，不重复场景渲染测试。证据位于 `artifacts/009-powerpose/layout`，摘要见 [layout-persistence.json](evidence/layout-persistence.json)。

已部署 `out/DazFastViewer.exe`，SHA-256 为 `929890f8e94333a1d7af937a06e98c031f90460cf4375385b31c9fd7eb3968fe`，发布清单的 161 项源码哈希一致。启动新版、调整面板并正常关闭后，即会保存当前状态。本轮未创建 Git 提交。

## 紧凑布局调整

2026-09-25：根据用户截图，将 Template Set 与 Template 合并为同一行；顶部角色说明、底部控制点说明改为单行高度，左右键四项提示保留紧凑的两行。剩余高度交给模板画布。长说明使用省略号，完整内容保留在悬停提示中；固定文本行高避免选择控制点时画布跳动。

副屏使用实际面板与 Windows 字体检查 Body／Hands／Head，在 500×586 和 330×586 两种尺寸下，下拉框均同行、没有撑宽面板，画布高度均为 466 像素。检查包括已选择联动组的底部提示。截图和尺寸记录位于 `artifacts/009-powerpose/compact`；沿用现有布局与模板页持久化。本次只修改面板排版，不重复场景渲染验证。

20 项工程检查通过，含现有 680 个 Qt 方向手势检查。新版已部署到 `out/DazFastViewer.exe`，SHA-256 为 `9383c2d4c48de50d5fef899252f1fe6a60883d5dee4c378cebd81d5ab94ed7c9`，161 项源码哈希一致。摘要见 [compact-layout.json](evidence/compact-layout.json)。本轮未创建 Git 提交。
