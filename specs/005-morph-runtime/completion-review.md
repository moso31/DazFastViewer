# Spec 005 阶段完成核查

日期：2026-09-21。结论：**005 当前约定的实施范围已完成，可以归档到本地 Git。** 此结论是阶段实施完成，不等同于整个产品达到 Version 0.1 Definition of Done，也不将工程验证代替用户手动验收。

## 范围与证据

总规范第 36 节将 Morph Runtime 定义为 Property、Morph Discovery、Sparse Delta、Active Set、UI Slider 和 Dirty Propagation。逐项核查结果如下：

| 要求 | 当前实现 | 验证依据 |
|---|---|---|
| Property | 每对象独立 Morph 权重、范围、初始值和相对变换；载入保存的直接 Morph 权重 | `morph_runtime` 的范围、重复值、保存值覆盖和对象隔离检查 |
| Morph Discovery | 多库优先级、递归目录、兼容目标校验、原始分组、控制器 / 子节点别名、依赖诊断与 G8.1 空覆盖 | `morph_catalog`，真实 G8 / G8.1 参数报告及四个指定参数核对 |
| Sparse Delta | 从不可变基础顶点累加稀疏差值；顶点 / 实例 Delta 接入 Cycles 动态几何 | 真实 Bodybuilder Size 权重 0.5 / 1 / 0 对照原始 DSF，最大坐标误差为 0；副屏几何更新证据 |
| Active Set | 只遍历非零 Morph；恢复初始值无累计漂移 | 核心叠加、负权重、恢复、偏移访问计数检查 |
| UI Slider | Qt 中文编辑器，先选择对象 / 参数，再使用滑块或数值框；搜索和分组浏览 | 新参数树副屏编辑检查通过，中文截图已检查 |
| Dirty Propagation | Morph、实例变换、相机分开处理；相机操作不触发 Morph 求值 | 核心回归及副屏相机检查；编辑期间只创建角色与地板两个网格 |
| 用户补充：长期 UI | 菜单、内容浏览器、对象树、属性面板与独立 Runtime；FitTo 契约已规划 | Qt 实际构建、场景后台切换检查及长期规划 |
| 用户补充：项目设置 | 四个根目录持久化、可增删排序、原子保存；重新扫描时保留仍兼容的参数值 | 配置持久化 / Unicode / 优先级 / 无效写入回归及实际多库扫描 |

证据入口：[首批执行报告](execution-report.md)、[多库修复报告](content-libraries-report.md)、[机器可读摘要](evidence/content-libraries.json)。历史单库统计不再用作全库支持数量；当前 G8 / G8.1 默认可见且可直接编辑的 Morph 分别为 720 / 745。

## 本次复核

- 已逐项核对需求、任务清单、核心求值、Cycles 增量更新、Qt 交互及项目配置实现。
- `out/build-manifest.json` 中 55 个源码 / 构建 / 工具 / 测试 / 第三方文件的哈希与工作区一致。
- 当前编辑器 SHA-256 为 `50d31ea155b98532c8fb2ccf9e016439e8ed1d7ffca7f7f37099eae810feb796`，与最新部署及证据摘要一致；相关目录报告、GUI 报告和截图哈希均匹配。
- 复用已经通过的五项轻量回归，以及 gzip 修复后的三项受影响回归、真实数值检查和副屏交互检查。最后的布局与搜索计数调整已编译，未重复渲染检查。
- 本次只补充完成状态和归档记录，没有修改运行代码，没有重跑 demo、全库扫描或性能基准。

## 保留的后续工作

1. 总规范第 37 节和后续 006 / 007：Skeleton、Skinning、Formula / ERC / JCM。Arms Length、Chest Scale、Eyes Closed、HS Sanny Shy 已发现，但完整求值仍依赖这些能力。
2. 后续 FitTo / IK、DBZ、HD、保存 DUF、完整 Undo、完整骨骼层次、动态视口尺寸与 Morph 磁盘索引缓存，按已有规划推进。
3. DAZ Studio 完整角色 Geometry Golden、Spec 004 材质 Golden、严格 VisibleFPS 和发布许可审计仍未完成，不能据本次阶段归档认定通过。
4. 两份 Neko 修正资源的差值 count 不一致及其他未解析依赖继续明确报告；阶段完成不代表所有第三方资产均兼容。

用户本轮已明确授权“检查 005 完成后中文提交”。本次提交包含 005 代码、测试、依赖记录、规范和文本证据；商业资产、运行二进制、截图及本机项目配置保持在已有忽略目录 / 文件中。
