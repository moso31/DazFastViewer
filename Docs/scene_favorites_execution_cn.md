# DUF 场景收藏继承

2026-09-25：对象属性与 Morph 的“收藏”页读取 DUF 中节点 `extra` 内 `studio_node_channels.favorites`，沿用当前参数值。选中角色显示该角色的收藏，选中骨骼显示该骨骼的收藏。

## 行为

- 解析百分号编码、中文及 `参数名/Value` 写法，匹配参数原始名称与 ID，不按显示标签猜测。
- 支持整体缩放、平移、旋转、各轴缩放、骨骼变换和当前已提供的细分控件。
- 收藏与数值是否为零无关；隐藏参数继续受“显示隐藏参数”控制，不支持的参数继续保留原有禁用状态。
- 同源角色复用 Morph 目录时，收藏按场景节点独立读取；追加场景、刷新参数后继续使用各自来源。
- 角色收藏不会重复点亮骨骼上的同名别名。同名资源优先选择 DUF 实际引用的通道。
- 已保存的空收藏列表保持为空；没有保存收藏列表的节点继续兼容原有本机收藏。
- 手动加星或取消收藏以场景路径、对象身份、骨骼和参数为范围保存在本机设置中，优先于导入列表；不写入源 DUF。取消后立即从收藏页移除。切换对象或刷新目录保留当前分类。

## 实现与验证入口

解析与实例隔离位于 `src/daz/morphs.cpp`，面板映射、筛选和用户覆盖位于 `src/editor/parameters.cpp`，对象／骨骼变换通道对应关系位于 `src/editor/main.cpp`。

`ParameterControlsTest` 使用独立临时设置和完整 DUF／DSF 夹具验证：中文和空格名称、零值收藏、同名资源、骨骼别名、两个角色及同名骨骼隔离、空收藏、取消／新增、重新创建面板后的本机覆盖、跨场景隔离、源值与源文件不变。`SceneWorkflowTest` 验证追加后刷新仍保留收藏来源。

真实场景目录检查可运行 `MorphCatalogTest <scene.duf> <report.json> <content-root>... --lazy`；目录报告包含每个对象的 `favorites`。

## 完成结果

- Release 构建及 18 项 CTest 全部通过，包括新增的收藏界面与追加刷新回归。日志：`artifacts/favorites-release-build.log`。
- 使用项目中完整的四个内容库路径，以按需加载方式核对原始 `H:/G1/Scenes/test7.duf`：13 个对象全部对照一致；女性角色 26 项、下颌骨 1 项，两个男性角色各 14 项，共 55 项。证据：`artifacts/favorites/test7-catalog.json`、`test7-check.json`。
- 真实资产目录仍报告两个 Neko 眼睛 Morph 的差值 count 不一致；这些资源不属于本次收藏列表，未作为本次修复范围。
- 源 DUF 前后 SHA-256 一致，见 `artifacts/favorites/source-hash.json`。
- 最新编辑器已重新构建并部署到 `out/DazFastViewer.exe`，见 `artifacts/favorites-final-stage.log`。运行中的旧进程保留，重新启动后使用新版。

真实场景验证覆盖目录读取；收藏交互在 Qt 离屏测试中验证，没有进行新的 GPU 场景截图验收。

此功能继承当前界面能够表示的收藏参数，不增加新的通道类型，也不新增 DUF 保存／回写功能。
