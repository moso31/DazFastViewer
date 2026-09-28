# Jolt Physics

- 上游：https://github.com/jrouwe/JoltPhysics
- 版本：v5.6.0
- 固定提交：`e77f175595e64cb44218cc9d9d56fc365ad0e36a`
- 许可证：MIT，原文见 `LICENSE.MIT`。
- 用途：CPU 刚体及软体约束解算；与 Cycles 材质无关。
- 获取：`tools/ensure_jolt.py`，由 `cmake/physics.cmake` 调用；默认位于 `.research/JoltPhysics-v5.6.0`。
- 构建：静态链接、动态 MSVC CRT，不启用 GPU Compute、调试绘制或性能分析器。
