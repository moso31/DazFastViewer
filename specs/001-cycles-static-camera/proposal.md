# Spec 001：Cycles Static Camera Baseline

状态：设计草案，性能 Gate 未通过。

## 问题与产出

在实现 DAZ 兼容性前，需要证明当前目标机器能以独立 Cycles 程序承载静态场景，并在相机连续导航时达到原生视口分辨率 ≥20 Visible FPS。最终产出为 `CyclesViewportBench`、可重复场景、相机回放、逐帧事件和性能报告。

本轮遵循总体规范第 47 节，只进行源码研究、构建环境探测、集成与测量方案设计。当前提交的是可审查的设计资料，不能视为基准程序已完成。

## 范围

- Windows x64；目标 RTX 4070 Ti SUPER。
- 相机 Orbit / Pan / Dolly，静态几何与材质。
- 独立 C++ 应用，Cycles Beauty 输出、渐进收敛、呈现和性能 HUD。
- CUDA 与 OptiX 对照，最终选择以同一场景实测为准。
- 来源 commit、依赖、License / NOTICE、设备和完整渲染配置可追溯。

不在本 Spec 内实现 DSON、Morph、Skinning、DAZ 材质翻译、完整 UI 或产品级 Render Scene IR。基准中的静态场景描述是测试输入，不提前确定 Spec 002 的完整数据模型。

## 交付 Gate

1. 来源 Gate：版本、依赖与许可证记录完整。
2. 构建 Gate：全新构建目录生成并启动独立程序，显式识别所选 GPU。
3. 正确性 Gate：静态图像、相机交互、停动收敛、相机更新路径及退出生命周期通过测试。
4. 性能 Gate：按 [tests.md](tests.md) 运行原生分辨率基准，所有规定运行达到 ≥20 FPS。

任何 Gate 未通过均保留失败证据，不进入 DAZ 兼容性扩展。
