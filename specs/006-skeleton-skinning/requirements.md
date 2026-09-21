# 需求

- 保留骨骼 ID、name、name_aliases、父关系、center / end point、orientation、六种 rotation_order 和 inherits_scale；父节点必须先求值。
- 加载 DSF SkinBinding 与逐顶点关节权重，校验顶点数、索引、count、重复记录、缺失父节点、环和非有限值。记录源权重归一误差与未加权顶点。
- 按源资产的 General / DualQuat 求值。提供 Linear 数学路径和合成验证；TriAx、Blend、独立局部 / 缩放权重图等未支持模式明确拒绝。
- DUF 支持 `preset_pose`、JSON / gzip、`name://@selection` / `id://@selection`、URI 解码、时间 0 单键通道。支持骨骼平移、欧拉旋转；缩放仅在蒙皮方式允许时应用。双四元数骨骼缩放当前整次拒绝，角色对象缩放仍可用。
- 骨骼使用正确的 name / id 匹配，控制器限定所属节点；角色控制器不能与骨骼上的同名 Alias 混淆。支持已有可求值的直接 Morph 通道，未实现控制器的非零值必须列明。
- 姿势为部分覆盖：未指定通道保持当前值，不影响其他角色。先解析、校验，再一次性更新快照，非法输入不留下半个姿势。
- 稀疏 Morph → Skinning → Render IR 顶点 Delta；切换姿势不得累加到上次蒙皮输出。恢复姿势保留 Morph；重置对象同时恢复形态和姿势。
- 相机操作不求值 Morph / Skinning；顶点更新沿用既有网格、材质和纹理，法线与 BVH 经现有 Adapter 更新。
- 中文菜单提供应用 / 恢复姿势、应用详情及取景；Explorer 识别姿势 DUF，不能用它替换当前场景。树中展示角色骨骼层级，选骨骼仍以其所属角色作为当前参数 / 姿势目标。
- 用户资产只读，商业顶点与权重不入 Git；有窗口验证只在第二屏，不重跑性能回放。
