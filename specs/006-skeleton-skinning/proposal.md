# 006：骨架、蒙皮与姿势 DUF

用户于 2026-09-21 授权进入 006，并提供 Bed Hogs II 姿势目录作为蒙皮验收输入。沿用当前模型、Qt 中文编辑器、独立 C++ Runtime 和 Cycles 视口，不再请求已确认的模型或执行授权。

本阶段将角色从可编辑静态网格推进到可应用骨骼姿势的网格：读取 Genesis 8 / 8.1 的骨架和 General 权重，以资产声明的 DualQuat 蒙皮求值；加载单帧姿势 DUF，作用于当前选中角色。菜单、Explorer 双击、命令行共用姿势解析与应用逻辑。

用户指定目录：`H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female`。目录中的 40 个 DUF 包含普通与镜像姿势。基础 G8 和现有 G8.1 均需检查，不能将“读到文件”当成蒙皮验收。

006 是总规范 Phase 5 的骨架 / 蒙皮部分；007 继续实现 Formula / ERC / JCM。眼睛控制器、弯曲修正、体型驱动骨骼中心等依赖不能宣称已支持。FitTo、IK、多帧动画和完整 DAZ Golden 留在后续阶段；本阶段不需要新增第三方源码或服装资产。
