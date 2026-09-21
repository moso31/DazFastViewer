# 007 后插入优化：穿戴物 Morph 与骨骼跟随

日期：2026-09-21。用户反馈人物可以正常变形，但服装没有同步变化。本次补齐已穿戴物的 `conform_target` / AutoFollow 求值，主线仍为 007 结束、008 未开始。

## 原因与修复

之前仅加载了服装自己的 Morph、骨架和保存姿势，丢失了 DSON 的 `conform_target`。父子对象的移动传递无法代替体型和骨骼跟随，因此人物改动后衣服仍保持自己的形状。

- Loader、参数目标及文档合并保留明确的 `conform_target`，以场景实例解析目标。重复导入场景时重映射引用，普通 parent 关系不会被误当 Fit To。缺失、歧义或成环的目标明确报错。
- 按依赖顺序先求值角色，再求值穿戴物，不依赖文件中的对象排列。服装优先使用同 channel ID 的原生适配 Morph，读取人体的最终 ERC / JCM 结果；不按显示名称或顶点数量猜测绑定。
- 未提供原生适配的可求值 `auto_follow` 稀疏 Morph，使用静止人体三角形建立 BVH 最近点绑定并转移位移。绑定同时保存表面间隙的局部坐标，随三角形切线和法线变化调整间隙。原生适配的分量排除在转移之外，Alias 不重复参与。
- 匹配的骨骼跟随人体最终旋转、位移、缩放及中心 / 末端 / 朝向偏移；服装自己的 ERC 能读取继承姿势。避免将人体已经求值的骨长公式再叠加一遍，保留服装额外骨骼及加载后的局部编辑。
- 转移在蒙皮之前进行，每次从静止网格重建，蒙皮仍使用服装自己的权重。相同输入不重复转移，归零不残留上次变形；相机操作不触发形变求值。

主要代码：`src/runtime/conform.*`、`deformation.*`、`morph.*`、`src/daz/loader.*`、`morphs.cpp`、`src/editor/document.cpp`。渲染报告增加绑定顶点、原生适配和转移求值计数。

## 验证范围与证据

新增 `ConformRuntimeTest`，覆盖原生 Morph 优先且不重复投射、解析三角形的重心和法线间隙、禁止跟随的参数、不同实例矩阵、角色 ERC 的最终修正、骨长 / 中心 / 姿势、衣物局部调整、归零、无效绑定，以及合并后实例隔离。

真实文件仍直接读取 `H:\G1\Scenes\test2.duf`，没有修改用户资产。场景包含四个人物、八件衣服、睫毛和泪膜，建立 10 条明确跟随关系、110,040 个顶点绑定、106 个原生 Morph 对应关系。验证首个人物的 `FBMBodybuilderSize`、`PBMBreastsSize`、`SCLArmsLength`、体型叠加胸部姿势和 `face_08.duf`；检查对应衣物变化、其余人物及穿戴物逐顶点不变、重复输入无更新、恢复后顶点完全一致。

最终证据路径：

- `artifacts/conform-final-ctest.log`：10 项工程检查。
- `artifacts/conform/final/test2.json`：真实绑定、各对象位移和 Shape 应用诊断。
- `artifacts/conform/final/ui/editor-check.json`：副屏真实滑块、恢复、相机操作验证。
- 同目录 `editor-before.png`、`editor-parameter-*.png`、`editor-parameter-reset-*.png`：编辑及恢复截图。
- `artifacts/conform/final/summary.json`：最终部署及证据摘要。

早期 `artifacts/conform/test2.json`、`artifacts/conform/ui` 是间隙旋转修正前的中间结果。第一轮真实用例曾只按内部 ID 查询 `chestLower`，而该骨骼内部 ID 为 `chest`、name 为 `chestLower`；测试查询已按资源身份修正，旧失败日志保留。

## 边界

本次解决已有明确 Fit To 关系、兼容骨架的穿戴物跟随；不是任意代际间的 AutoFit、dForce、DAZ 平滑 / 碰撞修改器、HD / SubD 或 IK。没有声称与 DAZ Studio 的完整服装求值逐顶点一致；最近表面转移属于当前 Runtime 的实现，大幅变形仍可能需要服装专用修正或碰撞处理。

副屏截图确认衣物随胸部大小改变并可恢复，但 Breasts Size = 0.5 时运动内衣表面仍可见少量浅色穿插点。间隙旋转修正没有完全消除这些点，因此本次只能验收 Morph / 骨骼同步，不把无穿插或 DAZ 碰撞效果标为已通过。

`face_08.duf` 的缺失第三方依赖、跨对象控制器和同名歧义仍按预设诊断报告；不会通过伪造 Morph 来消除缺项。

格式依据：[DAZ node_instance 的 conform_target](https://docs.daz3d.com/public/dson_spec/object_definitions/node_instance/start) 和 [channel_float](https://docs.daz3d.com/public/dson_spec/object_definitions/channel_float/start)。实现未复制第三方插件源码。

## 手动启动

```cmd
"D:\Github\DazFastViewer\out\DazFastViewer.exe" --file "H:\G1\Scenes\test2.duf"
```

在层次树选择其中一个人物，调整 Bodybuilder Size 或 Breasts Size，观察其衣服同步变化及其他人物保持原状；重置选中对象可恢复初始状态。
