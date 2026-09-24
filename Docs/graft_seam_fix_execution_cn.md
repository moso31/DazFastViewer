# GeoGraft 共同曲面与皮下散射接缝修复

承接 [test7 接缝诊断](graft_subdivision_seam_analysis_cn.md)，本轮将修复接入正常加载、自动穿戴和增量编辑路径。使用原生 DUF 加载流程，不要求用户预先导出合并网格。原始内容库保持只读。

## 原因与修复

接缝涉及两个问题：

1. 基础网格的 `vertex_pairs` 对齐后，宿主与附件分别细分。两侧缺少共同的相邻面，细分后边界位置和法线又发生分离，产生毫米级开口。
2. 将边界连通、统一细分和法线后，如果仍拆成多个 Cycles Object，原皮肤材质下会残留细线。Cycles 的随机游走皮下散射通过 `scene_intersect_local(..., object, ...)` 限定在当前对象内，无法跨越这样的对象边界。

第二个原因经过实际 test7 材质控制实验确认：关闭 Bump 后细线仍在；关闭皮下散射后消失。随后改成共同 Cycles Object，在保留原贴图、Bump 和皮下散射的情况下消除了该线。相关内核位于 `.deps/cycles/src/kernel/integrator/subsurface_random_walk.h`；实验图像位于 `artifacts/graft-fix/no-bump/`、`no-sss/` 和 `joined-color-close/`。

正式路径根据直接宿主关系建立 GeoGraft 组合：剔除被替换的宿主面，按 `vertex_pairs` 复用宿主顶点，合并其余控制点，再一起细分、计算法线。最终整个组合进入同一个 Cycles Mesh / Object。嵌套附件先连接到自己的直接宿主，随后参与根宿主的共同曲面。

面角 UV、材质槽及其来源保留；没有通过重绘纹理、移动 UV、关闭 Bump 或关闭皮下散射遮盖问题。之前逐角点检查已经排除本场景的 UV 导入索引／数值误差。这里解决的是已确认的几何与渲染对象边界，不保证任意第三方材质自身都没有色差。

## 编辑行为

- 场景树、选择、Morph、穿戴关系及附件身份保持独立；只在渲染层组成共同对象。
- 角色和已挂接 GeoGraft 使用宿主的同一“渲染细分等级”。选中附件修改该参数，也修改整个共同曲面；普通衣物和独立头发维持各自等级。
- Morph 或姿势改变时重新插值共同曲面，复用细分模板；恢复参数不累加顶点变更。
- 单独隐藏附件时从该组合的渲染面列表剔除对应面。绑定与宿主遮盖面继续保留，解除挂接／删除附件才恢复对应宿主面。这沿用已有附件语义。
- 因为 SSS 需要共同对象，附件显隐现在允许更新所属组合的面列表，不能继续套用旧版“显隐的几何更新计数必须为零”断言。会话、细分模板、无关网格、材质和贴图可复用。
- 相同原型与显隐组合的 DAZ Instance 共享设备网格；仅其中一个实例隐藏附件时生成对应可见面变体，不影响其他实例。
- 400 万细分面保护作用于共同网格。组合较大时可能比过去独立计算更早触发预算；拒绝后保留原场景，允许降低等级恢复。

## 验证方法

17 项工程测试通过，新增共同曲面测试使用手工建立的完整曲面作为参考，覆盖共享边界、法线、面角 UV、材质来源、嵌套附件、Morph 恢复、实例变换、无效关系及统一细分参数。记录：`artifacts/graft-fix/ctest.log`。

新增 OptiX GeoGraft 检查通过 14 个阶段：初始共享、宿主与附件 Morph、单实例显隐、显隐与 Morph 同时提交、SSS 材质编辑、细分 0／1／2、整体移动、非法等级拒绝、解除、重新挂接、删除及恢复。共同设备顶点与参考曲面的最大误差为 0 米；打包法线的最大向量误差约 `4.11e-5`。同时验证 UV、材质槽、独立实例显隐、网格共享和无关网格身份。记录：`artifacts/graft-fix/gpu/graft-check.json`。

原有 OptiX 置换／场景同步检查的 23 个阶段也通过，覆盖置换恢复、材质、Morph、实例及曲线增删。记录：`artifacts/graft-fix/displacement/`。

真实 test7 的两个附件均通过属性与场景树显隐／恢复，每次开关只更新一个组合网格，全程一个会话，未重新求值 Morph。真实 Qt 细分控件连续切换 0／1／2／3、快速输入、相同值和超预算恢复均通过；基础网格哈希、会话、碰撞求值和用户机位的保持检查通过。全场景三角形计数分别为 1,156,984／1,308,438／1,914,210／4,337,298，重复回到同一等级的数量一致。每步完成后记录的私有提交最大约 11.61 GiB，不代表瞬时峰值。记录：`artifacts/graft-fix/visibility-release/`、`stress/`。

真实角色的 `Breasts Size`、`Chest Scale` 和 `Heavy` 参数经过 Qt 控件修改、渲染更新和恢复检查，全部通过；参数恢复无累计漂移，相机操作不重新求值形变。记录：`artifacts/graft-fix/morph-release/`。

发布版还完成 Genesis 8.1 Female 移动／转头后，通过 HD Nipples 的受支持 `.dse` 产品入口添加附件、解除和重新绑定，等待各次渲染完成后检查通过。记录：`artifacts/graft-fix/wear/`。该入口仍仅映射产品基础 DUF，不执行产品脚本；基础接缝误差检查只是此流程的一项检查，正式无缝结论还依赖共同拓扑检查及上述真实图像。

为保留排查过程，目录中还保留早期失败记录：`visibility/` 使用了旧版“任何显隐都不得更新几何”的断言；`morph/` 指定了本场景角色未提供的 `Eyes Closed` 参数。最终分别由上述 `visibility-release/` 与 `morph-release/` 结果取代。没有将这些失败记录计入通过结果。

真实场景通过 `artifacts/graft-seams/test7-ev13.duf` 将曝光 15 改为 13，其余采用原场景。白模副本 `artifacts/graft-fix/test7-white.duf` 使用 RGB `(1, 1, 1)` 的无贴图材质，并隐藏无关角色，保留被测角色及两个附件。白模使用固定工作室照明；原材质检查保留原场景照明。所有 GUI 检查均在副屏，降噪关闭。

发布版原材质胸部图 `release-color-front/reference.png` 在固定相机后 15.033 秒、1,152 samples 保存；臀部图 `release-color-rear/reference.png` 为 15.045 秒、1,392 samples。图像分辨率均为 924×636，之前的黑色开口及胸部散射细线未再出现。正式截图直接由编辑器输出，没有图像修补。

对照入口：

- [修复后胸部原材质](../artifacts/graft-fix/release-color-front/reference.png)
- [修复后臀部原材质](../artifacts/graft-fix/release-color-rear/reference.png)
- [修复后胸部白模](../artifacts/graft-fix/release-white-front/reference.png)
- [修复后臀部白模](../artifacts/graft-fix/release-white-rear/reference.png)
- [修复前臀部白模](../artifacts/graft-white/2-rear/reference.png)

白模显示的明暗来自照明与形状，材质输入为纯白。它用于观察几何连接及局部曲面；不将原材质与白模之间的亮度差异作为几何修复依据。

胸部与臀部白模均已核对边界。修复前后白模都达到 4,096 samples；胸部约 15.040／15.025 秒，臀部约 15.038／15.022 秒。修复前臀部仍有清晰黑色开口，正式修复版闭合，因此结果不能归因于延长渲染。相机参数及原始采样记录保存在各目录的 `command.json`（发布版）和 `convergence.json`。

## 发布

程序已部署到 `out/DazFastViewer.exe`；重新启动程序后加载原始 test7 即使用新路径，曝光 13 仅是对照图的观察设置。源码与二进制清单核对通过，记录位于 `artifacts/graft-fix/delivery-integrity.json`、`delivery-summary.json` 和 `out/build-manifest.json`。

- 编辑器 SHA-256：`5a1a9c230c7eea11f8ad1a45caa0e0c339f616c1670ba5bad8df8a13445a2035`。
- 原始 test7 SHA-256 未改变：`0ee163fb9032ed36c00d574816212b31895c4d81104c58688442c9b39715c05d`。
- 先前保存原有代码的提交为 `83d0d0b`。后续按用户要求，将本轮改动拆为功能修复 `a41646c` 和诊断测试两次本地提交；本记录随诊断测试提交。混合文件中的功能与测试改动按代码块分别暂存，拆分未改变已验证的运行代码。

## 验收边界

本轮实现普通细分下的 GeoGraft 共同曲面与 SSS 连续性，没有新增 DAZ HD Morph 解码、产品纹理生成脚本或 dForce。附件本身的雕刻形状、材质差异仍按资产表达，不用边界抹平去改变它们。

前后对照必须标明几何版本、材质和采样时长。之前“分别细分”与“共同细分”的两张图是不同几何处理，不能解释为多等一会儿，原来的裂缝就会消失。更多样本只能降低噪点，不能修复已经分离的顶点或 SSS 对象边界。
