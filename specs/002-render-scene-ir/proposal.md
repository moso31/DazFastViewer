# Spec 002：Render Scene IR 与 Cycles Adapter

更新：2026-09-20。状态：静态场景与相机增量路径已实现、已运行验证；材质编辑接口尚未进行图像验收。

将程序化场景和 DSON 输入统一为不依赖 Cycles 的渲染数据层。加载器只输出 IR，适配器负责 Cycles 对象、材质图、灯光与相机，使后续 DAZ 求值无需直接调用渲染后端。

用户已要求继续推进并提供两个真实 DUF，沿用本轮已确认模型。Spec 001 的严格 Visible FPS 仍未完成；本轮不再次开展性能验收，也不将窗口预览记作性能 Gate 通过。

范围包括静态 Mesh、Instance、逐角 UV、材质、贴图、预览灯光、相机，以及 Camera / Material Delta。动态拓扑、骨骼、Morph、完整场景热更新和材质 Reference 不在本次最小实现内。

实现、证据与剩余项分别见 [design.md](design.md)、[tests.md](tests.md)、[tasks.md](tasks.md)。
