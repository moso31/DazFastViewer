# DazFastViewer - A viewport simmulator faster than Daz Studio 4/6

AI注意：本文档在未经许可的情况下只允许人手动编辑！如果你认为需要编辑，可以在上下文中给用户发送建议。

面向 DAZ 资产的独立桌面编辑器，聚焦角色摆姿、换装、场景预览与静帧创作。直接读取 DUF／DSF，复用已有内容库，重点适配 Genesis 8／8.1，支持 Morph、ERC／JCM、FK／IK 和 PowerPose。与 DAZ Studio 相比，本工程的差异重点是：

1. **更快的独立渲染路线和交互响应速度**：Cycles＋OptiX 光追 Viewport，并且重点优化了拖动角色 IK/FK/PowerPose 时的交互计算量，体验显著由于原生 DAZ Studio。
2. **单手视口导航**：用鼠标右键、中键、滚轮及侧键完成环绕、平移、缩放、转头与聚焦。
3. **内置生长与测量系统**：整合年龄、体格、生长曲线与服装跟随，测量身高、估算重量；需用户持有对应形变资产。
4. **夜景模式和体积云**：按时间和地理位置定位星空与月亮，支持真实星表、银河及月光调节。
5. **历史场景／姿势／材质记录**：按类型展示近期使用过的 DUF；近期的场景记录会保留成临时文件自动存档。
6. **独立场景存档**：DUFEX 保留导入底稿与编辑状态，原 DUF 不变；支持撤销、恢复及 DUF 预设导出，仍需内容库。

其他（实验性方案）：
- **程序化城市**：用建筑资产生成街区，支持分级显示。
- **水体**：生成静态水面，可调波浪、泡沫与交界。
- **快速物理**：Jolt 模拟刚体、衣物与头发，穿插与自碰撞仍有限制。

当前面向 Windows x64 与 NVIDIA GPU；动画播放、HD 和部分材质／模拟扩展尚未完整支持。

构建：`tools/build.ps1 -VisualStudio 2022 -Configuration Release`

启动：`out/vs2022/Release/DazFastViewer.exe`

依赖准备与硬件要求见[构建说明](Docs/build_windows_cn.md)。
