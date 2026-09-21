# 设计

`daz::discover_morphs` 在原有单次资源读取中将公式编译为紧凑的中间记录。重复 URI 仅保存一次，常见的“参数 × 系数”使用专用记录；不将 54 万条公式保留成庞大 JSON 树。`daz::enable_formulas` 将记录绑定到当前角色的 Morph 和骨骼属性，建立每个实例独立的 FormulaGraph。

`load_skeletons` 同时保留基础 DSF 的节点公式：G8 为 150 条，G8.1 为 118 条。它们参与相同依赖图，承担 Bend → Twist 缩放传递、脚趾联动等功能；不能只读取 `modifier_library` 中的公式。节点引用限制在所属骨架，未支持的外部节点引用单独报告。

公式按 DSON 的 RPN 求值；支持官方 `multiply` 写法和真实资源使用的 `mult` 阶段。当前手动输入参与求和阶段，再乘各乘法阶段，Morph 结果按资源通道范围 clamp。骨骼保留 006 直接应用姿势值的策略，本轮只检查有限值及正缩放；完整骨骼 Limits / Constraints 留给 008。依据：[Formula](https://docs.daz3d.com/public/dson_spec/object_definitions/formula/start)、[Operation](https://docs.daz3d.com/public/dson_spec/object_definitions/operation/start)、[Channel](https://docs.daz3d.com/public/dson_spec/object_definitions/channel/start)。具体与 DAZ Studio 最终值的完整对照仍保留为 Golden 项。

FormulaRuntime 缓存每条公式的结果，用拓扑优先队列处理脏通道。上游有效值改变才标记下游公式；无关通道、相同值与相机分支不求值。循环通过强连通分量识别，相关通道局部禁用并报告。原始编辑值与最终结果分开，避免 JCM 结果回灌造成累积。

`DeformationRuntime` 统一连接 FormulaRuntime、MorphRuntime 和 SkinningRuntime。求值先产出候选 Morph 权重和骨骼状态，通过有限值 / 缩放检查后才应用几何；无效编辑恢复图输入，视口保持上一有效状态。骨骼中心和 orientation 使用相对于原始绑定数据的偏移，零姿势下不重复施加已存在于稀疏 Morph 中的几何移动。

两阶段 DQS 先在调整后的绑定姿势中混合伸缩，再对伸缩后的关节位置执行刚性双四元数变形。单关节结果与完整仿射变换一致，纯刚性路径保留 006 的语义。依据 [Kavan 等人的两阶段蒙皮方法](https://users.cs.utah.edu/~ladislav/kavan08geometric/kavan08geometric.html)；独立实现，不复制 Blender / 插件源码。

Alias 在目录中保留名称、分组和所属节点，但编辑时写入规范目标；最终 ERC 值仅在面板展示，不写回快照。姿势 DUF 与 UI 使用相同的参数赋值规则。HD、拓扑不匹配、缺失 / 歧义 URI 与未支持属性保留诊断。
