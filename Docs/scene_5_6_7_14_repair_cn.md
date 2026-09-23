# test5、test6、test7 与 14 场景兼容修复

本轮属于 007 之后的场景兼容与性能修复，不启动 008。用户资产位于 `H:/G1/Scenes/`，只读引用；界面验证在副屏进行。详细证据保存在 `artifacts/scene-repair/`。

后续用户反馈的 GeoGraft 接缝、Group 层级与“新建后打开”环境丢失已另行修复并部署，见[追加修复记录](scene_followup_seams_groups_environment_cn.md)。下文“不含边界焊接”和程序哈希描述的是本轮历史版本。

## 已确认的问题及处理

- `test5`：DAZ 普通实例和 `studio/node/group_instance.instance_items` 原先没有进入 Render IR。现展开源节点的子树，共享源对象最终形变后的网格及材质，不为散布条目重复发现 Morph、蒙皮或创建高亮几何。场景包含 824 个人群散布条目和 10 个坦克复制体；每辆坦克引用 68 个可绘制子对象，与原件合计 11 辆。
- `test5`：三个炮口火焰保存为零缩放。跳过这些没有表面面积的对象并记录诊断，避免构造不可逆的编辑矩阵。另将求逆的固定行列式阈值改为相对轴长阈值，允许合法的微小实例比例。
- `test7`：此前只把 `graft.hidden_polys` 用于碰撞辅助，没有从实际绘制中排除。现在按明确的 Fit To 关系及目标顶点／多边形数量校验后，合并附加件遮盖面；宿主保留完整拓扑供 Morph 与骨架核对，Cycles、选取与高亮跳过遮盖面。不同角色的遮盖独立。附加件 `Visible=false` 不恢复宿主面；删除附加件重新计算剩余遮盖。
- `14`：`/resources/walnut_bump.png` 位于 DAZ 安装目录，原解析器只搜索内容库。现在补齐安装资源路径。真正缺失的贴图使用通道常量继续加载并保留诊断，严格模式仍拒绝；损坏几何、越界索引和引用循环仍报错。
- `14` 的走廊玻璃使用 DAZ Default 材质，顶层 `transparency` 保存的实际含义是 Opacity Strength，值为 0.2；此前完全遗漏而当成不透明面。补充顶层不透明度、光泽、折射、凹凸、法线和平铺通道，Iray 显式通道优先。语义参照 [DSON 材质定义](https://docs.daz3d.com/public/dson_spec/object_definitions/material/start)及 [DAZ 材质说明](https://docs.daz3d.com/public/software/dazstudio/4/userguide/chapters/textures_surfaces_and_materials/start)。这不等于支持任意第三方着色器。
- `14` 更关键的可见性问题：`Group-1` 在保存文件中的 `Visible=false`，其走廊和柜子不应出现在当前教室中。此前只读取子对象自身可见性，使隐藏走廊遮挡学生和采光。现继承全部父节点的 Visible／Renderable；实例另外区分“隐藏原型根节点”和“原型内部隐藏零件”，允许隐藏原型而显示独立副本。
- 大场景还暴露了一个显示问题：Cycles 收敛后的空 RenderWork 标记为零样本，可能抢在 UI 取图之前覆盖最后有效帧。显示驱动现在忽略这种更新，保留有效帧；增加 `cycles-progress.log` 记录渲染阶段，便于区分加载、场景同步和采样等待。
- 视觉复核追加两项材质修复：`14` 的桌椅／柜子用 NVIDIA MDL Walnut，其贴图通道叫 `Diffuse Color` 而非 `diffuse`，补充该颜色与基础粗糙度映射，避免白色家具；`test6` 部分头发的 Glossy Color／Top Coat Color 贴图接入 Cycles。旧材质中反向凹凸范围转换为正距离及 Invert 标记，不再导致整场景验证失败。
- `14` 的走廊包含独立缩放权重，`test5` 的鞋带包含不同轴的 Local 权重。对尚未激活的能力不再拒绝整个场景：独立缩放图只允许单位骨骼缩放，非等价 Local 图只允许静态骨架；后续启用不支持的变换仍明确拒绝，不静默按另一种蒙皮算法处理。

## Sun-sky Only 调研与首批支持

[DAZ 官方环境说明](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/interface/panes/render_settings/engine/nvidia_iray/environment/start)定义了太阳天空模式、经纬度／时间和太阳参数；[Blender 官方天空节点说明](https://docs.blender.org/manual/en/latest/render/shader_nodes/textures/sky.html)及本地 Cycles 源码确认当前后端具备原生多次散射天空与太阳盘采样能力。因此无需引入 DAZ Studio 或 Iray 运行时。

本轮接入 Cycles 多次散射天空，按保存的儒略日、当地秒数、UTC 时差、经纬度计算太阳方向。太阳历使用赤纬、赤经及当地时角，坐标为东 X、北 Y、天顶 Z；相关定义见 [NOAA 太阳位置资料](https://gml.noaa.gov/grad/solcalc/solareqns.PDF)。模式 2 忽略环境贴图和场景灯光，保留环境强度、Tint、Draw Dome、Dome Rotation；支持太阳盘尺寸／强度、Haze、SS Multiplier 和 RGB Unit Conversion 的初步映射。

这是跨渲染器近似，**不是 Iray 等价实现**。太阳历未包含近地平线折射；Haze 与 Cycles 气溶胶的映射、辐射单位与项目现有 EV13 预览尺度仍需 Golden 校准。Sun Node、穹顶倾斜、太阳辉光、蓝红偏色、地平线、夜色和虚拟地面尚未等价映射，诊断保留 `sun_sky_approximation`。用户提供的 `test5` 和 `14` 均使用日期／经纬度太阳，无 Sun Node。

## 验证记录

工程检查覆盖共享实例、普通／打包实例的放置、引用循环、微小缩放求逆、缺失贴图与严格模式、宿主面遮盖、角色隔离、关闭／删除附加件、隐藏面的射线选取，以及未激活蒙皮能力的放行和激活后的拒绝。

本轮项目约定的 14 项 CTest 全部通过，最终构建、测试和部署见 `build-release-final.log`、`build-14.log`、`stage-release.log`。直接不加筛选运行 CTest 还包含上游 `cycles_version`，它引用产品包不发布的 `out/cycles.exe`，因此该项为 Not Run；最终采用项目构建脚本原有的 14 项测试范围。

CPU 加载工具包含参数发现、公式编译、首次完整形变和选取树，不含 Qt/Cycles/GPU：`test5-cpu-final` 68.58 秒、`test6-cpu-final` 121.12 秒、`14-cpu-fixed` 129.87 秒，均 PASS。测量期间有其他开发工作，不作为隔离环境的加载性能基准。

`test5-release/interaction-initial.png` 实际画出两侧高楼、中间小楼、11 辆坦克及连续人群，MDL 颜色通道补齐后地面也恢复场景原有的绿色。1,650 个实例对象只创建 146 份网格，唯一三角形 2,259,736，实例展开后的绘制三角形 7,656,974；GPU 图像互操作无 CPU 回读。824 个人群条目和 680 个坦克子对象副本共享各自原型的最终网格。

`test5-release` 在 RTX 4070 Ti SUPER、副屏完成 10 项短操作和停靠、持续编辑、色调恢复边界检查。连续相机移动 2.015 秒，实际提交 50 个不同相机 epoch，约 **24.81 帧/秒**；首响应 37.11 毫秒，停止后 55.28 毫秒恢复完整分辨率。完整视口 758×836，移动预览 189×209、2 samples。导航期间新增会话、几何重建、Morph、蒙皮求值均为零；恢复后局部几何与所有实例矩阵一致。证据见 `interaction-analysis.json`、`navigation-summary.json`、`events.csv` 和原始截图。此前一轮约 24.83 帧/秒，最后材质修复后未出现性能退化。

上述帧率是 SwapBuffers 后不同视角的提交率，**不是物理扫描输出 Visible FPS，也不是最终完整分辨率的收敛帧率**。保持关闭降噪、最终采样设置不变。

四个 GeoGraft 的原资产遮盖数分别为：Golden Palace 112 个多边形、HD Nipples 964 个、两个 Male Genitalia 各 82 个。女性宿主共剔除 1,076 个多边形／2,152 个三角形，两个男性宿主各剔除 164 个三角形。原始拓扑保留，因此 Morph、蒙皮和面编号不变。尚未新增 GeoGraft 边界焊接或 SubD/HD 求值；这次验证针对重复宿主面的剔除，接缝细节仍受现有基础笼限制。

`test7-release` 在实际场景中依次切换并恢复四个插件，通过属性、Hierarchy 和渲染状态核对，每次几何重建数均为零；`visible-0-toggled.png` 的正面截图能看到胸部缺口。源宿主隐藏面始终排除，关闭 Visible 没有恢复旧面；背侧皮肤仍可绘制。截图含原始角色资产，仅作为本地技术验收证据。

热切换截图测试的 stderr 仍有 Cycles 的 `could not begin update` 日志：现有三缓冲在无空闲槽时主动跳过更新，Cycles 将回调返回 false 打印为 ERROR。四插件最终状态检查通过，渲染器 error／edit_error 为空，图像互操作无 CPU 回读；不能因此将日志描述为完全无告警。它与此前零样本覆盖有效帧的问题分开记录。

`test6-release` 宽视角实际画出六个角色、大小两部手机、上下铺钢架和床品，角色之间有自然遮挡。补齐高光／涂层颜色贴图后，右后方角色的棕色头发和红色衣物恢复；20 秒记录为 96 samples，约 24.76 秒的最终截图为 144 samples，降噪保持关闭。与较早 256 samples 的截图属于不同材质映射版本，不能直接比较采样速度。

`14-release` 最终发布包完成真实出图，20 秒记录为 1,296 samples。五个学生、桌椅实例和木柜纹理均可见，保存为隐藏的走廊不再出现或遮光；`editor-check.json` 为 PASS，渲染和互操作错误为空。其阳光强度与原 Iray 参考仍有差异，不能把这一张图当作曝光／材质 Golden 已通过。

可直接查看[城市实例截图](../artifacts/scene-repair/test5-release/interaction-initial.png)、[宿舍最终截图](../artifacts/scene-repair/test6-release/reference.png)和[教室最终截图](../artifacts/scene-repair/14-release/reference.png)。

Sun-sky 的曝光／天空模型、部分复杂毛发和 PBRSkin 仍属于跨渲染器近似，未宣称通过 Iray 逐像素材质 Golden。这里的“正常”限于已验证的加载、场景组成、遮盖关系、可见性和交互路径；材质参考差异另列，不以工程 PASS 替代全部视觉验收。

## 复现与部署

运行入口为 `out/DazFastViewer.exe`。同一发布包逐场景验证的完整参数保存在 `artifacts/scene-repair/release-commands.json`；构建清单为 `release-build-manifest.json`。编辑器 SHA-256：`f8e2d81cb6792b8d5e3a3abe0e67d5bcd59efd0884cd7adf367940fde2aade8f`。检查确认清单中的源码哈希均与工作树一致。

四场景最终 `editor-check.json` 均为 PASS，汇总见 `verification-summary.json`。其中 `test5-release` 为实例场景交互验证，`test6-release`／`14-release` 为实际画面捕获，`test7-release` 为四个插件的 Visible 切换与恢复；这些不同检查范围不能互相替代。

命令行补充 `--capture-view x,y,z,distance,yaw,pitch`，单位为米／弧度，用于固定已知场景视角；`--visibility-test` 可重复指定多个插件，逐个关闭、恢复和记录。相机交互测试包含两秒约 60 Hz 连续输入，避免短输入序列人为限制被测帧率。

本轮不修改 `H:/G1`、`H:/G3` 或其他内容库原件，不创建 Git 提交，不启动 008。
