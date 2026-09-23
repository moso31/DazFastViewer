# 多选、曲面细分与属性数值修复

本轮为 007 后的追加修复，008 未开始。

## 多选与聚焦

- 场景树使用扩展选择；Ctrl＋左键增选／取消，普通点击替换选择。
- 原生视口保留点击时的 Ctrl 状态。Ctrl 点击空白保留选择，普通空白点击清空。
- 多选传递给渲染线程；F 与鼠标侧键使用所有选中项的包围盒并集。包含对象、骨骼、实例组和灯光，普通场景组展开为其对象。
- 聚焦等待对应文档、选择集合与编辑版本，避免刚选完就按 F 时使用上一项的范围。
- 属性编辑仍作用于当前活动项；取消活动项后使用其余选中项。此轮未扩展为批量属性编辑。

### 同角色骨骼 Ctrl 多选补修

此前视口把 Ctrl 点击统一当作“角色尚未选中”，因此左中指末节已经选中时，Ctrl 点击右中指末节会退回角色整体。现在从完整多选集合读取命中角色的选择层级：该角色已有骨骼选中时，Ctrl 增选／取消实际命中的骨骼；只有整体选择或尚未选择时，仍增选／取消整个角色。活动项切换到另一角色不会丢失原角色的骨骼上下文。

悬停高亮与点击使用同一判定，随 Ctrl 按下／释放同步切换。头部多选保留 Head → 眼睛／嘴唇的细选层级；F 继续使用所有选中骨骼的范围并集。

CPU 和 Qt 回归覆盖同一角色左右指尖增选／取消、跨角色活动项切换、整体角色切换以及头部细选顺序，`scene_workflow`、`parameter_controls`、`camera_mailbox` 三项测试通过。副屏独立夹具复现命令：

```powershell
python tools/make_joint_selection_fixture.py artifacts/joint-selection-fixture
out/DazFastViewer.exe --self-test --joint-selection-test --file artifacts/joint-selection-fixture/scene.duf --project artifacts/joint-selection-fixture/project.json --output artifacts/joint-selection-run
```

副屏夹具通过视口 Ctrl 增选右中指末节、F 聚焦双指尖、激活另一角色后 Ctrl 取消左指尖、F 聚焦剩余右指尖四项检查。记录见 `artifacts/joint-selection-run-3/editor-check.json` 与 `joint-multi-focus.png`。原有两对象多选／细分／数值联动复测也通过，见 `artifacts/edit-regression-after-joint-fix/editor-check.json`。这里使用独立合成骨架验证交互，没有重跑真实角色场景；`out/DazFastViewer.exe` 与诊断视口均已重新构建、暂存。

## DAZ 曲面细分

导入 DSF 默认值与 DUF 场景覆盖：`current_subdivision_level`、`edge_interpolation_mode`、`subd_normal_smoothing_mode`，以及 `studio_geometry_channels` 下的 `SubDIALevel`、`SubDRenderLevel`、`SubDAlgorithmControl`、`SubDEdgeInterpolateLevel`、`SubDNormalSmoothing`。不同细分设置的同源对象不会误共享设置。

对象属性的 `/General/Mesh Resolution` 提供分辨率、视口级别、渲染最低级别、算法、边界插值和法线模式。基础模式不细分；视口使用视口级别，离线 `--smoke` 使用视口级别与渲染最低级别的较大值。诊断视口也可使用 `--render-subdivision`。

原始三角面／四边面与逐角 UV 保留在 IR 中。渲染通过 OpenSubdiv 3.7.0 的 Catmark、Bilinear 或 Loop 生成几何；Catmark 不使用预先三角化的四边面。细分发生在 Morph、蒙皮及接缝约束之后。材质槽、UV 接缝和源面隐藏继承到细分结果。模板与 UV 缓存，后续形变只插值顶点；相机移动不重新细分。改变细分设置会重建渲染场景，保持当前姿势和 Morph。

边界：

- 当前为均匀 SubD；不包含 Iray 屏幕自适应细分和 DAZ HD Morph。
- Legacy Catmull-Clark 用 Catmark 近似，并记录诊断。
- Preserve Cage 原值保留，但基础网格的分裂法线尚未完整实现；UI 标注待支持，渲染使用现有平滑法线。
- UV 使用线性插值。拾取／悬停及聚焦仍使用基础控制网格；细分轮廓与控制网格可能存在局部差异。
- 当前允许 0–6 级，每网格最多预计 1600 万个细分面；超过时明确报错，不静默降低保存值。

实现语义参考 [DAZ geometry](https://docs.daz3d.com/public/dson_spec/object_definitions/geometry/start) 与 [DzFacetMesh](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/scripting/api_reference/object_index/facetmesh_dz)。第三方使用记录及许可见 [OpenSubdiv 来源记录](../third_party/opensubdiv/SOURCE.md)。

## 属性数值

Scale 的原始保存值与 ERC 求值后的最终值可能不同。例如原值 `0.02` 加上 ERC 输出 `-0.01`，最终应显示 `1%`；旧 UI 显示原值，表现为多了 1 个百分点。属性面板现在读取最终根骨骼缩放因子，并在用户输入绝对百分比时做逆向换算。原始文件和 ERC 公式不做人工加减修补。

Morph 与对象缩放的 float 值按最短可往返十进制显示，后台同步容忍百分比换算引入的舍入差异。`100.01` 不再展开为 `100.010002`，六位小数输入仍可保留。环境等 double 参数不统一截短。

## 验证

- 完整 15 项 CTest 通过，新增 `subdivision`；控件测试包含实际 Enter、Tab、后台刷新和 Ctrl 鼠标点击。
- 副屏 OptiX 回归通过，原始记录：`artifacts/edit-regression-run/editor-check.json`，窗口坐标 `(2729,55)`，尺寸 `1580×920`。
- 两对象 Ctrl 增选、取消及两条 F 聚焦路径通过。
- 细分 1 → 2 级时，夹具总三角面数从 98 增至 242；细分之后的 Morph 产生实际几何增量。
- 保存 2% 且 ERC 扣除 1 个百分点的夹具显示 1%；输入 100.01 后读回及显示为 100.01。
- CPU 回归覆盖视口／离线级别、Catmark／Bilinear／Loop、角点规则、UV、隐藏面、重复形变无漂移、场景覆盖及不同实例设置隔离。

复现副屏检查：先执行 `python tools/make_edit_regression_fixture.py artifacts/edit-regression-fixture`，再执行 `out/DazFastViewer.exe --self-test --edit-regression-test --file artifacts/edit-regression-fixture/scene.duf --project artifacts/edit-regression-fixture/project.json --output artifacts/edit-regression-run`。

真实 `H:/G1/Scenes/test3.duf` 在副屏加载／渲染通过：23 个对象、22 套蒙皮；19 个对象启用细分，其中 2 个为 2 级、17 个为 1 级；Cycles 实际提交 3,177,370 个唯一三角面。角色 B 的 DUF 保存 Scale 为 161%，截图显示包含 ERC 的最终 160%。记录见 `artifacts/subdivision-test3-run/editor-check.json`、`editor-render.json`、`scene.png`。本轮截图只验证加载与形态，不用于噪点收敛或 DAZ / Iray Golden 验收。
