# 设计

`src/daz/skeleton.*` 从解析后的对象几何来源读取骨架及 SkinBinding，父节点拓扑排序；场景内保存的骨骼覆盖值按 Figure 实例关系加载。`runtime::Skin` 与 JSON / Qt / Cycles 无关。Figure 已有世界变换留在 Render IR Instance，避免重复施加初始根变换。

骨骼在 DAZ 厘米、Y 向上坐标内求值，输入输出顶点统一转换为 IR 米、Z 向上。orientation 包围按 rotation_order 求出的局部旋转，父节点传递起点和旋转；最终变换乘绑定中心的逆平移。公式依据 [DSON Node](https://docs.daz3d.com/public/dson_spec/object_definitions/node/start)、[Skin Binding](https://docs.daz3d.com/public/dson_spec/object_definitions/skin_binding/start) 与 [Weighted Joint](https://docs.daz3d.com/public/dson_spec/object_definitions/weighted_joint/start)。

General / DualQuat 使用四元数半球对齐、逐顶点权重归一、双四元数归一与平移恢复。纯零姿势直接保留输入顶点，保证恢复不漂移。双四元数的刚性路径不假装支持骨骼非均匀缩放；该类预设整次拒绝，后续可增加经验证的两阶段缩放。算法依据 [Kavan 等人的双四元数研究](https://users.cs.utah.edu/~ladislav/dq/index.html)，独立实现，未复制插件代码。

`SkinningRuntime` 缓存蒙皮前顶点，消费 `MorphRuntime` 输出的网格 Delta 后再求值；同一网格只向 Adapter 发一次最终顶点更新。姿势变化从缓存重算，相同姿势不置脏；相机分支完全绕开变形求值。每个角色实例各自持有姿势和输入几何。

`src/daz/pose.*` 将时间 0 单帧 DUF 解析为属性覆盖，按照 [DSON 资源寻址](https://docs.daz3d.com/public/dson_spec/format_description/asset_addressing/start) 区分 name 与 id；骨骼别名和控制器名称不等同于 DSF 内部 ID。相同名称的控制器限定所属节点，不能把 pelvis Alias 和 Figure 控制器当成同一个对象。多帧文件拒绝，避免把 [Channel Animation](https://docs.daz3d.com/public/dson_spec/object_definitions/channel_animation/start) 的插值语义误当成普通线性关键帧。

未实现的零值控制器计入 `ignored_zero_controls`；非零控制器与未知骨骼 / 属性进入 `unapplied`，界面和 `pose-report.json` 均可查看。当前 Bed Hogs II 的非零未实现参数为眼睛上下 / 左右控制器。007 实现 ERC 后应接入同一属性通道，不另做样例特判。

Qt Document 增加只读骨架目录，Snapshot 增加姿势；渲染线程独占求值与 Cycles 更新。应用姿势后按变形后的角色包围盒取景。背景场景加载保留 generation 隔离；项目设置重载时按骨骼 ID 恢复兼容姿势。

验证分为解析 / 数学合成、真实 40 姿势批处理、独立 Blender mathutils 数值对照、一次副屏实际应用与恢复。mathutils 对照不启用插件、不渲染，也不等价于 DAZ Studio Golden：JCM、SubD、ERC、材质等完整结果仍待验证。
