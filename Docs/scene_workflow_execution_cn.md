# 007 后插入优化：场景工作流执行报告

日期：2026-09-21。主线仍为 007 已结束、008 未开始。本报告对应场景 / DUF / 选取 / 界面调整；用户随后反馈的服装跟随已另行补齐，见 [穿戴物跟随报告](conform_follow_execution_cn.md)。这不表示后续阶段、IK 或 Golden 已完成。

## 行为调整

- 内容浏览器双击或“文件 → 添加 / 应用 DUF”：角色、模型和场景合并进入当前文档；姿势与形态预设应用到选中角色；材质预设按表面组更新选中对象。另设“打开场景（替换）”入口。未知预设明确报错，不继续当模型加载。
- 删除“姿势”菜单。Pose / Shape 保留其他对象、灯光和观察相机；选择骨骼部位时仍将预设作用于所属角色。参数显式零值会应用，未匹配或不可求值通道列入详情。
- 文档合并重映射网格、材质、贴图、实例和骨架图索引。实例 ID 独立；变形顶点独立；父对象变换可传递给挂接的子对象。
- “创建 → 面光源”可添加灯光；场景树可选择灯光并调整位置、相对旋转和功率。导入基础 DSON 点光、聚光、平行光及环境色。
- 视口左键选择最近的可选几何对象；角色首次选整体，再次点击同一角色选部位。头部使用“人物 → 整个 Head → 眼球 / 嘴唇等细分节点”三级选择；选中 Head 或其子节点后才进入面部细分，切换到其他部位或人物会退出该头部上下文。部位优先从多边形组匹配骨骼，缺少组时使用命中三角形的蒙皮权重。层次树与视口共用选择身份；头部面板显示头部所属参数及相应 DAZ 分组。
- 带 `conform_target` 的穿戴物不进入视口射线加速结构，射线会继续检测后方人物；这类对象仍能从场景层次选择。未绑定服装可被射线直接选择，仅有 parent 关系不会被误判为 Fit To。渲染和覆盖深度仍包含穿戴物，服装不会因此消失或变成透明。
- 悬停在 Beauty 显示之后执行独立几何覆盖：先绘制场景深度，再用淡黄色混合命中对象的可见表面。角色尚未选中时覆盖整体；已选中该角色后，仅覆盖射线命中的骨骼部位，范围与二级点击共用多边形组 / 蒙皮权重映射。已选中某个骨骼时，悬停范围仍随鼠标命中位置变化。悬停其他未选中角色或普通模型时覆盖整体，取消选择后恢复一级悬停。不会改材质或因鼠标、选择变化而重新求值变形。
- 视口为默认居中的独立 Dock，可浮动、停靠、关闭和重新显示。按子窗口尺寸与 DPI 更新原生窗口、渲染缓冲、相机宽高比和射线坐标。缩放复用已求值的网格。
- 关闭应用时用 QSettings 保存窗口几何与 Dock 状态，启动后恢复；提供“视图 → 恢复默认布局”。不在副屏工作区内的恢复窗口会重新定位。

## 加载等待的修复

几何加载阶段曾为场景中的每个 modifier 解析资源，只为生成“尚未求值”警告。现在该阶段将 Modifier 求值留给已有 Morph / Skeleton / Formula 阶段。

Morph 发现曾在每个参数内部复制并遍历整个 scene modifier 数组。现在一次建立覆盖索引，并为同资产的多个实例复用参数定义目录，重新应用各实例自己的保存数值。加载状态显示当前对象、参数资源数量、骨架与公式阶段；关闭时支持在发现阶段取消。

`test2.duf` 的几何阶段从本次首轮的 23.05 秒降至约 1 秒。此计时只用于定位本次等待问题，不是 Visible FPS 性能验收。

## 已取得的验证证据

- 9 项项目 CTest 通过，包含原 Morph / Skeleton / Formula 回归及新增 `scene_workflow`。上游 `cycles_version` 依赖未构建的 `out/cycles.exe`，不属于项目脚本指定的这 9 项检查；曾尝试全量 CTest 时，该项未运行。
- `artifacts/scene-workflow/test2-final.json`：真实 `H:\G1\Scenes\test2.duf` 载入 14 个几何对象和 14 套蒙皮（四个人物及服装 / 附件）。本轮记录：几何 1.00 秒，累计 Morph 61.76 秒、骨架 63.33 秒、公式 64.15 秒、初次求值 67.57 秒。
- 对首个人物应用 `H:\G1\Presets\Shaping\face_08.duf`：1,672 个 Morph 通道被应用。此阶段旧检查要求其他 13 个对象不变，遗漏了关联服装应跟随的语义；现已修正为允许所属穿戴物更新、无关人物和穿戴物保持不变，最新证据见穿戴物跟随报告。
- `artifacts/scene-workflow/ui-final/editor-check.json`：副屏两次射线点击、树选 Head、通过 Head 的 Jaw Open 别名实际变形、悬停、缩放以及布局写入磁盘 / 恢复均 PASS。最终 viewport 为 784×725，Head 仍保持选择；渲染报告无 GPU 回读。
- `artifacts/scene-workflow/ui-final/workflow-head.png` 与 `workflow-resized.png`：头部参数生效及淡黄色表面覆盖的副屏截图。
- `artifacts/scene-workflow/ui-docking/editor-check.json`：补充验证 viewport 浮动到副屏独立窗口后继续渲染，再恢复停靠；最终 Stage 7 PASS，选择保留且 Morph / Skinning 求值计数没有因缩放或停靠增加。浮动截图为同目录 `workflow-floating.png`。
- `artifacts/scene-workflow/scene-ui/editor-check.json`：真实四角色场景副屏渲染 PASS；`scene.png` 显示四个人物及服装，15 个渲染网格包含 14 个资产网格和预览地面。

新增工程检查还覆盖真实执行路径中的合并后独立 Morph / 变换、父子对象移动、Shape 显式归零、材质预设不污染共享原材质的另一实例，以及无几何的 DSON 灯光文件。

### 二级悬停高亮修复

根据用户反馈，修复选中一级角色后悬停仍覆盖整个人物的问题。选择状态通过带文档世代号的独立消息同步到渲染线程，悬停不提交变形快照。按点击共用的部位映射缓存三角形区域，深度遮挡仍使用完整场景，黄色覆盖仅绘制命中的区域；Morph / Pose 更新后覆盖几何同步更新。

`artifacts/hover-part-ctest.log` 的场景交互及相机检查通过，覆盖部位组 / 别名 / 权重回退、切换人物、普通模型与空白。`artifacts/hover-part/ui/editor-check.json` 的副屏流程 Stage 14 PASS：未选择时覆盖 32,736 个三角形，首次选择角色后相同位置仅覆盖命中部位的 1,110 个三角形；二次点击节点一致。选中 Head 后悬停其他部位、取消选择恢复整体、空白清除及树中重新选择均通过，选择 / 悬停未增加 Morph 或 Skinning 求值。前后截图为 `workflow-hover-whole.png`、`workflow-hover-part.png`；部署摘要为同级目录的 `../summary.json`。

### 头部三级选择与穿戴物过滤

后续用户指出头部应先整体选择，再进入眼球、嘴唇等子部位。现在按 Head 骨骼的后代关系聚合头部区域；匹配组名时处理 `Head` / `head` 大小写，头颈边界尊重资源中明确的多边形组，不让混合骨骼权重扩大 Head 范围。头部整体高亮包含眼球、口腔及面部子节点；第三层使用对应骨骼区域，悬停与点击仍共用解析结果。选择消息同时携带对象、骨骼与文档世代。

`artifacts/head-selection/final/test2-selection.json` 验证四个人物：每个人物聚合 10,668 个头部三角形，并保留眼球与嘴唇细分。8 件衣服的 880 条前景射线由原本命中服装变为命中各自所属人物，模拟未绑定状态可重新命中服装；原始用户资产没有修改。工程回归另覆盖普通 parent 不代表绑定、树选择 Head / 子节点时的上下文，以及切换人物后不沿用旧人物的面部层级。

副屏验证入口为 `--head-selection-test`，最终报告和截图位于 `artifacts/head-selection/final/ui-verified`，Stage 10 PASS：同一眼球位置连续三次点击，分别选人物、Head、眼球；随后检查嘴唇对应的上 / 下唇骨骼选择、树选绑定服装以及穿过衣服选择所属人物。完整 Head 覆盖 10,668 个三角形，单侧眼球 688 个，本次命中的上唇节点 114 个，Morph / Skinning 求值计数没有因选择而增加。`head-whole.png`、`head-eye.png`、`head-lip.png` 用于检查实际覆盖区域；部署哈希见 `artifacts/head-selection/final/summary.json`。早期只按骨骼静止中心定位的脚本受当前体型影响，已改为从几何区域定位并验证实际射线命中；旧测试日志保留，不作为最终验收。

## 明确保留的限制

- `face_08.duf` 不是全部通道完整还原：286 个不可应用的零值通道被忽略，另有 29 项详情，其中 6 项非零。Vo Xiao Mei Body 缺少相关商业资源 / 公式依赖；四个睫毛或眉毛控制器涉及当前角色之外的对象或无独立变形；SCLPropagatingHead 存在同名歧义。不通过任意选取同名参数来掩盖问题。
- 材质继续使用当前 PBR 子集；不宣称 Iray / MDL 与 DAZ 完全一致。基础灯光已导入，但 Iray 光度、专用灯光扩展和保存的观察相机尚未完整映射；相应导入诊断保留。无显式灯光的场景会建立可编辑的预览灯光。
- 实时服装 Morph / 骨骼跟随已在后续修复中接入；不含跨代 AutoFit、DAZ 平滑 / 碰撞修改器、IK、HD / SubD、动画或完整 DAZ Studio Golden。适用范围见穿戴物跟随报告。
- 悬停深度和射线使用基础几何遮挡，尚未逐纹素计算透明贴图的孔洞；透明头发卡片等可能命中其几何表面。

## 格式参考

DUF 是包含多个资产库及可选 scene 的容器，按类型与内容用途处理：[DAZ 官方 File Types](https://docs.daz3d.com/public/dson_spec/format_description/file_types/start)。基础灯光字段参考 [DAZ 官方 light](https://docs.daz3d.com/public/dson_spec/object_definitions/light/start) 和 [light_spot](https://docs.daz3d.com/public/dson_spec/object_definitions/light_spot/start)。本轮没有复制第三方实现源码。
