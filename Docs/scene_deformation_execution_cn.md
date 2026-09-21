# 保存场景的脸型与服装形变修复

2026-09-21。用户确认参考截图使用 Diffeomorphic DAZ Importer 的 DBZ File 模式。属于 007 后修复，008 未开始。

## 参考与验收范围

输入为 `H:\G1\Scenes\3.duf`，参考为同目录 `3.dbz`。后台 Blender 5.2.2 / Diffeomorphic 5.2.0 独立导入，基础网格世界坐标与 DBZ 的最大差异小于 0.00051 毫米。没有把 Runtime 顶点写入参考模型，没有改动用户的 Blender 文档，也没有把 DBZ 顶点用作产品运行时的替代网格。

本轮只研究本机 Diffeomorphic 的行为与输出，未复制其 GPL 实现。旧 `blender_reference.py` 的 UNIQUE 模式用于材质对照，不能用于这次脸型 / 服装验收。

## 修复原因

- 保存值 `-1` 可以抵消 ERC 控制器贡献的 `+1`。发现参数和公式输入阶段提前限幅会把保存值变为 `0`，错误叠加整套脸型 / 体型。改为保留原始输入，只限制最终公式结果；预设应用及面板也保留这些抵消值。
- 非继承缩放的骨骼必须在父骨骼 orientation 坐标内补偿非均匀缩放。原算法在另一坐标系取逆，沿骨链产生剪切和位移误差。
- Figure 的保存缩放已包含在实例矩阵里，根骨骼 ERC 仍要在原始通道空间计算，再换算为相对缩放。例如保存值 0.635 加上 -0.01，结果应为 0.625。
- 附属 Figure 下的同名骨骼不能覆盖人物姿势；父链查找现在遇到其他 Figure 即停止。
- 骨骼挂接附件原先仅保留 Loader 的静态位置，眼镜没有随头骨的 ERC 变换移动。现按骨骼的场景实例身份绑定，复用蒙皮的骨骼矩阵计算相对保存姿势的变化，并传递给附件及其子对象。追加场景时重映射骨骼实例身份，避免同名骨骼跨角色绑定。
- 带基础顶点差值的 HD Morph 保留基础形态，并标注 HD 未支持。缺少附件输出时保留人物自身有效公式，并记录“部分支持”；有已解析输出却缺少输入的公式仍按失败隔离。补齐整数控制器求值。
- 单个最近三角形的绑定在腋下、腿间及远离皮肤的裙摆产生不连续位移。沿衣物拓扑平滑生成的位移场，不平滑基础几何或作者 Morph；每次从静止绑定重算，重置不累积误差。

通道及公式语义参考 DAZ 官方 [channel_float](https://docs.daz3d.com/public/dson_spec/object_definitions/channel_float/start) 与 [formula](https://docs.daz3d.com/public/dson_spec/object_definitions/formula/start)。

## 原生几何对照

单位均为毫米，使用同顶点编号的世界坐标比较。DBZ SHA-256：`087f1d69b0f11dedc022f3695ab6d9ec2f0dd6af5651e37e51e74f7d0129a13c`。

| 对象 | 修复前 RMS | 修复后 RMS | 修复后最大误差 |
|---|---:|---:|---:|
| lit | 35.664 | 0.070 | 0.787 |
| lit2 | 39.337 | 0.125 | 1.869 |
| 白色上衣 | 6.940 | 2.985 | 18.005 |
| 绿色长裙 | 10.905 | 8.242 | 23.727 |
| 黑色上衣 | 26.199 | 3.314 | 15.000 |
| 短裙 | 10.542 | 2.228 | 8.963 |

两人头部 RMS 分别从 7.952 / 9.246 毫米降至 0.000187 / 0.000241 毫米，达到浮点精度附近。身体少量差异主要分布在非头部区域，未宣称全身逐位一致。

后续实际渲染发现并修复了额头上悬浮的眼镜。补齐骨骼附件跟随后，Midday Glasses 的 8,440 个顶点对 DBZ 的 RMS 为 2.515 毫米，最大 4.654 毫米；仍有局部位置差异，不宣称逐点等价。此版本重新验证 lit 的全部 16,556 个顶点，上述身体与头部误差保持不变。证据为 `artifacts/geometry-attachments.json`、`geometry-attachments-comparison.json`。

证据：`artifacts/geometry-before-comparison.json`、`geometry-fixed3-comparison.json`。`geometry-fixed3.png` 左侧两人为 DBZ，右侧两人为原生求值，双方使用相同相机、光照与材质；不包含头发和纹理，专门观察几何。`artifacts/blender-geometry-reference.json` 是独立插件导入的结果。

## 回归与复现

`SceneWorkflowTest --geometry 输入.duf 输出.json lit lit2 "LSO Blouse A" "LSO Skirt" "Sweet Girl Skirt" "Sweet Girl Sweatshirt"` 导出原生基础 / Morph / 世界坐标、有效通道和骨骼参数。

`python tools/reference/compare_geometry.py 输出.json H:/G1/Scenes/3.dbz --output 对照.json` 独立读取 DBZ 并逐点统计。`blender_geometry_reference.py` 必须在独立后台 Blender 中以 `--factory-startup` 运行；`render_geometry_comparison.py` 生成上述几何对照图。

新增回归覆盖公式抵消值、带保存缩放的 ERC、缺失附件输出、带 orientation 的缩放补偿、嵌套 Figure 姿势隔离、生成跟随场的连续性及完整重置；骨骼附件覆盖旋转、ERC 缩放、子对象传播、手动实例变换、重复求值、重置、保存姿势不重复应用、追加角色隔离。追加 / 删除时还验证原始负输入没有被二次限幅。

最终 10 项工程 CTest 全部通过，构建与部署日志为 `artifacts/geometry-delivery-build.log`。日志包含上游 CMake `FindCUDA` 策略警告，PowerShell 因 stderr 记录返回非零；日志中的各构建目标、10 项测试和运行文件部署均已完成，并单独核对了构建产物与 `out/DazFastViewer.exe` 的 SHA-256 一致：`F6C78A7486DD566F0CEC89DAAF0BAC4685E3874862E9FB2A8D2D28E38FC86BDD`。

真实 `3.duf` 的完整场景显示验证在 `artifacts/geometry-delivery-ui`，37 个对象 / 31 套蒙皮。完整场景的正面视角被大角色遮挡，因此另外从原 DUF 生成仅保留 lit / lit2 及附件的测试副本 `artifacts/geometry-two-characters/scene.duf`，原文件没有改动。最终部署版本的双角色实际 Cycles 截图为 `artifacts/geometry-two-characters/final-ui/scene.png`；同目录 `editor-check.json` 显示 18 对象 / 18 套蒙皮、PASS，副屏窗口已退出。已人工查看这张截图，眼镜回到眼部，脸型及服装整体形态恢复；仍可见局部衣物穿插。

保存输入修复后另完成 2 轮实际 Cycles 生命周期复测，共 14 个状态，旧文档释放、空场景模型 / 网格 / 材质 / 贴图计数归零均通过；日志为 `artifacts/geometry-preserve-ui`。此前的 32 轮资源回收测试见生命周期报告。整轮证据索引为 `artifacts/geometry-delivery-summary.json`。

## 尚未等价的行为

人物脸型已对齐该场景的 DAZ 导出结果。服装整体跟随及异常皱缩已修正，但生成的 AutoFollow 仍是近似算法；长裙等局部最高约 24 毫米的差异和部分衣物穿插仍存在。DAZ 的专有平滑、碰撞与 dForce 未实现，不将这次结果称为完整服装 Golden。基础分辨率比较也不代表 HD、细分或材质一致。

本次保留之前的删除 / 资源回收修复；源码、回归测试、参考脚本与执行文档按用户要求以中文提交说明归档至本地 Git，提交记录见 `git log`。
