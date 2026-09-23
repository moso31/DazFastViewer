# 脸部材质、实例选取与远距离聚焦修复

本轮继续处理 007 之后的真实场景兼容性，008 未开始。用户提到的 `test5.cuf` 按已有 `H:/G1/Scenes/test5.duf` 验证。原始 DAZ 文件和内容库保持只读，GUI 检查使用副屏。

## 材质原因与改动

`14.duf` 的 `tiny_02`（`Genesis8_1Female-2`）脸部没有直接使用 `image_file`，而是引用 `image_library` 中的 `HS_Keicy_facenoeyebrows_d14`。它包含脸部底图、Yamazaki 眉毛和 Keicy 妆容三层；透光颜色另有对应三层贴图。原加载器漏读 `image`，使白色常量替代脸部颜色。

现在解析图像库引用、百分号编码和引用所属文档；保留有效图层、顺序、混合方式、透明度、反转及变换。图层先在图像编码空间叠加，再统一转换到线性工作空间。支持本场景实际使用的 alpha blend、multiply，以及 add、subtract；其他混合模式和独立蒙版仍明确记录诊断。资源回收按完整贴图身份去重，防止同底图不同妆容被合并。`image: null` 作为空引用处理；材质覆盖会清除旧的互斥贴图绑定。

实现依据为 [DAZ image 定义](https://docs.daz3d.com/public/dson_spec/object_definitions/image/start)和 [image_map 定义](https://docs.daz3d.com/public/dson_spec/object_definitions/image_map/start)。三层 GPU 合成不会写入或生成内容库贴图。

`test6.duf` 的两个指定角色实际使用 Iray Uber。此前 Mono 模式也取隐藏的 `SSS Color`，忽略 `SSS Amount`；非薄壁皮肤没有接入 `Translucency Color` 贴图。现在分别按 Mono 散射量／Chromatic 对数颜色计算散射系数，结合传输颜色与厘米测量距离形成米制半径和吸收比例；通过独立颜色的表面／散射闭包接入透光贴图。Scatter & Transmit 模式额外采用 SSS Reflectance Tint。

`big_01` 同时使用零粗糙度双瓣高光和强度 2 的凹凸图。原先统一的 1 毫米凹凸高度容易过度放大细纹。没有显式高度范围时，后端现在根据该材质的世界面积、UV 面积及贴图分辨率计算两个纹素的高度范围；显式厘米范围继续优先。DAZ 大于 1 的 Bump Strength 放大高度，而不是落入 Cycles 插值强度的上限。该尺度仍是跨渲染器近似，不能声明等价于 Iray 原生凹凸。

散射语义参考 [DAZ Iray Uber 说明](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/interface/panes/surfaces/shaders/iray_uber_shader/shader_general_concepts/start)及本机原生 MDL 参数定义；纹素尺度参考 [DAZ Importer 原始实现](https://github.com/Diffeomorphic/import_daz/blob/master/cycles.py)。本轮未复制其代码。Cycles Random Walk 与 Iray 体积传输仍有模型差异，本轮不是完整 Iray 材质 Golden 验收。

## 选取、聚焦和选项界面

- GPU 副本复用原网格，场景树仅保留 Instance 一级。普通实例的全部零件合并到其原生节点；打包散布的每个条目保留一个独立节点。点击任意零件都映射到同一个 Instance，高亮和聚焦使用整组可见几何；不再展示不可编辑的内部网格列表，也不为副本重新发现 Morph 或复制几何。当前副本支持点选、高亮、树定位与聚焦，未新增副本级 Morph、变换或删除操作。
- Fit To 的穿衣选择过滤保留，但有 GeoGraft 元数据的插件参与射线选取。宿主被插件遮盖的面仍剔除。
- `Base_Circle` 直径约 160 千米，聚焦距离约 320 千米，超过 Cycles 默认 100 千米远裁剪。相机改用 Cycles 的无限远裁剪约定；高亮投影随观察距离调整，滚轮也不再把远处相机截回 2000 米。
- Environment Mode 的 Sun-sky Only 移除旧的禁用／待支持标记。SS Day 使用年月日与日历控件，内部保留儒略日；SS Time 使用时分秒，内部保留当地时间秒数，UTC Offset 继续独立设置。天空仍沿用已有 Cycles 近似。

## 验证记录

工程 14 项 CTest 已通过，覆盖新增的 LIE、空图像引用、SSS 两种模式、GeoGraft 选取、日期时间转换、置换通道与 Instance 合并／追加身份隔离。记录位于 `artifacts/material-picking-followup/`：

| 检查 | 结果与证据 |
| --- | --- |
| `Genesis 8.1 Female` | `final-female` 渲染通过；固定机位皮肤从灰白恢复为带肤色。此轮与 Iray 仍非逐像素等价。 |
| `tiny_02` | `final-tiny02` 渲染通过；脸部底图、眉毛和妆容恢复，未再显示为白色常量。 |
| `big_01` | `fixed-big01` 正面观察中皮肤细节变平顺，曝光与原始场景一致。用户确认机位由其手动调整，渲染效果目前正常；据此记为用户视觉验收通过，不把该目录名称理解为与早期基线机位相同。 |
| `Base_Circle` | `final-focus-base`：F 与侧键均通过，距离 319581.25 米，截图有地面，几何更新为 0。 |
| 坦克 | `instance-tree-tank`：10 个副本各只有一个 Instance 树项，子网格树项为 0；整车高亮 387119 个三角形，射线与树选择一致，几何更新为 0。原型另保留正常结构，共 11 辆。 |
| 桌椅 | 用户已验收；收到“跳过”时已在后台完成的 `instance-tree-desks` 也通过，此后不再重复该项。 |
| 四个插件 | `select-four-grafts`：Golden Palace、HD Nipples、两个人物的 Genesis 8 Male Genitalia 均可点击、高亮和定位树项，几何更新为 0。基础形变接缝最大误差约 6.664×10⁻⁸ 米，Golden Palace 为 0；此值不包含着色器置换。 |
| 置换 | `displacement-gpu`：RTX 4070 Ti SUPER / OptiX，9 个阶段全部通过，黑／灰／白、反向范围等测试的顶点高度最大误差为 0，共享副本保持共用几何。真实 test5／14／test7 分别读到 16／7／2 个启用置换材质。 |

上述实例和插件 GUI 检查的视口 interop 读回均为 0；不是物理显示帧率测量。失败的早期 `after-*` 和未完成的 `final-pick-tank` 不作为验收证据；后者已由 Instance 一级的新测试替代。

采样上限 4096、自适应阈值 0.01、蓝噪声及关闭降噪的设置不变。脸部采集记录相机就绪后 15 秒画面，以及达到 256 样本或 60 秒时的画面。`final-big01` 后半段相机和 EV15 → EV13 是用户主动调整，属于正常查看；`fixed-big01` 的机位变化同样由用户确认是手动验收。`final-tiny02` 的后半段相机也有变化。上述结果不能据此声称同机位误差或同条件收敛速度；自动截图入口继续允许用户调整，不保留临时的界面禁用开关。

## 追加：材质置换

原链路只有 Bump / Normal，没有读取或连接 Displacement。本轮加入 Iray 和旧 DAZ / 3Delight 通道别名、激活开关、贴图、强度与带符号的最小／最大高度。高度按 `strength × (minimum + texture × (maximum − minimum))` 计算，DAZ 厘米转换成 IR 米；反向范围保留，不截成正数。没有贴图时不产生常量膨胀。

后端使用 Cycles 的 Displacement and Bump：现有网格顶点发生真实位移，细节由置换凹凸补充；使用对象空间，使实例缩放同步作用于高度。材质更新先恢复未置换位置，Morph 更新清除旧的未置换属性，避免高度累加或恢复漂移。共享实例保持共享几何。

依据为 [DAZ 置换说明](https://docs.daz3d.com/public/software/dazstudio/4/userguide/chapters/textures_surfaces_and_materials/start)、[Cycles 置换方法](https://docs.blender.org/manual/en/latest/render/materials/components/displacement.html)和[置换节点坐标空间](https://docs.blender.org/manual/en/latest/render/shader_nodes/displacement/displacement.html)。本轮没有加入 DAZ SubD Displacement Level 对应的细分／自适应细分，因此基础网格不足以表达的高频轮廓仍有差异；CPU 选取与悬停覆盖仍使用形变后的基础网格。不能将这次接入声明为 Iray 微置换完全等价。

真实设备验证入口为 `out/CyclesViewportBench.exe --displacement-check --output <目录>`。验证生成自有 DUF / EXR 夹具，检查黑、灰、白、反向范围、关闭和无贴图，并在同一 Session 检查材质编辑、关闭／恢复、Morph／恢复、相机与对象变换。检查对象是 GPU 求值后读回的网格位置，而非只比较表面亮度；该离线诊断与视口 interop 零读回指标分开。

## 最终发布

2026-09-24 已更新 `out/DazFastViewer.exe` 与运行依赖，源码哈希、构建产物与发布产物逐项一致。GUI 截图取自本轮首次置换发布包；其后只补充追加场景的 Instance 身份隔离并移除临时的界面禁用开关。追加修复后的 14 项 CTest 通过，最终包的 `displacement-gpu-final` 也通过全部 9 个阶段。

- 编辑器 SHA-256：`11e43a85c693df6cd8c742131014cd83f06302451bc28473347ac2b65b124da8`。
- 基准程序 SHA-256：`1a9bd100d028fed61036258e9e852a029fbc81068f49b9bf227461b234ce7c1c`。
- 汇总：`artifacts/material-picking-followup/validation-summary.json`；完整清单：`after-build-manifest.json`；回归日志：`ctest-final.log`。

用户原始场景及内容库未修改，008 未开始。本批修复、测试与文档按用户要求以中文说明提交至本地 Git。
