# 中文编辑器与 Genesis 8 长期规划

更新：2026-09-21，依据用户新增需求。目标是可持续扩展的桌面编辑器，支持内容浏览、菜单命令、加载角色、场景层次、Morph、服装 FitTo 和 IK；理论适配范围优先 Genesis 8，当前 Genesis 8.1 样例需按兼容关系验证。

当前 005 / 006 / 007 的实施范围已完成：Qt 参数编辑、蒙皮 / 单帧姿势以及 Formula / ERC / JCM 已接入统一 Runtime，007 对此前禁用的指定参数进行了实际变形复测。详见 [007 报告](../specs/007-erc-jcm/execution-report.md)。完整 DAZ Golden 仍未完成，服装绑定尚未实现；下一阶段 FK / IK 应直接修改同一骨骼输入，再由既有 ERC / JCM / 蒙皮管线更新几何。

## UI 技术决策

选择 **Qt Widgets + C++ Runtime + 独立 Cycles 视口**。Windows 原生控件可以实现功能，但停靠布局、模型视图、文件浏览、编辑器与命令系统的长期维护量更大。ImGui 适合轻量参数工具，但当前要求更接近完整资源和场景编辑器。

Qt MainWindow 与 DockWidget 提供可扩展面板，Model-View 用于文件与场景数据，QAction / 后续 Undo Stack 统一菜单、快捷键、工具栏及右键动作。界面持有选择与编辑状态，Runtime 持有真实场景状态，避免“换 UI 必须重写 Morph / FitTo”。中文使用 QString / Unicode、系统中文字体、原生输入法与 UTF-8 资产路径。

官方依据：[QMainWindow](https://doc.qt.io/qt-6/qmainwindow.html)、[Model/View](https://doc.qt.io/qt-6/modelview.html)。Qt 模块采用动态链接并保留许可证与来源记录，发布时继续完成依赖审计：[Qt Licensing](https://doc.qt.io/qt-6/licensing.html)。

## 功能演进

| 阶段 | 交付重点 | 为服装预留的契约 |
|---|---|---|
| 005 Morph / UI | 对象选择、属性、标准稀疏 Morph、变换和顶点增量 | 稳定对象 / 属性 ID、目标族谱、auto_follow、独立实例状态 |
| 006 Skeleton / Skinning | Genesis 8 骨骼、绑定姿态、权重与蒙皮；选中角色应用单帧姿势 DUF，以 Bed Hogs II 的 40 个姿势检验 | Figure 与服装独立 Mesh；ConformBinding 保存目标、骨骼映射、绑定矩阵及额外骨骼 |
| 007 ERC / JCM | Formula、依赖图、修正 Morph、循环诊断 | 身体 Morph → 服装 Morph；优先服装自带适配数据，再考虑经验证的转移 |
| FitTo 集成里程碑 | 菜单“服装 → 绑定到角色（FitTo） / 解除绑定 / 更换目标” | 基于稳定 ID 的可撤销命令，目标兼容性校验，不用简单父子关系替代蒙皮 |
| 008 FK / IK | 骨骼 FK、四肢 IK、约束与固定点 | IK 解算 → 目标骨骼 → JCM / 蒙皮 → 角色与所绑定服装的顶点 Delta |
| 009 及后续 | PowerPose、预设、完整 Explorer、保存与 Undo | 所有入口调用同一命令 / 求值系统 |

## FitTo 的具体边界

服装跟随包含三个不同问题：骨骼姿态跟随、体型 Morph 跟随、弯曲时的修正和防穿插。仅设置父节点不能完成这些语义。

优先适配同一 Genesis 8 家族且已有骨骼权重的 conforming clothing；保留衣服额外骨骼，不要求破坏性合并人体和服装网格。不同实例绑定到不同角色时分别保存关联和参数。跨性别 / 跨代 AutoFit、松垮衣物模拟、任意无权重网格自动绑定不作为首个 FitTo 里程碑的保证。

Diffeomorphic 作者的工作流把骨架重定向 / 合并与形态键转移分开；宽松衣物直接转移修正 Morph 可能产生错误，需要限制影响区域。这里只研究行为和导出结果，独立实现，不将其 GPL 实现复制进核心库。参考：[Changing Outfits](https://diffeomorphic.blogspot.com/2021/07/changing-outfits.html)、[Morph Transfer Progress](https://diffeomorphic.blogspot.com/2020/12/morph-transfer-progress.html)。

DBZ 用作离线拟合参考、评估后形态快照或受限兼容导入；不会直接替代持续的 Morph / IK / FitTo 求值图。若采用 DBZ，必须标明保存了哪些几何、姿态和绑定信息，以及仍可编辑的范围，不能把静态烘焙宣称为完整动态绑定。

进入 FitTo 实测前需要用户提供至少一套 **Genesis 8 Female（与当前角色性别一致）的服装 DUF 本地路径**，优先紧身上衣或裤装及完整内容库依赖；有长裙 / 额外骨骼服装可作为第二组。届时检查是否包含权重、适配 Morph、附加骨骼，并安排同服装绑定两个同代角色、改变体型、拖动手臂 / 腿部 IK、解除 / 更换绑定的用例。006 使用用户已提供的角色与姿势资产开展蒙皮验证，不等待服装资产。
