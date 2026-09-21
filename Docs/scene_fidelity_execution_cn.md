# test.duf 可见性、发丝、材质与服装贴合修复

日期：2026-09-21。此项是 007 后的实际场景修复，008 FK / IK 未开始。输入为 `H:\G1\Scenes\test.duf`，原 DUF 和内容库资源没有改写。运行验证仅使用第二屏。

## 实现结果

| 用户反馈 | 原因与处理 |
|---|---|
| 隐藏物体仍然渲染 | 节点扩展中的 `Visible` 没有进入运行时。现在合并资产默认值和场景覆盖，保存到实例及编辑快照；场景树复选框和属性面板的“可见（Visible）”联动，更新 Cycles 可见性、射线选取及覆盖层。普通切换不重新计算顶点。 |
| A 的 StrandBasedHair 丢失 | 资产只有 `polyline_list`，旧管线只生成三角面。现在保留曲线源顶点编号，接入 Cycles Hair、发根 UV、根梢宽度和头发 BSDF，复用 Morph / 蒙皮更新。该场景读取到 35,638 根曲线、875,210 个源控制点。隐藏的 HYLongHair 保持关闭。 |
| 头发、丝袜及皮肤材质失真 | 增加颜色常量线性化、UV 缩放 / 偏移、Glossiness、Weighted Glossy、各向异性、薄壁透光、双瓣高光、涂层及米制 SSS 半径。透明遮罩和法线继续参与最终表面。叠层透明路径上限从 8 提高至 32。 |
| 眼睛内层过暗 | 角膜和泪膜此前被按实体透射处理，并把漫反射颜色用于玻璃。现在读取 `Thin Walled`，按折射颜色及 `Share Glossy Inputs` 构建透射，不把灰色视口漫反射值误乘到透明层。增加采样数不是这项修复的替代措施。 |
| 根层级拖鞋不能跟随 A | `conform_target` 独立于 `parent`。导入时补齐目标场景变换；交互时根层级穿戴物跟随 FitTo 目标，不改写场景树。已经继承目标父变换的穿戴物不会再次叠加。 |
| A 胸部与上衣穿插 | 场景为上衣设置的 Mesh Smoothing / Collision Item 原先未执行。现在在 Morph → FitTo → Skinning 后进行碰撞，读取启用状态、迭代数、权重及碰撞对象。人体与可见 GeoGraft 分别提供外侧约束；增加人体顶点到服装面的反向检查，处理粗三角面之间的小凸起。人体形态没有被缩小。 |

可见性按节点独立保存，不自动递归隐藏子对象，也不沿 FitTo 传播。DAZ 中 Ctrl 点击眼睛图标的递归操作是额外的交互行为，本轮未新增该快捷操作。

碰撞每次从未修正的蒙皮结果开始，不将上次修正反馈给 Morph 或蒙皮。只平滑修正位移，保留原始褶皱；默认安全间隙为 0.5 毫米，反向局部检查范围为 2 厘米。碰撞依赖排序、循环拒绝、追加场景的引用重命名及删除后的引用清理均已接入。

## 验证与证据

证据目录为 `artifacts/scene-fidelity`，不是素材库的一部分。

- `ctest-final-contact.log`：10 项工程回归通过，包括曲线索引、材质解析、独立可见性、根层级 FitTo、碰撞关闭 / 恢复、Morph 后碰撞、GeoGraft 显隐、内外层约束和粗服装面小凸起。
- `actual-final.json`：原始完整场景加载及运行时检查通过。A 移动 10 厘米时拖鞋跟随、B 不动；隐藏 HYLongHair 可通过增量更新显示和恢复；A 头骨旋转 10 度后发丝控制点变化，恢复逐点一致，B 顶点未受影响。发丝最大 L1 位移为 `0.058405347 m`。此记录在最后一次粗面接触修正之前生成；最终接触算法另由专项回归及整场景截图验证。
- `visibility-ui/editor-check.json`：副屏操作属性复选框和场景树复选框均通过，几何更新次数为 0。隐藏与恢复截图已保存，恢复图像逐像素一致。
- `b-head-final-ui/scene.png`：B 的 64 samples 头部特写，用于检查头发、皮肤、角膜及泪膜。
- `a-contact-ui/scene.png`：从实际蒙皮输出生成的冻结几何诊断，确认 A 胸部及附加网格被上衣覆盖；此图用于局部问题定位，不代替最终直接加载原始 DUF 的验证。
- `final-ui/scene.png`、`final-ui/editor-check.json`、`final-ui/editor-render.json`：最终部署版本直接加载原始 `test.duf` 的整场景副屏检查通过；33 个对象、35,638 根曲线、11 次初始碰撞求值，渲染和编辑错误均为空。截图中 A 胸部被上衣覆盖，拖鞋处于脚部。

另用 Blender BVH 对实际上衣的 100,155 个顶点 / 面内采样点作独立近表面检查，脚本为 `tools/reference/check_clothing_collision.py`。`a-contact-check.json` 中超过 2 毫米的向内采样数由 4,867 降至 231，超过 0.5 毫米的采样数由 7,257 降至 350；1,267 个上衣顶点发生超过 1 微米的修正，最大修正约 18.70 毫米。缓存坐标往返使人体坐标最大差约 0.000273 毫米，人体没有受到碰撞形变。

上述 BVH 检查是最近面有符号距离，开放边界处不构成封闭实体穿插证明。仍存在最大约 6.86 毫米的局部向内采样，因此不能把近景改善写成任意姿势、任意服装完全无穿插。

开发中的 `ui`、`a-chest-ui`、`a-chest-final-ui` 是中间截图，仍有已被后续修正的穿插，不能用作最终验收。`b-head-ui` 曾请求超过编辑器 64 样本上限的截图，未通过；截图参数现限制在有效范围内，使用 `b-head-final-ui`。

## 支持边界

- 这是原生 Cycles 转换，不是 Iray 材质的完整等价实现。SSS 使用 Random Walk 及测量距离近似；部分涂层模式、颜色效果、通道贴图、体积参数和凹凸距离仍未完全映射，继续在资产报告中列出。当前预览灯光也不是 DAZ 截图中的完整环境设置，材质 Golden 尚未通过。
- StrandBasedHair 支持资产中已存储的曲线、宽度及形变；额外渲染发丝生成器和 dForce 模拟未复现。
- Mesh Smoothing 是原生碰撞与位移平滑近似，未实现 DAZ Pyramid Coordinates、完整 Base Shape Matching、布料自碰撞或 dForce。GeoGraft 元数据本轮用于服装碰撞，完整渲染拓扑裁切与边界焊接仍不是本轮能力。
- 当前 `test.duf` 没有同名 `test.dbz`。已有 `3.dbz` 对应旧场景，不作为当前场景的逐顶点 Golden；没有用 DBZ 顶点替换运行时输出。

## 参考与复现

行为核对使用本机 Blender 5.2.2、Diffeomorphic 5.2.0 及 Cycles Hair / Principled 源码接口；没有复制插件实现代码。

- [DSON node_instance：conform_target](https://docs.daz3d.com/public/dson_spec/object_definitions/node_instance/start)
- [DSON geometry](https://docs.daz3d.com/public/dson_spec/object_definitions/geometry/start)
- [DSON graft：目标拓扑、隐藏面和顶点对](https://docs.daz3d.com/public/dson_spec/object_definitions/graft/start)
- [DAZ 4.12 发布记录：Ctrl 点击可见性图标](https://docs.daz3d.com/public/software/dazstudio/4/change_log_4_12_0_86)

完整场景运行时检查：

```powershell
build/Release/SceneWorkflowTest.exe --fidelity H:/G1/Scenes/test.duf artifacts/scene-fidelity/actual-final.json
```

几何诊断通过 `SceneWorkflowTest --geometry` 输出碰撞前后位置。`ConformRuntimeTest --collision-cache INPUT OUTPUT` 可在已导出的蒙皮结果上复验碰撞，避免每次重新扫描商业 Morph 库；缓存需附带源面编号、GeoGraft 信息和显式碰撞配置。

部署入口为 `out/DazFastViewer.exe`，SHA-256 为 `7330df4af560086c5fc1353cee1106c8fe17c296c8325dc0c5253ede17272b67`。程序与构建产物一致，摘要见 `artifacts/scene-fidelity/summary.json`。本轮按用户要求以中文提交说明归档到本地 Git，提交记录见 `git log`。
