# 多内容库与参数发现修复

日期：2026-09-21。用户指出 `Arms Length`、`Chest Scale`、`Eyes Closed`、`HS Sanny Shy` 缺失，并补充四个内容库根目录。本次已定位并修复发现链路，未将尚未实现的公式 / 蒙皮求值宣称为可用。

## 根因与修改

1. 原启动脚本只传入样例所属库，菜单选择内容库也没有持久化。现在通过“项目 → 项目设置…”管理有序的多内容库，支持添加、移除、上移、下移并原子保存。根目录统一用于 DUF / DSF / 纹理依赖和参数扫描；同路径优先采用靠前的库。
2. 原发现器虽然递归扫描了 `Morphs` 子目录，但只保留 float 且具有非空顶点差值的项，丢弃了纯控制器和 Alias，并仅接受几何父对象。现在保留图形对象及子节点的控制器 / 别名，原样保留 `group`、所属节点、来源、公式数量和可用性；同名不同文件的别名不再因 ID 重复被丢弃。
3. 旧 gzip 读取器仅接受一个 member。真实资源中存在合法的连续 gzip member（部分尾部为一个空 member），已按多段流解压，同时保持校验和、总大小和非法尾部检查。这修复了 57 个先前读取失败的文件。
4. 未验证目标拓扑的 Morph 继续禁止应用顶点差值，但参数元数据保留，不能把“尚不能应用”误报为“没有发现”。实际仍有两份 Neko 修正文件的 `deltas.count` 与内容不符，记录为解析诊断，未修改原资产。
5. 参数引用补充递归发现目录外的依赖，并报告文件 / 通道缺失、歧义和空覆盖。节点依赖仅确认属于已知角色资产，尚不执行骨骼 / Formula；依赖可解析不等于完整变形已支持。

## 指定参数核对

以下都已在 Genesis 8 Female 的多库目录中找到，来源均为 `H:\g3\data\DAZ 3D\Genesis 8\Female\Morphs` 下的资源：

| 参数 | 相对路径 | 原始分组 | 实际类型 / 当前限制 |
|---|---|---|---|
| Arms Length（用户所述 ArmLength） | `DAZ 3D/Body/SCLArmsLength.dsf` | `/Real World` | 4 条公式，骨骼缩放控制器；没有直接顶点差值 |
| Chest Scale | `DAZ 3D/Base/SCLPropagatingChest.dsf` | `/Real World` | 18 条公式，骨骼缩放控制器；没有直接顶点差值 |
| Eyes Closed | `DAZ 3D/Base Pose Head/eCTRLEyesClosed.dsf` | `/Pose Controls/Head/Eyes` | 2 条公式驱动左右眼通道；另有 head 节点 Alias |
| HS Sanny Shy | `Hamster/Sanny/CRTLHSSannyShy.dsf` | `/Pose Controls/Head/Expressions/Sanny Expression` | 4,000 个顶点偏移、456 条公式；尚缺完整公式及骨骼配合 |

上述参数引用在 G8 检查中均能解析，但当前仍不可编辑。G8.1 中前三者的情况不能照搬：旧的 `Eyes Closed` 总控制器由 8.1 的同相对路径空文件屏蔽，不应通过依赖递归重新引入；其余三项仍能发现。已有的 Genesis 8.1 样例与本轮 Genesis 8 样例需分开看待。

## 最新统计与验证

统计是参数条目，包含子节点别名，不是独立 Morph 的数量，也不是全部可求值数量。

| 目标角色 | 读取文件 | 参数条目 | 默认可见 | 可直接编辑（含隐藏） | 默认可见且可编辑 |
|---|---:|---:|---:|---:|---:|
| Genesis 8 Female | 6,019 | 6,012 | 3,470 | 1,001 | 720 |
| Genesis 8.1 Female | 7,085 | 6,799 | 3,875 | 1,028 | 745 |

两份目录各有 2 条差值 count 不一致诊断；发现清单和未解析引用留在本地报告，不能宣称全部商业资源兼容。G8.1 最后一次无窗口扫描耗时约 52 秒；尚未实现持久化目录索引缓存。

- 五项轻量回归通过：相机邮箱、IR / DSON、Morph Runtime、多库目录、项目配置。多段 gzip 修复后仅重跑受影响的三项，全部通过。
- 目录用例覆盖多根目录优先级、递归子目录、中文路径 / 分组、纯控制器、同 ID 子节点别名、外部参数依赖、缺失引用、未验证拓扑元数据保留及 8.1 空覆盖不可绕过。
- 配置用例验证保存后重新读取、优先级顺序、Unicode、大小写去重、无效路径写入不破坏已有配置、损坏 JSON 明确失败。
- 新参数树经过一次实际副屏交互检查：Bodybuilder 滑块、变换、重置、相机均通过；网格始终 2 个，顶点与变换各更新 2 次。最后选择 `HS Sanny Shy` 并保存分组树截图。
- 最后仅调整参数树缩进、详情区高度和搜索匹配计数，已重新编译，未重复渲染检查。未重跑性能基准或 DAZ Golden。

本地证据：`artifacts/content-libraries/editor-g8/` 与 `artifacts/content-libraries/g81-final-catalog.json`。可入库摘要见 [evidence/content-libraries.json](evidence/content-libraries.json)。商业资产及包含大量商业参数名称的完整报告不提交 Git。

## 配置与使用

本机项目文件为 `DazFastViewer.project.json`，按用户顺序保存：

1. `H:\g1`
2. `H:\g3`
3. `C:\Users\Public\Documents\My DAZ 3D Library`
4. `C:\Users\xatia\Documents\DAZ 3D\Studio\My Library`

该文件由菜单维护并忽略 Git；新检出可使用 `configs/project.example.json`。菜单保存后重新扫描当前场景，按对象 / 参数 ID 保留仍然兼容的 Morph 编辑值与变换。若根目录优先级改变导致实际参数来源变化，该参数重新采用新来源的初始值。内容浏览器顶部可切换已配置的库。

```cmd
cd /d D:\Github\DazFastViewer
powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Genesis8
```

选择角色，搜索参数，点击树中的具体参数后查看下方支持状态与编辑区。“显示隐藏参数”控制隐藏通道；在详情处悬停可查看来源和未解析引用数量。需要完整操纵 Arms Length / Chest Scale / Eyes Closed / HS Sanny Shy，仍需后续 Skeleton / Skinning 与 ERC 求值；本轮不以直接取消禁用替代实现。

实现依据：[DSON modifier](https://docs.daz3d.com/public/dson_spec/object_definitions/modifier/start)、[DSON channel_alias](https://docs.daz3d.com/public/dson_spec/object_definitions/channel_alias/start)、[DSON formula](https://docs.daz3d.com/public/dson_spec/object_definitions/formula/start)、[Qt QSaveFile](https://doc.qt.io/qt-6/qsavefile.html)。具体四项的分类、分组及数量直接来自用户本地 DSF。
