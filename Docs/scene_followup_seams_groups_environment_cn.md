# 接缝、场景组与新建后环境设置修复

本轮处理用户追加的三个问题，属于 007 之后的兼容性修复，008 未开始。用户资产继续只读引用，界面验证只使用副屏。

**后续复查修正：** 本文的“最终顶点／最终世界坐标”仅指形变后、细分前的基础控制网格。旧检查不能证明渲染接缝消失；test7 仍因宿主与附件分别细分而出现毫米级裂缝。新的基础／细分边界测量、原始 UV 核对及灰模对照见 [GeoGraft 接缝复查](graft_subdivision_seam_analysis_cn.md)。

## 原因与改动

**Golden Palace 接缝。** 之前只导入 `graft.hidden_polys`，没有使用 `vertex_pairs`。按照 [DAZ 官方 graft 定义](https://docs.daz3d.com/public/dson_spec/object_definitions/graft/start)，每对索引依次指向插件顶点和宿主顶点。现在保存并校验顶点对，在 Morph、蒙皮、碰撞和刚性跟随完成后，将插件边界转换到正确的局部坐标并对齐宿主最终顶点。按 Fit To 依赖顺序处理嵌套插件，结果通过网格 Delta 同步到渲染和选取；不修改资产、不合并材质或改变原拓扑。

共同祖先变换从相对矩阵中消去，整体移动人物不会重复重写接缝；插件相对移动、宿主形态或姿势变化时重新对齐。Visible 不参与解除接缝或恢复宿主遮盖面。接缝诊断独立比较最终世界坐标，避免只凭截图角度判定。

**Group 层级。** 加载器原先只保留节点 ID 和 parent，场景树只创建可编辑网格与骨骼，遇到 Group 就跳到更远的祖先。现在保存合并后的 Group 标签与类型，先创建普通组／实例组，再按原父子关系连接组和对象。组用于层级展示与展开，子对象继续使用原有编辑控件；本轮没有新增组级变换或批量显隐操作。

**新建后缺少环境。** 新建会创建一个非空的 `Document` 指针，内容库打开文件据此走追加逻辑；追加逻辑保留旧快照选项，于是把空场景的设置留给了 14.duf。现在加载入口同时检查实际节点、对象、灯光与环境内容；空文档中的第一次打开采用源场景设置，已有场景的追加仍保留当前设置。

## 回归范围

- 工程测试覆盖顶点对解析和越界、嵌套接缝、不同局部坐标／非均匀缩放、Morph 与姿势变化、共同移动、插件相对移动、显隐及静止重复求值。Group 测试包括空组、嵌套父节点和资产库中的继承标签。
- 新增 `--scene-reopen-test <第二个 DUF>`：第一场景出图后新建，等待空场景出图，再调用内容库相同的打开入口；核对源环境／ToneMapper 完整选项、Group 行及可见树节点的父子关系。
- 实际命令、构建记录和原始报告位于 `artifacts/scene-followup/`。项目约定的 14 项 CTest 全部通过，`SceneLoadProfile` 也单独编译通过。

## 真实场景结果与部署

发布包副屏流程 `test7 → 新建空场景 → 内容库打开 14` 通过。对象数依次为 13、0、160；重新打开后的完整环境／色调设置与 14 的加载结果一致，节点 ID 为 `Environment Options-1` 和 `Tonemapper Options-1`，实际教室恢复环境照明。对场景树的节点父子关系逐项核对通过：`Group` 下 9 个直接子节点、`TS Classroom` 下 28 个、`Group (2)` 下 2 个，另保留 `Table Instances` 实例组。实例组内部的散布条目仍不作为独立可编辑对象列出。

`graft-visible` 使用 `test7` 的实际最终形变结果，四个插件合计 228 对顶点。误差为配对顶点在最终世界坐标中的欧氏距离：

| 插件 | 顶点对数 | 最大误差（米） |
|---|---:|---:|
| Golden Palace Gens | 44 | 0 |
| HD Nipples for G8F - 2.0 | 116 | 5.9605 × 10⁻⁸ |
| Genesis 8 Male Genitalia | 34 | 3.0720 × 10⁻⁸ |
| Genesis 8 Male Genitalia (2) | 34 | 6.6640 × 10⁻⁸ |

Golden Palace 实际关闭／恢复 Visible 通过属性、场景树和渲染状态核对，几何更新数保持 0。近景截图用于辅助复核，接缝结论以上述顶点数据为依据。两项真实场景检查的渲染器 `error`、`edit_error` 均为空，图像互操作 CPU 回读为 0；显隐测试仍记录到此前已知的缓冲忙 `could not begin update` 日志，不将其描述为完全无告警。

可查看[重新打开后的教室](../artifacts/scene-followup/reopen/reopened-scene.png)、[流程检查](../artifacts/scene-followup/reopen/editor-check.json)、[接缝及显隐检查](../artifacts/scene-followup/graft-visible/editor-check.json)和[汇总](../artifacts/scene-followup/verification-summary.json)。截图在低样本数时捕获，只验证场景和照明存在，不作为 Iray 材质 Golden。

`out/DazFastViewer.exe` 已部署；SHA-256 为 `d3daa65a48d4794df3101343a81442d611d54f7b909dfe04d866bc6bfb80f707`。`release-build-manifest.json` 中的源码及程序哈希已与工作树／发布包核对一致。本轮没有创建 Git 提交。

本轮修复基础网格边界的位置连续性，不代表实现 SubD／HD 或消除跨材质的法线、纹理差异；Sun-sky 与 Iray 的亮度近似边界保持不变。
