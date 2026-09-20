# Spec 003：指定 DUF 的最小静态加载

更新：2026-09-20。状态：用户提供的角色与道具已加载并渲染，通用 DSON 兼容性和 Reference 验收未完成。

将用户指定的 Genesis 8.1 Basic Female 与 ARK Food Plate with Fries 直接加载到独立 Cycles 程序，贯通内容库寻址、DUF / DSF、Mesh、逐角 UV、静态实例变换和基础材质。运行时不启动 Blender、DAZ Studio 或 Iray，不借用 Diffeomorphic 实现代码。

这是指定资产的静态基础网格预览。人物的 SubD、HD Morph、蒙皮 / Pose，以及完整 Iray Uber 着色仍需后续实现。总体规范 Phase 2 中的 DAZ 相机导入尚未实现，不能把整个 Spec 003 标为最终完成。

验证遵守用户的副屏限制；只针对实际故障修正复查，不运行 Spec 001 性能轨迹。
