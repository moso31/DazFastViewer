# 内容浏览器与 Xanthe Hair 后续修复

2026-09-27。

## 目录树文件名

左侧目录树此前关闭了末列拉伸，且只在“定位到内容库”时手动计算列宽。目录异步加载、展开或拉宽面板后，仍可能沿用旧宽度并显示省略号。

文件名列现在使用 `ResizeToContents`，随当前内容重新计算宽度；长路径超出面板时仍允许横向滚动。回归覆盖异步展开长目录、长文件名，以及缩窄后重新拉宽。

## DJL 材质应用

这次错误发生在 DJL 已经解析到 DUF 之后。Xanthe 的部分 G8 链接引用 G9 材质预设；预设指定 `default-0xa8bd82a`，实际 UV 文件只包含 `default-0xa93958a`。检查了本地 291 个链接，其中 131 个链接的目标预设包含前一种引用。上一轮真实资源检查仅覆盖 Style/Morph，遗漏了材质 UV 应用。

解析仍优先准确匹配 ID。仅当 UV 引用末尾是 `-0x` 十六进制生成后缀、同一文件中存在唯一同名 UV 时，允许兼容新的后缀。不进行跨文件猜测；有歧义、不同名称或顶点数不匹配仍然拒绝。其他资产类型继续严格匹配 ID。

真实检查包括 Xanthe 普通颜色、深色发根、挑染和 Fantasy 链接，均应用到 6 个表面。新增回归覆盖唯一匹配、准确 ID 优先、歧义拒绝、名称不符及顶点数不符。具体引用证据见 `artifacts/content-followup/uv-reference.json`，应用结果见 `artifacts/content-followup-material-*.log`。

正式程序在副屏完成基础 G8F 摆姿势后穿戴 Xanthe、解除挂接、重新绑定，再经内容入口应用 `Xanthe Hair Color 01 Tone 03.duf.djl` 并等待光追画面。检查结果为 `PASS`、`material_link_applied: true`，见 `artifacts/content-followup/gui-material/editor-check.json` 和 `wear-rebound.png`。

## 头发加载耗时

测量了基础 Genesis 8 Female 选中后添加 Xanthe Hair 的完整路径。导入校验和渲染准备此前各执行一次相同静止网格的最近表面绑定。

加入进程内几何绑定缓存，最多保留 32 项、约 128 MiB。缓存键包含宿主和附件顶点、拓扑以及相对变换，只复用表面绑定和邻接表；骨骼、Morph、Fit To 依赖仍重新校验。任何几何或相对变换变化均重新绑定。没有降低细分、纹理或渲染质量。

| 测量项 | 修改前 | 修改后 |
| --- | ---: | ---: |
| CPU 渲染准备中的形变运行时构造 | 466 ms | 17 ms |
| 完整界面中的同一步骤 | 517 ms | 20 ms |
| 点击加载到首帧 | 4.98 s | 5.08 s |

完整首帧还包含文件解析、纹理与 GPU 场景准备，本轮单次测量没有显示明显的整体提速；这里只能确认重复绑定计算已消除，不能把局部耗时下降等同于端到端加速。该测试为基础角色加头发，不能代表大型保存场景。优化前后 CPU 顶点及 Morph 权重哈希一致，完整界面三角形数均为 738,522，均未新增渲染会话。

性能证据：`artifacts/content-followup/xanthe-before/profile.json`、`xanthe-after/profile.json`、`timing-comparison.json`，以及 `gui-before` / `gui-after` 子目录。

## 构建与部署

26 项工程测试通过，`out/DazFastViewer.exe` 已更新。构建、部署清单及实际 EXE 哈希一致，226 个源码文件与清单一致。构建记录：`artifacts/content-followup-full-build.log`；部署核验：`artifacts/content-followup/validation.json`。构建包装脚本仍将 CMake 开发者警告报告为 PowerShell `NativeCommandError`，日志中的编译、26 项测试与部署均已完成。

用户内容库保持只读，本轮未创建 Git 提交。
