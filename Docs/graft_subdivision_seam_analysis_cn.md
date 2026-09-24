# test7 GeoGraft 接缝复查

后续已经将共同细分、共享法线及同一 Cycles Object 的皮下散射修复接入生产路径，见 [修复执行记录](graft_seam_fix_execution_cn.md)。下文保留修复前的诊断数据和实验阶段结论；版本状态及最终验证以执行记录为准。

本轮先按用户要求提交原有本地代码：`83d0d0b`（支持服装与角色附件自动穿戴，优化换装和细分增量渲染），随后进行专项诊断。以下结论针对实际 `H:/G1/Scenes/test7.duf`，不是仅根据用户截图推断。

**白模复查修正：** 按用户反馈，后续对照已改为纯白材质。下面所称实验合并后的“裂缝消失”仅表示开口闭合，不等于当时曲面质量或完整渲染已经验收。前后图采用不同的拓扑处理，不能理解为同一网格等待更长时间后的变化。当时的生产程序尚未接入实验合并路径。

## 已确认的原因

**上次基础顶点映射修复生效了，但角色和 GeoGraft 随后分别细分，原本对齐的边界又被拉开。** 这是本次黑色裂缝的主要原因。统一细分等级不能解决拓扑分离；基础 UV 导入未发现索引或数值错误。

当前数据流是：

1. Morph、蒙皮、碰撞和附件跟随完成后，`DeformationRuntime::weld_grafts()` 根据 `vertex_pairs` 复制宿主基础顶点的位置。
2. `CyclesAdapter::synchronize()` 为每个网格分别建立 `runtime::Subdivision`，再分别计算细分位置、建立 Cycles 网格。
3. 两边只共享数值相近的位置，没有共享顶点和相邻面。各自的细分插值受各自拓扑影响，最终边界位置及法线不再连续。

对应代码：[基础顶点对齐](../src/runtime/deformation.cpp)、[逐网格细分](../src/cycles/synchronize.inl)、[OpenSubdiv 模板](../src/runtime/subdivision.cpp)。`vertex_pairs` 的读取方向与 [DAZ graft 规范](https://docs.daz3d.com/public/dson_spec/object_definitions/graft/start)一致：第一项为附件顶点，第二项为宿主顶点。细分曲面依赖控制网格拓扑及其插值规则，参见 [OpenSubdiv 文档](https://opensubdiv.org/docs/subdivision_surfaces.html)。

之前的 `graft_seams()` 检查比较的是**最终形变后、细分前**的基础顶点位置。其数值本身正确，但不能作为最终渲染无缝的证明。本轮给该接口补充明确注释，并增加独立的细分边界追踪。

## 几何测量

本轮使用实际场景的完整形变求值，然后沿 OpenSubdiv 的边后代关系追踪最终细分边界，按 `vertex_pairs` 配对基础边。度量为边界链上顶点到对侧折线的最近距离，取双向采样的最大值；单位为毫米。它不是 UV 距离，也不是仅比较相同编号的细分顶点。

Golden Palace 的 44 条边、HD Nipples 的 116 条边全部匹配，未匹配边均为 0。

| 情况 | Golden Palace | HD Nipples |
|---|---:|---:|
| 形变后基础网格，不细分 | 0 | 0.0000596 |
| 当前保存设置：角色 2 级、附件 1 级 | 3.5116 | 3.1633 |
| 三者统一为 1 级，但仍分别细分 | 3.3366 | 2.8879 |
| 三者统一为 2 级，但仍分别细分 | 4.1797 | 3.5877 |

基础网格上的微小误差是浮点数量级，远小于渲染后毫米级的裂缝。提高或统一等级不能消除该问题；单纯再次覆盖基础顶点也无法改变后续分别细分的结果。

场景中三者使用相同的 Catmull–Clark 算法、`edges_only` 边界模式和 `smooth_all_normals` 设置，区别仍在拓扑与邻接关系。按实际细分三角面估计的几何法线也存在夹角：保存设置下 Golden Palace 的中位数约 7.98°，HD Nipples 约 3.04°。该统计不是最终着色器加入 Bump 后的法线，不用于宣称已量化全部材质接缝。

原始测量见 [graft-seams.json](../artifacts/graft-seams/probe/graft-seams.json)。其中 `level=-1` 表示保存设置；`status=PASS` 只表示诊断执行成功，不表示接缝合格。

## UV 与材质核对

按场景指定的 UV Set 重新读取原始 DSF，包括 `polygon_vertex_indices` 的面角覆盖，再与导入 IR 的所有可见三角面角点逐项比较。没有将附件和宿主本来不同的 UV Set 混作一套。

| 对象 | 核对的三角面角点 | 与原始 UV 不一致 |
|---|---:|---:|
| Genesis 8.1 Female | 91,752 | 0 |
| Golden Palace | 13,386 | 0 |
| HD Nipples | 46,272 | 0 |

以上共 151,410 个角点，按程序使用的 float32 精度完全一致，包含 HD Nipples 自身的 `default.dsf` 和 `HD UVs.dsf` 两套数据。未发现基础 UV 导入错误。

边界两侧的原始 UV 不完全相同。Body 与附件之间有整数 U 平移 2，Arms 有整数 U 平移 4；当前 Cycles 图像节点默认使用 Repeat，因此必须去除整数平移后比较实际采样位置。不能把整块 UV 平移直接判定为贴错图。

按对应边界起点比较，去除整数平移后的 UV 距离最大值为：

| 附件 | 最大 UV 距离 | 折算到实际 4096×4096 贴图 |
|---|---:|---:|
| Golden Palace | 0.00041184 | 约 1.687 纹素 |
| HD Nipples | 0.00018073 | 约 0.740 纹素 |

这些小差异已存在于所选资产的 UV 数据中，可能造成细小纹理变化；它们不能解释无贴图灰模中仍存在的毫米级裂缝。本次没有擅自修改资产 UV。

边界上的 `GP_Torso`／`GP_Torso_Back` 与 `Body`、`Breast` 与 `Body`、附件 `Arms` 与角色 `Arms`，在 IR 中除材质 ID 外所有字段逐项相同，使用相同颜色及 Bump 贴图，无真实置换。这里的“相同”指 IR 输入参数：渲染器还会根据各材质覆盖的世界面积／UV 面积估算 Bump 高度，分开的几何范围可能产生不同估值；这应作为后续精细着色连续性的一项检查，不能仅凭 IR 参数相同认定最终着色完全相同。

使用同一份完整形变网格、按当前适配器公式重算，Body 的自动 Bump 距离约 0.592 毫米，GP_Torso／GP_Torso_Back 分别为 0.422／0.693 毫米，Breast 为 0.546 毫米；角色 Arms／附件 Arms 为 0.328／0.258 毫米。这些是乘 Bump Strength 前的尺度（当前 Strength 为 2），不是实际顶点位移。差异证明该路径还需要连续性核查，但本次未单独量化它对彩色边界明暗的贡献，不将其认定为黑色裂缝的主因。

原始 UV 来源、逐角点比较及材质比较见 [uv-source-check.json](../artifacts/graft-seams/uv-source-check.json)。对应只读复查脚本保存在同目录的 `check_uv_sources.py`。

## 图像对照

原始 test7 只读保留；将解压后的副本写到 `artifacts/graft-seams/test7-ev13.duf`，仅把 Tonemapper Options 的 Exposure Value 当前值从 15 改为用户建议的 13。原始材质近景如下：

| 区域 | 原始设置，EV 13 |
|---|---|
| 胸部 | [截图](../artifacts/graft-seams/baseline-front/reference.png) |
| Golden Palace 背面 | [截图](../artifacts/graft-seams/baseline-rear/reference.png) |

[三者统一 2 级的胸部截图](../artifacts/graft-seams/equal2-front/reference.png)仍有黑色边界，与数值测量一致。

灰模对照取消颜色纹理、Bump、SSS 和置换，两边使用相同灰色材质、相同机位及同一默认环境。实验合并过程删除宿主被附件遮盖的面，依照 `vertex_pairs` 复用宿主顶点编号，追加附件剩余顶点和面，然后只建立一次细分拓扑。该参考网格只用于诊断，不是产品中已完成的自动挂接实现。

| 区域 | 三者统一 2 级、分别细分的灰模 | 连接拓扑后统一 2 级的灰模 |
|---|---|---|
| 胸部 | [仍有裂缝](../artifacts/graft-seams/gray-separate2/reference.png) | [开口闭合，曲面未验收](../artifacts/graft-seams/gray-joint/reference.png) |
| Golden Palace 背面 | [仍有裂缝](../artifacts/graft-seams/gray-separate2-rear/reference.png) | [开口闭合，曲面未验收](../artifacts/graft-seams/gray-joint-rear/reference.png) |

两个区域均已目视复核；表中两组细分等级相同，排除了等级不同的干扰。保存等级下的灰模也保留在 `gray-separate`／`gray-separate-rear`。该对照证明了几何处理路径的因果关系，不代表已完成原始彩色材质或 DAZ HD 的完整等价验证。

## 后续白模复查与图像口径

之前的灰模偏暗，局部曲面起伏不容易看清，后续以白模为主要图像证据。`GraftSeamProbe` 现默认导出 `white-*.duf`，漫反射颜色为 `[1,1,1]`。逐文件核对确认，与旧的相应灰模相比，五个场景副本只有漫反射颜色改变，几何、面序、UV 和其他材质字段均一致。检查见 [geometry-unchanged.json](../artifacts/graft-white/geometry-unchanged.json)。

两列都使用纯白材质、相同相机及默认预览灯光，均为 2 级细分、关闭降噪；没有对截图进行美化或涂抹。左右两列的差别是细分之前是否连接拓扑：

| 区域 | A：当前分别细分路径，有几何开口 | B：实验连接拓扑后细分，开口闭合 |
|---|---|---|
| 胸部整体 | [A 白模](../artifacts/graft-white/2-front/reference.png) | [B 白模](../artifacts/graft-white/joint-2-front/reference.png) |
| 胸部上缘近景 | [A 白模近景](../artifacts/graft-white/2-upper-close/reference.png) | [B 白模近景](../artifacts/graft-white/joint-2-upper-close/reference.png) |
| Golden Palace 背面 | [A 白模](../artifacts/graft-white/2-rear/reference.png) | [B 白模](../artifacts/graft-white/joint-2-rear/reference.png) |

用户询问的两张臀部白模分别是 A、B，**不是同一场景的早期／晚期采样**。相机稳定后截图时间分别为 15.038 秒与 15.054 秒，显示样本索引均为 4096；自适应采样下该索引不代表每个像素都追踪了相同数量的光线。继续采样能够减少噪点，不会改变已有网格的顶点连接，也不会填上这里测得的毫米级开口。

白模与近景支持“共同拓扑可以闭合开口”这一结论。胸部上缘到腋下仍能看到局部曲面起伏，本轮没有证明这些起伏与 DAZ 原始结果等价，因此不能把 B 图称作完整修复通过。几何开口、邻域曲面过渡以及恢复原材质后的明暗／纹理连续性需要分别验收。

本次重新构建了诊断工具，重新求值并导出白模，完成六个副屏固定机位截图；材质替换未改动产品渲染代码。完整记录在 `artifacts/graft-white/`，原资产与生产程序保持不变。

## 修复方向与本轮交付范围

正式修复应在渲染侧为宿主与其 GeoGraft 建立**共享的控制拓扑和细分模板**，同时保留每个面的来源对象、材质槽和面角 UV。按共同拓扑生成连续位置和法线，再由对象来源映射维持选取、显隐、穿戴／解除和增量更新。编辑器中的附件仍可保持独立身份，不能用一次性的合并导出替代运行时支持。

应覆盖宿主及附件 Morph／姿势变化、细分切换、附件删除／重新挂接、嵌套 GeoGraft、多角色实例隔离与内存预算。不要只同步等级、再次复制基础顶点或把 UV 强行改成相同值；只拉齐细分后的边缘也不能保证邻域曲面和法线连续。

本轮完成原因定位、可重复的 CPU 测量、原始 UV 核对及 GPU 灰模对照。新增 `GraftSeamProbe` 为 `EXCLUDE_FROM_ALL` 的专项目标，边界捕获默认关闭；它不改变正常渲染的细分结果。`out/DazFastViewer.exe` 仍为提交前已部署的版本，**本轮未宣称生产程序已修复该接缝，也未部署实验合并网格**。新的诊断代码、测试和报告暂留工作区。

复现 CPU 诊断：

```powershell
cmake --build build --config Release --target GraftSeamProbe
build/Release/GraftSeamProbe.exe artifacts/graft-seams/test7-ev13.duf DazFastViewer.project.json artifacts/graft-seams/probe
python artifacts/graft-seams/check_uv_sources.py
```

细分测试新增了三种算法、1／2 级、普通边界与孔洞边界的检查：追踪链必须与最终可见三角面的真实边界一致，且捕获开关不能改变顶点／三角面结果。全部相关目标重新构建后，16 项工程检查通过；按工程既定口径排除无独立可执行文件的上游 `cycles_version`。构建及工程检查记录位于 `artifacts/graft-seams/diagnostic-build.log`、`ctest.log`。

原始 test7 SHA-256 在调试前后均为 `0ee163fb9032ed36c00d574816212b31895c4d81104c58688442c9b39715c05d`。发布程序 SHA-256 保持为 `2afac2c00e1aa1ab14df074b08593ddec99938f7f013439911ff5f36919005b6`。所有 GUI 对照均在副屏运行，未修改内容库资产。
