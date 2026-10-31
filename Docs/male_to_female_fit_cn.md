# Genesis 8 女性的跨基型服装适配

支持将声明为 Genesis 8 / 8.1 Male 的服装穿到 Genesis 8 / 8.1 Female 上。选中女性后从内容库加载服装，或将已有服装的 Fit To 改到女性，转换自动执行。复制穿搭也使用同一转换流程。

G8 男装女用范围是 `Follower/Wardrobe` 服装及其子类型，包括上衣、裤子和鞋。另外支持 G9 鞋类到 G8 / G8.1 女性，范围与条件见下文。女性服装到男性、其他跨世代服装、跨性别头发和 GeoGraft 不在本次范围内。

## 转换方式

- 使用目标女性目录中经过几何兼容性校验的 `CloneGenesis8Male` 建立男性参考表面。缺少该参考时给出明确错误；需要安装 Genesis 8 Female Starter Essentials 并刷新参数目录。
- 在男性参考表面上绑定男装，再把女性基础形体与当前 Morph 的位移传递到服装。男女同名的体型 Morph 不再直接视为相同几何形态；由相同骨骼轴驱动的衣物 JCM 只应用一次。服装自己的调整通道继续可用。
- 平滑自动生成的位移场；胸腹区域额外执行向外平滑，减弱人体凹陷在布片上的刻痕。区域权重沿衣物拓扑过渡，保留衣服基础网格的褶皱、UV 和材质。骨骼跟随同时对齐女性的关节中心、末端和朝向，并保留服装额外骨骼。
- 碰撞使用男性参考形体确定局部身体区域。对姿势变化后进入布面的其他肢体，补充有向射线的内部判定和有限距离退出，不因身体仅靠近衣物就推开布面。
- 在一级细分表面检查接触，并通过 OpenSubdiv 模板把局部修正回传到控制网格。修正距离及相邻点的位移差都有约束，避免跨到另一侧肢体或拉出尖角；不改变用户的显示细分等级。
- 基础资产不被烘焙改写；解除绑定、换回男性以及 DUFEX 保存重开均重新按当前关系求值。

这是运行时几何适配，服装的自然垂坠、衣物之间的碰撞和极端姿势仍受现有静态碰撞能力限制。大幅胸部形态可能仍表现得偏贴身，需要使用服装自己的调整参数；本次没有与 DAZ AutoFit 做同条件图像对照，不能据此宣称所有资产的质量都更好。

## 验证

专项工具：`MaleToFemaleFitTest`。单元测试覆盖参考形体转换、男女同名 Morph 隔离、衣服自身 Morph、关节对齐、缓存一致性、缺失参考、GeoGraft 限制、大幅身体凸起穿过粗布面、跨区域肢体接触及移开后的复原。`SubdivisionTest` 另行验证细分接触回传和无接触时保持控制网格。

真实测试使用 `H:/G1/Scenes/test4.duf` 中两位女性的原有形态和姿势，加载 `Summer Vibes G8` 的 `!Tshirt.duf`、`Bermuda.duf` 和 `Flip Flop.duf`，覆盖体型编辑、肩部动作、换绑男性、解除再绑定、复制到另一女性及保存重开。测试副本移除旧穿搭以便检查新服装。

```powershell
tools/build.ps1 -VisualStudio 2022 -Configuration Release
build/vs2022/bin/Release/MaleToFemaleFitTest.exe --actual H:/G1/Scenes/test4.duf "H:/G1/People/Genesis 8 Male/Clothing/SD/Summer Vibes G8" artifacts/male-to-female/refined
build/vs2022/bin/Release/MaleToFemaleFitTest.exe --inspect artifacts/male-to-female/refined/summer-vibes-test4.dufex artifacts/male-to-female/inspect-refined
```

可打开的测试场景：`artifacts/male-to-female/refined/summer-vibes-test4.dufex`。正式验收程序：`out/vs2022/Release/DazFastViewer.exe`。

完整验收结果及几何比较记录见 `artifacts/male-to-female/refined`。基础资产保持只读。

2026-10-25：53 项 CTest 全部通过；真实场景换绑、体型／动作编辑、复制及保存重开通过；正式程序截图验收通过；部署源码与可执行文件散列一致。详见 [构建核对](../artifacts/male-to-female/refined/build-verification.json)、[真实场景报告](../artifacts/male-to-female/refined/report.json) 和 [正式程序截图](../artifacts/male-to-female/refined/app/scene.png)。Qt 部署工具仍提示找不到 `dxcompiler.dll / dxil.dll`，PowerShell 包装进程因此返回 1；日志确认构建、部署及全部测试完成，正式程序使用 OptiX 渲染通过。

同一组细分网格顶点及三角形中心采样，使用三个方向的射线复核，超过 2 mm 且至少两个方向判为身体内部的点数如下。这是采样结果，不代表穿插面积或任意姿势的保证；完整有符号距离和最大值见 [几何比较](../artifacts/male-to-female/refined/geometry-comparison.json)。

| 对象 | 初版 | 本轮 |
| --- | ---: | ---: |
| 第一位女性的上衣／短裤 | 0 / 0 | 0 / 0 |
| 第一位女性的鞋 | 18 | 0 |
| 第二位女性的上衣 | 3379 | 68 |
| 第二位女性的短裤 | 734 | 41 |
| 第二位女性的鞋 | 0 | 0 |

这是功能与显示验收，不是任意姿势的零穿插保证。第二位女性的腋下、手臂贴身和双腿靠近区域仍受静态接触求解限制；不能为了让局部数值归零，把布片推到肢体另一侧。大幅胸部形态仍可能偏贴身。

几何导出包含基础网格、碰撞前结果和实际细分结果，可供独立检查。最近面法线的有符号距离在手臂贴近躯干等姿势下可能把正常靠近衣物的裸露部位也计为穿插，需结合身体分组和图像判读，不能只使用全身最大值判断衣物质量。

`--inspect` 同时导出 `*-collision.json`，包含静止参考、原始多边形拓扑、细分设置及蒙皮后输入。可用 `--replay 输入.json 输出目录` 独立重放碰撞，避免每次诊断都重新加载完整 DAZ 素材库；该入口用于诊断，不替代保存重开的真实场景测试。

## 鞋类前掌统一权重

选中鞋子，在 **对象属性与 Morph → 参数 → 特殊处理 → 前掌统一权重（%）** 调整。默认 0%，保持原始蒙皮；100% 时，同一只脚的前掌共用一组平均骨骼权重，中间值连续混合。左右脚分别求平均。旧场景默认关闭，保存到 DUFEX 后保留当前值，支持撤销、重做和复制穿搭。

平均值按原始前掌三角形面积计算，减少局部网格加密造成的偏差。参与平均的是完整蒙皮权重，包括五根脚趾、末节以及原有 Foot / Metatarsals / Toe 等骨骼贡献，不直接把十根脚趾骨各设为 10%。这样既保持鞋子的总体动作，也消除前掌各处跟随不同脚趾产生的局部折叠。向脚背连接处平滑过渡；碰撞修正对前掌采用整体拟合并随同一参数混合，避免碰撞再次产生尖锐折痕。角色姿势、服装自身 Morph 和基础资产不被改写。

此功能独立于男装女用，也可用于其他具有 `lToe / rToe` 骨骼及前掌多边形分区的鞋类。只在符合条件的非人物对象上显示。0～100% 是混合算法的定义域，仍可使用共享控件的精度与范围设置。硬鞋底可先试 100%，较柔软的鞋或需要更贴合分趾动作时可调低。前掌统一后会减少局部避让自由度，极端脚趾姿势仍可能需要鞋子自身调整参数。

原 **渲染 → 独立阴影结构** 已移到 **特殊处理 → 独立阴影结构**；其参数标识、保存值和阴影功能保持一致。

验证命令：

```powershell
build/vs2022/bin/Release/MaleToFemaleFitTest.exe --forefoot-scene artifacts/male-to-female/refined/summer-vibes-test4.dufex artifacts/forefoot/final
build/vs2022/bin/Release/MaleToFemaleFitTest.exe --inspect artifacts/forefoot/final/summer-vibes-test4.dufex artifacts/forefoot/inspect-final
```

`GarmentDeformationTest` 覆盖 0% / 50% / 100%、左右独立、姿势跟随、缓存失效、关闭后精确复原、旧场景默认值与保存重开。`ParameterControlsTest` 用真实 Qt 鼠标、键盘和滚轮事件检查参数输入、点击选中后调参、外层滚动、原生箭头布局及阴影入口；快照和编辑历史测试覆盖数值持久化及撤销重做。

test4 的两双 Flip Flops 在 100% 下，前掌超过蒙皮前边长 2 倍的边分别从 95 / 394 条降为 0 / 0 条；第一双的边长比范围从 0.154～8.339 收窄为 0.945～0.947，第二双从 0～10.399 收窄为 1.095～1.141。这项比较仅统计蒙皮前长度超过 0.1 mm 的前掌边，包含碰撞后的最终结果，记录在 `artifacts/forefoot/average/edge-comparison.json`。

平滑改善不等于接触完全正确：100% 的细分网格复核仍发现局部穿插，最大确认深度分别约 6.24 mm、5.73 mm（包含鞋面和前掌连接处）。`artifacts/forefoot/inspect-final/5` 与 `7` 内的 `inside-check.json`、`contact-top.png`、`contact-side.png` 保留了复核结果；穿插点计数依赖网格采样密度，不表示面积。新测试场景为 `artifacts/forefoot/final/summer-vibes-test4.dufex`，两双鞋均设为 100%，便于直接调回 0% 对照。

本次最终 VS2022 Release 构建正常退出，53 项 CTest 全部通过；真实场景的连续强度、关闭复原、重复求值、保存重开及其他对象不变检查通过。部署源码与程序散列核对结果见 `artifacts/forefoot/final/build-verification.json`，完整场景检查见同目录 `forefoot-report.json`。

## 小脚趾外缘复核与自动 Morph 保形

用户指出 `5-after-collision-top.png` 左下侧外缘仍不平整。分阶段比较发现，统一蒙皮之前的自动 Morph 已经改变前掌局部轮廓：第一位女性右前掌与原鞋的最佳整体相似变换之间，最大剩余偏差为 5.94 mm。仅统一蒙皮权重会保留这一层形变。此外，旧统计仅覆盖 `Toe` 面，没有覆盖 `Foot` 内的鞋底连接区，不能代表整圈鞋底质量。

现在同一参数也控制自动体型传递的前掌保形：先分离宿主自动传递与衣物自身调整，再对自动传递结果拟合整体位移、旋转和统一缩放。衣物自身的调整 Morph、当前姿势驱动的自身修正仍保留，原生匹配和表面投射两条通路都覆盖；改变自身 Morph 会使保形缓存失效。0% 仍精确恢复原始形变。

碰撞阶段保留前掌整体拟合，并限制 `Foot` 连接区相邻点的碰撞位移差，避免在过渡带重新挤出尖峰；约束只作用于脚部，前掌已保形的顶点作为固定边界。它减少局部形变自由度，仍不保证零穿插。

同一份 test4 场景中的两双鞋已验证强度切换、关闭复原、重复求值、人物及其他衣物不变，以及保存重开。第一双左右 `Foot` 区域超过原长度 2 倍的边，从 23 / 26 条降为 0 / 0 条；第二双从 42 / 36 条降为 3 / 0 条（仅统计蒙皮前长度超过 0.3 mm 的边）。四个前掌在自动 Morph 阶段与原鞋整体相似变换的剩余误差均低于 0.001 mm。细分接触复核仍有局部穿插，最大深度约 6.94 mm / 6.59 mm。

最新场景与报告位于 `artifacts/forefoot/outline-final`。`edge-review/before-edge.png` 与 `after-edge.png` 使用相同相机和实际导出的细分网格；`source-edge.png` 是原始鞋型仅作整体角度、比例对齐并细分后的参照。原资产左侧本来就有一处轮廓凹口，保形会保留它；先前未细分、逐面着色的诊断图还会放大控制网格折角的观感，不能替代这一组比较。几何指标见 `shape-comparison.json`，完整场景检查见 `forefoot-report.json`。

## 前沿整圈波浪与鞋面穿插复核

用户随后提供了正面和侧面的带材质截图，并指定根目录 `temp.dufex`。这份场景来自 `7.duf`，姿势与先前 test4 不同；保留的输入副本为 `artifacts/forefoot/temp-rim-input.dufex`。其中 Flip Flops 的前掌参数保存为 0%。复现确认整圈波浪在蒙皮后已经出现。旧算法设为 100% 能统一前沿，但按 Toe 面分区处理整个前掌，也把部分鞋面压入脚部，不能视为可用结果。

本轮把 Morph、平均蒙皮与碰撞统一到静止鞋型中的连续支撑范围。左右分别处理；从 Foot 与 Toe 分区估计前后方向，前掌鞋底完整保形，向后方与高处鞋面用五次平滑函数过渡。统一范围跨越面分区边界，高处鞋面的原始蒙皮和碰撞避让保持柔性。面积平均只采样支撑区内的前掌鞋底，避免高密度装饰物和鞋面偏置结果。自身调整 Morph、旧场景 0% 行为、参数标识与保存格式保持不变。

这一版鞋底高度范围是相对于静止鞋长的几何估计，并非语义识别所有鞋型。后续厚底修正见文末；严重倾斜的静止鞋型仍需单独验证。原鞋自身的侧边凹口继续保留；这次处理的是绑定与变形额外产生的整圈波浪。

验证包括带高度差的鞋底／鞋面共用 Toe 分区、鞋底前沿被标为 Foot、左右独立，以及原有强度切换、自动 Morph、自身 Morph 和精确复原测试。完整 CTest 53 项通过。用户场景只调整 Flip Flops，其他对象保持不变；0% / 50% / 100%、重复求值、关闭复原和保存重开通过，结果见 `artifacts/forefoot/rim-final/forefoot-report.json`。原 test4 两双鞋也通过同一组检查，结果见 `artifacts/forefoot/rim-test4`。

用户场景右鞋前沿 1307 个顶点与原鞋最佳整体仿射变换的最大剩余偏差，从 18.44 mm 降至 0.001 mm 以下；这是保形指标，不是接触质量指标。实际细分网格的顶点与三角形中心采样中，用户场景鞋子未发现超过 0.5 mm 的穿插；test4 两双鞋最大确认深度约为 0.84 mm / 1.38 mm，均无超过 2 mm 的样本。采样不能证明任意位置、姿势都无穿插，详见对应 `geometry` 目录中的 `inside-check.json`。

`rim-final/review` 保留同一相机下的正面、侧面、底部和分阶段网格图；`app-front` 与 `app-side` 使用部署后的正式程序渲染带材质近景。基础 DAZ 资产保持只读。

验证后仅将根目录 `temp.dufex` 中 Flip Flops 的 `forefoot_uniformity` 从 0 写为 1，其他 JSON 数据逐项核对不变；原文件完整备份位于 `artifacts/forefoot/temp-rim-input.dufex`。更新记录与可直接打开的副本分别为 `rim-final/temp-update.json` 和 `rim-final/temp.dufex`。


## G9 鞋类到 G8 / G8.1 女性

选中 G8 / G8.1 女性后加载 G9 鞋类，或把鞋子的 Fit To 改到该角色。当前限定资产声明 `/Genesis 9/Base` 和 `Follower/Wardrobe/Footwear`（包括子类型），并且正权重只使用左右小腿、脚、跖骨和脚趾。未验证的额外带权重骨骼会明确报错，避免静默丢失形变。此范围不代表支持全部 G9 衣物或高筒靴。

几何转换使用目标女性的 `CloneGenesis9`：在 G8 女性拓扑上建立 G9 参考表面，再传递 G8 女性基础形体与当前 Morph 的差值。本机参考位于 `C:/Users/Public/Documents/My DAZ 3D Library/data/DAZ 3D/Genesis 8/Female/Morphs/DAZ 3D/Clones/CloneGenesis9.dsf`。缺少兼容 Clone 时会报错；不会回退使用男性 Clone。同一人物同时穿 G8 男装和 G9 鞋类时，各自使用独立参考缓存。

这个 Clone 的形体差值不包含两代角色站姿的全部差异，必须同时读取同一角色目录下的 `Tools/Clones/CloneGenesis9.dsj`。该模板把 G9 服装转换到 Clone 使用的参考站姿，再进行表面绑定；缺少它会导致鞋子跟随动作但整体偏离脚部。模板只作为运行时的几何参考，鞋子自身 Morph 的位移方向也随模板旋转，不修改用户的人物姿势或源资产。

G9 的 `l/r_foot`、`l/r_metatarsal`、`l/r_toes`、`l/r_shin` 映射到 G8 对应骨骼。实际蒙皮使用 G8 的完整父子链与旋转顺序，保留目标独有的腿部扭转和跖骨祖先变换，避免只改骨骼名称后漏掉动作。鞋子本身的权重、可用调整 Morph 和相对骨骼编辑仍参与求值；源资产不被改写。G9 的 `l_toes / r_toes` 骨骼及脚趾分组也可使用已有的“前掌统一权重”参数。

专项测试入口：

```powershell
build/vs2022/bin/Release/MaleToFemaleFitTest.exe --g9 artifacts/g9-footwear/input.dufex "H:/G1/People/Genesis 9/Poses/Aurora/Spring Cleaning/01 SC Clothing/Apron Suit/SC 05 AS Slipper.duf" artifacts/g9-footwear/final
```

测试使用当前 `temp.dufex` 的副本，保留人物体型、姿势、材质和其他衣物，只在副本中隐藏同一人物原有鞋子，以便观察新拖鞋。检查前掌 0% / 50% / 100%、关闭恢复、重复求值、目标跖骨动作、人体 Morph、鞋子自身 ExpandAll、解除重绑和保存重开。单元测试另外覆盖两种蒙皮方法、目标独有父骨骼引起的缓存失效、缺失 Clone、范围外带权重骨骼和参考缓存隔离。

`SC Slipper` 已通过上述真实场景测试，场景为 `artifacts/g9-footwear/final/g9-slipper.dufex`，前掌参数设为 100%。两只鞋的正面、侧面及底部诊断图位于 `final/review` 与 `final/left`。鞋底前沿保持平顺，但左脚趾下方仍有局部接触误差：实际细分网格采样最大确认穿插为局部坐标 4.41 mm，按当前对象缩放约 5.34 mm；超过局部 2 mm 的样本有 80 个。样本数量不代表穿插面积，也不能证明其他姿势的接触质量。详情见 `final/geometry/39/inside-check.json` 和 `final/geometry-review.json`。

最终 VS2022 Release 构建退出码为 0，53 项 CTest 全部通过，454 个源码文件及可执行文件与部署清单一致。正式程序正面和侧面验收均通过，128 样本的带材质图位于 `final/app-front/reference.png`、`final/app-side/reference.png`。Qt 部署工具仍提示缺少 `dxcompiler.dll / dxil.dll`，本次部署、测试与 OptiX 渲染均已完成。

原 Flip Flops 的类型仅声明为通用服装，测试副本已显式隐藏该对象，避免与新鞋叠加；最终两张正式程序图确认只显示 SC Slipper。可见性修订记录在 `final/visibility-fix.json`。按用户默认保存习惯，将验证后的场景复制到根目录 `temp.dufex`，写入前确认原文件未被其他操作修改；原场景完整备份为 `artifacts/g9-footwear/input.dufex`，更新散列记录在 `final/temp-update.json`。

## 厚底拖鞋的局部增厚

用户指出 SC Slipper 在前掌统一权重 100% 时，前掌与中足之间的鞋底异常增厚；同时关闭 `l_foot` 和 `l_toes` 可以得到更平直的参照。输入场景备份为 `artifacts/sole-thickness/input.dufex`，关闭两个区域的参照为 `left-disabled.dufex`。分阶段几何比较确认，主要厚度差在自动 Morph 和蒙皮阶段已经产生，不能靠继续调整碰撞参数解决。

原先的高度过渡从静止鞋底最低点上方约 8% 鞋长处开始，至约 12% 鞋长处结束。这条过渡带可能穿过厚鞋底内部，使同一截面的上下表面使用不同的 Morph 保形强度和平均蒙皮权重。100% 只代表乘在整个空间范围上的参数强度，不代表过渡区内每个顶点均为完整保形。

现在从静止参考网格的竖直截面寻找主体封闭层，排除远薄于主体的鞋面、底纹和装饰片，并建立左右独立的鞋底上沿高度场。在规则空间网格上补齐倒角边缘并平滑插值，避免逐顶点射线在鞋垫投影边缘突然命中鞋面而形成硬折。需要扩展时，保形范围覆盖鞋底完整厚度，再向鞋面平滑退出；同一范围继续用于自动 Morph、蒙皮和碰撞。

如果原高度范围已覆盖主体鞋底，就保留既有处理；新增范围也只扩大、不缩窄原保护区。这样避免薄底鞋因重新划分过渡带而出现鞋面穿插。没有可识别封闭层的网格继续使用原几何估计。此方法仍基于鞋型的几何特征，不能视为任意复杂鞋底、悬空饰件或倾斜参考姿态的语义识别。

没有增加参数，也没有关闭脚部形变区域。新增单元测试覆盖厚底上下表面一致、薄鞋面及底纹排除、超出足床投影的鞋面、姿势后的厚度一致性，以及已覆盖薄底的原过渡保持不变。VS2022 Release 构建与 53 项 CTest 通过；SC Slipper 和原 test4 两双鞋分别执行真实场景的强度切换、关闭复原、重复求值与保存重开检查。test4 两双鞋的自动 Morph、蒙皮后及碰撞后基础网格与上次通过验收的 `rim-test4` 结果逐顶点完全一致。

最终场景和报告位于 `artifacts/sole-thickness/verified`。厚度统计在同一原鞋上取竖直截面的上下对应点，再随各阶段网格映射，使用点对距离避免姿势旋转造成投影误差。151 处有效截面中，以原始厚度中位数的 75%～125% 定义主体鞋底，共 146 处；其余 5 处跨入侧帮或倒角，完整统计也保留在 `measurement.json`，不作为主体足床厚度。

主体鞋底碰撞后的厚度比例中位数由 1.1236 变为 1.1647，最大值由 1.5554 降至 1.1766；最大值相对于中位数的额外增厚由 38.42% 降为 1.02%。关闭 `l_foot / l_toes` 的参照约为 0.12%。此指标去除了整体缩放，只衡量局部厚度是否出现突增，不代表整只鞋完全刚性或接触完全正确。

最终实际细分网格仍有脚趾处穿插，最大确认深度为局部坐标 2.587 mm，乘当前对象缩放约为 3.128 mm；超过局部 2 mm 的样本有 58 个。原 test4 两双鞋分别为约 0.844 mm / 1.375 mm，均无超过 2 mm 的样本。接触报告见各 `geometry/<host>/inside-check.json`。正式部署程序侧面截图与验收结果位于 `verified/app-side`；构建清单核对与场景保存记录为 `build-verification.json`、`temp-update.json`。

## Fit To 后姿势编辑性能

厚底效果确认后的 `temp.dufex` 留存于 `artifacts/fit-performance/input.dufex`。优化前，脚部 FK、小腿 FK、腿部 IK 及姿势恢复的 CPU 形变求值分别约为 9.73 / 9.97 / 11.25 / 11.00 秒。正式程序从姿势提交到完整材质首帧约为 11.26 / 11.19 / 11.82 / 11.35 秒；姿势应用本身只需约 0.6 秒，主要等待后台碰撞完成。鞋底范围、统一权重及固定绑定已有内存缓存，不是每次姿势编辑的主要耗时。

碰撞优化保留原迭代次数、细分接触和保形约束。反向查询沿用原有穿透深度上限做保守距离裁剪；静止参考局部范围也加入空间树节点裁剪。迭代之间仅更新衣物空间树包围盒，避免反复重建拓扑分区；大网格的首次构建并行执行。上一轮命中三角形只提供保守搜索上界，仍搜索并验证最近点；上界带数值余量，防止三角形插值与包围盒距离的浮点舍入差异导致漏查。串行诊断路径保留完整重建和无界查询，作为独立参照。

自动 Morph 投射只比较本件衣物实际绑定的身体顶点及旋转邻域。其他部位变化时，复用已生成的投射结果；绑定邻域或衣物约束变化仍正常失效。鞋底碰撞迭代中不变的边长上限移至循环外计算，临时缓冲重复使用。

独立且不可见的衣物延后碰撞，在重新显示时校验当前姿势并恢复完整结果。其他对象的 Fit To 宿主、碰撞目标、Geometry Shell 来源或共享网格即使隐藏仍保留计算。此优化只延后不参与显示和依赖的碰撞，隐藏衣物的蒙皮输入仍随姿势更新。

本轮没有增加磁盘 `.dufcache`。实测热点是随姿势变化的动态接触；固定绑定和投射依赖的内存复用即可获益。姿势或材质编辑会使其失效的动态结果不适合按 Morph 路径落盘。

真实场景计时与几何对照入口：

```powershell
build/vs2022/bin/Release/MaleToFemaleFitTest.exe --pose-profile artifacts/fit-performance/input.dufex artifacts/fit-performance/after artifacts/fit-performance/before
out/vs2022/Release/DazFastViewer.exe --file artifacts/fit-performance/input.dufex --fit-pose-test "SC Slipper" --output artifacts/fit-performance/app-after
```

首帧测试使用与交互提交相同的渲染快照通路，依次修改脚部 FK、小腿 FK、腿部 IK，再恢复原姿势；不包括加载场景的时间，不把拖动期间的灰模预览算作材质首帧。几何对照保留每个步骤的完整网格二进制数据，并额外验证隐藏旧鞋重新显示后的结果。

最终同场景结果如下，单位为秒：

| 操作 | CPU 优化前 | CPU 优化后 | 材质首帧优化前 | 材质首帧优化后 |
|---|---:|---:|---:|---:|
| 脚部 FK | 9.730 | 2.754 | 11.259 | 3.805 |
| 小腿 FK | 9.969 | 2.805 | 11.188 | 3.945 |
| 腿部 IK | 11.248 | 2.839 | 11.824 | 4.025 |
| 恢复姿势 | 10.999 | 3.195 | 11.348 | 4.028 |

完整材质首帧等待减少约 64.5%～66.2%，约为原先速度的 2.8～3.0 倍；当前约 771 万渲染三角形的场景仍需约 4 秒，不代表即时完整渲染。当前姿势重复求值约 0.26 ms。所有步骤的可见网格与优化前逐顶点完全一致，隐藏旧鞋重新显示后的可见结果也完全一致。隐藏且无依赖对象的未显示几何允许暂不执行碰撞，不能把这部分延后计算误记为可见几何误差。

最终 53 项 CTest、正式程序首帧验收均通过；456 个源码文件及可执行文件与部署清单一致。完整记录见 `artifacts/fit-performance/performance-comparison.json`，CPU 原始记录在 `before/profile.json`、`pass3/profile.json`，程序首帧原始记录在 `app-before/editor-check.json`、`app-after/editor-check.json`。根目录 `temp.dufex` 与本轮输入备份散列一致，性能测试未改写用户场景。

## 同基型上衣的胸片 Morph 连续性

`artifacts/torso-fit/input.dufex` 保存了本次用户最后关闭的 `17b.duf` 派生场景。分别检查 Sue Yee School uniform Shirt 和 GymClass TShirt 的作者 Morph、自动投射、蒙皮、碰撞阶段，确认尖锐折叠首先出现在自动投射阶段。另一件 SSO 上衣在保存状态中隐藏，服装重叠不是本次原因；也未发现这两件上衣的有效体型值被限幅截断。

Sue Yee 上衣没有配套的 BodySize、Voluptuous、BreastsSize 等常用体型 Morph，需要自动投射这些形体；Gym Class 提供其中三个，剩余 Youth、Heavy 和其他体型仍需自动传递。原先同基型投射在离皮肤 3 cm 内完全不平滑，身体表面的胸下沟和两侧最近面切换会形成不连续的衣物位移。跨基型上衣已有的连续性处理没有覆盖这条同基型路径。

修复将连续布片处理扩展到同基型上身衣物：按实际胸片分组识别，平滑自动生成的位移，再沿向外方向平顺局部凹陷。分组边界使用渐变权重，新增迭代只影响胸腹区域。衣服原始几何、作者 Morph、口袋和门襟保持独立，角色本身不改变。分类为 Skirt 或错误标为 Actor/Character、实际声明了衣物适配基型并包含胸片的商业上衣也参与；真正角色、GeoGraft 和几何壳排除。没有胸片的鞋和裙摆不新增处理，明确分类的内衣与泳装保留原有贴体行为。

该处理复用已有投射缓存，仅在相关体型或修正位移变化时更新，不新增 `.dufcache`。`ConformRuntimeTest` 覆盖局部凹槽的连续过渡、错误资源分类、作者 Morph 保留、归零恢复、重复求值缓存和往返无漂移。诊断入口为 `MaleToFemaleFitTest --torso-scene <dufex> <输出目录>`；`--torso-replay` 用已提取的数值 fixture 单独排查自动投射。仅衣物灰模的同视角阶段对照位于 `artifacts/torso-fit/before` 和 `after`，不应把该修复表述成完整 dForce 布料模拟。

最终材质检查还发现，身体碰撞会将衬衫内层顶过外层口袋。新增同基型上衣的邻近布层约束：在静止网格上识别同向、非相邻的近距离布面，缓存顶点及面内部的对应关系；碰撞结束后恢复本次蒙皮输入的正向间距，只移动外层。它保留当前体型与作者 Morph 确定的间距，不保存姿势结果，也不将上帧输出反馈回输入。明确的贴体内衣、真正角色、GeoGraft、几何壳和既有跨基型鞋类路径不参与此项处理。

`ConformRuntimeTest` 另覆盖身体同时挤压两层布料时仍保留层间距离、碰撞缓存命中和重复输入无累积。`--torso-collision` 对这两件诊断上衣回放资产原有的 3 次碰撞、2 次平滑设置；与新增层间处理关闭时的完整场景输出逐顶点一致。最终层间几何放在 `artifacts/torso-fit/layers`，程序材质截图放在 `app-sue-final`、`app-gym`。初次静止绑定的成本与每次姿势更新的成本应分别计量。

最终验收记录为 `artifacts/torso-fit/verification.json`：53 项 CTest 通过，457 个源码文件与部署清单一致，用户原 `temp.dufex` 未改写。两件上衣的材质渲染均已检查；此前口袋因内外层相交形成的斑驳已消除。旧鞋类场景的 8 个步骤中，G8 凉鞋和 G9 拖鞋均与原基线逐顶点一致。独立运行的 FK、IK、恢复姿势 CPU 形变耗时约 3.06～3.21 秒（旧记录约 2.75～3.19 秒）；新增静止布层绑定在首次求值共约 0.78 秒，后续这些姿势步骤没有重复执行绑定或布层修正。这些数值只统计 CPU 形变，完整材质首帧需另行测量。
