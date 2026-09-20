# Spec 005 首批执行报告

日期：2026-09-21。已完成标准稀疏 Morph 与 Qt 中文编辑器的首批实现；完整角色求值、服装绑定和 DAZ Studio Golden 尚未完成。

**阶段结论：** 多库修订后，005 当前约定的实施范围已完成，可按用户要求以中文归档到本地 Git。逐项依据和保留事项见 [阶段完成核查](completion-review.md)。下文保留首批实施过程的验证记录。

**后续修订：** 用户补充四个内容库后，已修复单库启动、纯控制器 / Alias 过滤和多段 gzip 读取。新增可保存的项目设置及参数分组树，最新结果见 [多库与参数发现修复报告](content-libraries-report.md)。下文 225 / 23 / 16 等数字是首批单内容库结果，不能代表目前多库清单。

## UI 决策与交付

依据用户对 Explorer、菜单、SceneHierarchy、加载角色、Morph、FitTo 等长期需求的补充，最终采用 **Qt Widgets 6.10.3 / MSVC x64**。已有 Win32 / OpenGL 代码仅承载 Cycles 视口，不作为应用 UI 控件框架。

新增独立 `out/DazFastViewer.exe`，包含中文菜单、可停靠面板、内容库浏览与切换、打开 DUF、场景对象选择、Morph 搜索与滑块 / 数值编辑、位移 / 旋转 / 缩放和重置。中文标签已在实际副屏截图中检查；Morph 原始名称保留。Qt 内置中文翻译与依赖已暂存。

文件解析与 Morph 扫描在后台进行。UI 发送带文档世代号的属性快照，渲染线程合并更新；场景替换时重新建立当前文档状态，旧对象的属性不作用于新对象。Qt 不直接操作 Cycles 网格，后续 FitTo、Undo、菜单命令可使用同一 Runtime 边界。

## Morph 与动态几何

- 解析 Morph 路径、目标、范围、标签、分组、可见性和 `auto_follow`，保留未支持项的原因。
- Genesis 8 → 8.1 兼容桥验证相同的三角形顶点编号；继承目录中的空覆盖文件具有屏蔽语义。
- 载入 DUF 中保存的直接 Morph 权重。每个可编辑实例保留独立基础网格、参数与 Active Set，重复设置同值不产生几何脏标记。
- 通过基础顶点加非零稀疏差值求值，不对上一次结果反复叠加；恢复零值无累计漂移。
- Render IR 新增顶点 / 实例 Delta；Cycles 原位更新网格，失效顶点法线并标记动态 BVH，不重建材质和纹理。
- 相机、实例变换与 Morph 分别传播脏标记；相机移动不进入 Morph 求值。

本机 Genesis 8.1 Female 共发现 **225 个具有稀疏差值的兼容 Morph**，其中 23 个可直接求值、16 个默认可见且可编辑；其余包含 Formula / ERC 或 HD，暂不作为可用直接 Morph。界面默认可见的不可用项为 51 个。不能将“发现”计数当作“完整支持”计数。

## 验证

| 项目 | 结果 | 证据 |
|---|---|---|
| MSVC Release 编辑器 / Runtime 构建 | PASS | `build/compile-editor-final.log`、`build/compile-editor-shell.log` |
| 三项轻量回归 | PASS | `camera_mailbox`、`scene_ir_dson`、`morph_runtime`，合计 0.12 秒 |
| 真实 Morph 数值 | PASS | Bodybuilder Size，10,905 个稀疏偏移；权重 0.5 / 1 / 0，与 DSF 直接计算的最大坐标误差为 0 |
| Morph / 变换 / 还原 / 相机 | PASS | `artifacts/editor-check/editor-check.json`；副屏内实际 Qt 滑块信号与属性控件操作 |
| 场景后台替换 | PASS | `artifacts/editor-reload/editor-check.json`；角色换为道具，文档世代号 2，未继承旧 Morph / 变换状态 |
| UTF-8 与范围 / 目标校验 | PASS | 合成中文文件路径、错误目标、NaN、clamp、已保存权重覆盖与多对象隔离 |
| DAZ Studio 完整变形 Golden | NOT_RUN | 当前是直接 DSF 差值与实际视口验证 |

副屏窗口位置为 `(2729,55)`、客户端 1580×920；原生视口 960×720。Morph 交互过程中网格始终为 2 个（角色 + 地板）、顶点更新 2 次、实例变换更新 2 次、Morph 求值 2 次；访问 10,905 个非零偏移，最终还原最大位移为 0。相机更新 2 次，未额外求值 Morph。GPU 互操作回读计数为 0；没有测量 GPU 上传字节或严格 VisibleFPS。

两次 GUI 检查分别覆盖 Morph 编辑与场景替换，不是性能回放。后续仅增加菜单入口和加载错误保留提示，执行了增量编译，没有重复同一渲染检查。

## 仍有明确边界

1. 首轮原生视口固定渲染尺寸；面板可以停靠 / 浮动，动态渲染尺寸、布局持久化与 DPI 全场景覆盖留待后续。场景树目前列出可渲染对象，尚不是完整骨骼层次编辑器。
2. 位移 / 旋转 / 缩放是相对载入状态的 DAZ 坐标偏移（厘米、角度、百分比），不是完整 DSON 父子层次重求值。服装 / 骨骼尚不自动跟随这些编辑。
3. Formula-only、Alias、ERC / JCM、HD、蒙皮及绑定姿态调整尚未求值；直接几何 Morph 不代表与 DAZ 最终带骨骼 / 修正结果等价。
4. FitTo 的数据契约、菜单流程和依赖顺序已规划，尚未提供可操作的服装绑定功能；DBZ 仅保留为兼容 / 对照选项，尚未实现导入。
5. 未实现编辑后保存 DUF、完整撤销 / 重做、Morph 磁盘索引缓存。当前内容库与资产保持只读。
6. Spec 004 材质差异、Spec 001 严格 VisibleFPS 与整体发布许可审计继续独立保留。

启动说明见 [README](../../README.md)，规划见 [中文编辑器与 Genesis 8 长期规划](../../Docs/editor_and_genesis8_roadmap_cn.md)。机器可读摘要位于 [evidence/summary.json](evidence/summary.json)，截图和商业资产相关报告仅保存在本地 `artifacts`。
