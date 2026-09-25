# PowerPose 布局与模板页继承

2026-09-25。正常关闭程序后，下次启动继承 PowerPose 的停靠位置、可见尺寸、浮动状态、显隐、前台标签页及当前 Body／Hands／Head 模板页。

停靠布局继续使用已有的 `window/geometry`、`window/docks` 保存与恢复。此前缺少模板页持久化，本次新增 `powerpose/template`，按模板名称保存；旧设置没有此键时默认 Body，无效名称不会启用 Face。读写与主窗口布局一起发生，自动验证模式不写用户偏好。

用实际 `PowerPosePanel` 在两个独立的离屏进程中完成保存／恢复检查，覆盖：PowerPose 前台、属性面板前台且选中 Hands、隐藏且选中 Head、浮动位置与大小、移至左侧停靠及尺寸。五项均通过；使用临时 INI，没有改动用户设置。隐藏或非前台 Dock 的 Qt 内部离屏坐标不作为可见位置比较。

工程规定的 20 项 CTest 通过；本次没有修改渲染或姿态计算，不重复场景渲染测试。证据位于 `artifacts/009-powerpose/layout`，摘要见 [layout-persistence.json](evidence/layout-persistence.json)。

已部署 `out/DazFastViewer.exe`，SHA-256 为 `929890f8e94333a1d7af937a06e98c031f90460cf4375385b31c9fd7eb3968fe`，发布清单的 161 项源码哈希一致。启动新版、调整面板并正常关闭后，即会保存当前状态。本轮未创建 Git 提交。
