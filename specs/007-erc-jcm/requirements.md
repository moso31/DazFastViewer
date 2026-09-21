# 需求

- 读取 RPN 公式，支持常量 / 通道引用、加减乘除、倒数、负值、线性 / 常量 / TCB 样条；校验栈、控制点和有限值。
- 支持 `sum` 与 `mult` / `multiply` 两阶段组合，保留手动编辑输入与求值结果的区别，Morph 结果应用通道范围限制；骨骼 Limits / Constraints 的交互语义留给 008。
- 同一角色内按资源 URI、所属节点与 stable ID 解析；内容库优先级、G8.1 覆盖、丢失资源和歧义不能被模糊名称匹配绕过。
- 按依赖拓扑求值，检测循环并隔离；仅重新计算脏输入影响的公式，相同输入和相机操作不触发求值。
- 保留 bool 的 true / false 默认值；Base Joint Correctives、Base Flexions 的资源默认开启值必须参与 JCM，不能把 true 读取成 0。
- 角色控制器与子节点 Alias 共享编辑值；面板显示当前可求值状态及最终 ERC 值。禁用项保留具体原因。
- 支持骨骼平移、旋转、缩放、general scale、center_point、end_point、orientation 公式输出；Morph → JCM / 稀疏差值 → Skinning → 顶点 Delta。
- 两阶段 DQS 支持正值非均匀骨骼缩放；非法非有限值或零 / 负缩放编辑不得破坏上一有效网格。
- 005 直接 Morph / 属性隔离 / 恢复及 006 姿势入口不得退化。自动 JCM 与手动输入不可回写叠加，恢复不累积漂移。
- 真实资产只读，商业公式、差值和网格不得复制进 Git；demo 仅第二屏，验证针对新功能与具体失败进行。
