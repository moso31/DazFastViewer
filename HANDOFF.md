# 交接记录

## 最新：参数滚轮选择与小数精度

2026-09-25：参数列表仅允许在左键选中的行上滚轮调参，其他行转交列表翻页；点击名称、Slider、数字框共用行选择。Slider 滚轮复用数字框的十进制步进，修复连续滚动及百分比换算的 float 尾数。专项离屏交互及工程规定的 18 项回归通过，已部署 `out/DazFastViewer.exe`，需重启使用。详见 [滚轮修复记录](Docs/parameter_wheel_execution_cn.md)。本轮未创建 Git 提交。

## 最新：DUF 场景收藏继承

2026-09-25：对象属性与 Morph 的收藏页现继承 DUF 节点的收藏列表，支持角色、骨骼变换、编码名称及 `/Value`，按场景／对象／骨骼隔离本机手动覆盖。同名资源优先匹配场景实际引用，避免重复显示头部别名。18 项 CTest 通过；原始 test7 的 13 个对象、55 项收藏逐项核对通过，源文件哈希不变。新版已部署 `out/DazFastViewer.exe`，运行中的旧程序需重启。详见 [收藏继承记录](Docs/scene_favorites_execution_cn.md)。本轮未创建 Git 提交。

本批 008 FK／IK、角度固定和恢复优化的代码、测试与文档已按用户后续要求以中文说明归档至本地 Git，提交记录见 `git log`。下文“本轮未创建 Git 提交”为实施完成时的历史状态。

## 最新：008 角度固定与松手恢复优化

2026-09-25：新增默认关闭、与位置固定独立的“固定角度（IK）”，只固定角度时仍可拖动位置。最短旋转约束包含 Twist，完整 ERC 在提交前单独校准，避免预览与最终朝向不一致。优化独立碰撞查询、同次精确缓存和面采样点预计算，保留原顺序修正、迭代和采样点；IK 松手直接恢复完整分辨率。

18 项回归及最终两组副屏 stage 27 全部通过，已部署 `out/DazFastViewer.exe`，SHA-256 为 `78addd7d44c53a024de827ab8a9c42b820cfe8d2833502032532a95a5f35f050`。高细分普通／角度固定恢复约 2.51／2.50 s，35,638 根发丝专项约 1.86／1.89 s；最终角度误差约 0.013／0.027°。正式证据是 `artifacts/008-refine/{subd-erc,hair-erc}`，早期 `hair-final` 失败已由 ERC 校准修复，不计为通过。完整口径和边界见 [追加执行记录](specs/008-fk-ik/refinement-report.md)。原始资产未修改，本轮未创建 Git 提交。

## 最新：008 FK／IK 与复杂场景拖动

已实现并部署 `out/DazFastViewer.exe`：部位属性按真实 DSF／DUF 通道编辑，保留锁定、限位和隐藏状态；仅鼠标按下前已单选本角色时允许左键 IK，多选或未选中拒绝。支持位置固定、Esc／失焦取消及版本隔离。拖动用最多 40,000 三角形的宿主灰模代理，不重复处理服装、发丝、碰撞、JCM、细分和 Cycles；松手完整恢复，不改变资产设置。

18 项工程检查、G8／G8.1 各 1,291 项通道写入及 21 项可见 FK 几何检查、真实四肢／指节／头部 IK 与双脚固定通过。副屏 test7 宿主 SubD 3 的拖动呈现中位数约 20.54 ms，松手恢复 6.93 s；原 test.duf 提取角色 A 及全部挂接物的 SubD 2／35,638 根发丝专项约 20.88 ms／4.05 s。后者不代表原三角色全场景测试通过。正式程序哈希为 `754f359fed95af10d0084e3e28d0c94a811b365b018e38664dfc7b5435e02cf7`，详见 [008 执行记录](specs/008-fk-ik/execution-report.md)。原始资产未修改，本轮未创建 Git 提交。此前“008 未开始”均为历史记录。

## 最新：GeoGraft 接缝正式修复

已将共同拓扑、统一细分、共享法线及共同 Cycles Object 接入生产路径，并部署到 `out/DazFastViewer.exe`。分别细分造成几何开口；仅共享几何但仍分成独立对象，还会使 Cycles 皮下散射在边界处中断。本次两层均修复，保留原 UV、贴图、Bump 和 SSS。角色与附件沿用独立编辑身份；组合使用宿主细分等级，附件显隐局部更新组合的面列表，Morph 复用模板，DAZ Instance 复用共同网格。

17 项工程测试、14 阶段 GeoGraft GPU 检查及 23 阶段原有 GPU 置换／场景同步检查通过。发布版 test7 的两个附件显隐、细分 0／1／2／3 反复切换与预算恢复、胸部大小／胸部缩放／体型参数修改恢复通过。曝光 13 的原材质胸部、臀部及纯白材质两区域截图均已核对：黑色开口闭合，原材质下的胸部散射细线消失。白模前后均为 4,096 samples、约 15 秒，不是延长采样带来的变化。

详见 [修复执行记录](Docs/graft_seam_fix_execution_cn.md)。最终 GUI 证据位于 `artifacts/graft-fix/` 的 `*-release`、`stress`、`release-color-*`、`release-white-*`；早期失败记录已在报告中说明替代关系。源码清单与发布二进制一致，编辑器 SHA-256 为 `5a1a9c230c7eea11f8ad1a45caa0e0c339f616c1670ba5bad8df8a13445a2035`。原始 test7 哈希未变，GUI 均在副屏。本轮按用户要求拆分提交：功能修复为 `a41646c`，诊断工具、测试及本记录另作后续提交；拆分未改变已验证的运行代码。下面的“尚未实现／out 保持原版本”属于此前诊断阶段，不是当前状态。没有新增 DAZ HD Morph 解码或产品脚本执行。

## 最新：test7 接缝原因复查

后续按用户反馈已改用纯白材质完成六张固定机位对照，见专项报告“后续白模复查”。A 图为分别细分，B 图为连接拓扑后统一细分的实验网格，不是不同采样时长。臀部两图截图时间均约 15 秒。仅确认 B 的开口闭合，胸部邻域曲面起伏与完整材质效果尚未验收；不得将其表述为正式修复已通过。诊断工具现输出 `white-*.duf`，白模证据在 `artifacts/graft-white/`。

按用户要求先将原有代码提交为 `83d0d0b`，再诊断 HD Nipples 与 Golden Palace 的接缝。已确认基础顶点映射正确、UV 导入逐角点与原始 DSF 相同；旧检查仅覆盖细分前的控制网格。实际分别细分后，Golden Palace／HD Nipples 边界最大采样间距约 3.51／3.16 毫米，统一到 2 级仍为 4.18／3.59 毫米。无贴图灰模同样开裂，按映射连接拓扑后统一细分的灰模在两个区域均无该裂缝。

详见 [专项报告](Docs/graft_subdivision_seam_analysis_cn.md)。新增 CPU 诊断工具和默认关闭的边界追踪，不改变生产渲染路径；实验合并网格仅用于对照，正式共享拓扑修复尚未实现。`out` 保持原版本；新诊断代码与文档暂留工作区。原始 test7 未改动，曝光 13 使用副本，GUI 均在副屏。后续必须以细分后几何及图像验收，不能以 `graft_seams()` 的基础误差接近 0 宣称渲染无缝。

## 最新：换装增量同步与单一细分等级

已按用户授权完成优化并部署 `out/DazFastViewer.exe`。界面只保留“渲染细分等级”，同时作用于当前预览与最终渲染；0 为基础网格。细分使用轻量快照，不重建形变运行时；换装／删除复用 Cycles 会话和已有材质、贴图、绑定与碰撞结果。更新期间保留最后画面。加入拓扑／顶点数检查、400 万细分面预算拒绝、错误后降低等级恢复，并修复同源不同 UV／细分／遮盖网格身份冲突。

最终同资源 `test7` 复测，按同为 731×836 的完整分辨率首帧：细分 2→1／1→2 从 11.19／11.33 秒降至 0.27／0.51 秒；添加两件 AH Nightwear 从 14.35 秒降至 4.36 秒，删除其中一件从 12.34 秒降至 1.06 秒。新版细分先给出低清预览，约 0.21／0.45 秒。全程一个会话、操作期间空画面 0；内存峰值仍与旧版同一量级。16 项工程检查、真实细分反复切换与机位保持、细分后 Morph／ERC、12 轮生命周期、23 阶段 GPU 置换／曲线同步、HD Nipples 接缝及真实曲线头发回归通过。完整口径与限制见[执行记录](Docs/scene_rebuild_optimization_execution_cn.md)，最终计时在 `artifacts/rebuild-optimization/test7-delivery/`，其他证据在同级目录。原始资源未修改，本轮未创建 Git 提交。

## 最新：换装与细分重建延迟诊断

根据用户最近会话，用 `test7.duf` 与 AH Nightwear 在副屏定量复测。视口细分 2→1→2 每步约 11.2～11.3 秒；只修改最终渲染细分、视口拓扑不变也等待约 11.3～11.4 秒。追加两件服装约 14.35 秒，删除其中一件约 12.34 秒；每步都新建一个完整 Cycles 会话，空画面约 3.9～4.1 秒。主要耗时是全场景碰撞重算（4.6～6.1 秒）和完整设备场景同步（4.0～4.2 秒），本次内存和显存余量充足。

仅新增专项诊断入口 `--rebuild-test`、分阶段计时和 `tools/profile_rebuild.py`，尚未实施优化。诊断程序为 `out/DazFastViewer-profile.exe`，正式 `out/DazFastViewer.exe` 未替换。六项真实操作及 16 项工程测试通过；原始证据位于 `artifacts/rebuild-profile/`。下一步应区分最终渲染／视口细分、复用基础形变及碰撞、扩展局部拓扑／对象增删同步，并独立保留最后画面。详细口径、数据和实施优先级见[分析报告](Docs/scene_rebuild_latency_analysis_cn.md)。本轮没有创建 Git 提交。

## 最新：自动穿戴与角色附件挂接

选中角色后，从内容浏览器打开兼容服装、头发或 GeoGraft 会自动解析选择引用并绑定 Fit To、父节点及相关跟随目标。新增“穿戴与附件”菜单，支持整套解除和更换角色；根据资产声明校验基型兼容性，并拒绝重复遮盖同一身体区域的 GeoGraft。修复已带姿势角色的骨骼附件绑定，以及头发／头皮反向层级在解除时形成循环的问题。

HD Nipples for G8F - 2.0 的指定 `.dse` 入口映射至产品自带基础 DUF，不执行脚本；纹理生成、配套碰撞优化、dForce、DAZ HD Morph 和跨代 AutoFit 未新增。完整使用方式和边界见[执行记录](Docs/attachment_import_execution_cn.md)。

最终 16 项工程测试、五组真实资源的挂接／解除／重绑检查通过；最终发布程序在副屏完成 MK 上衣和 HD Nipples 产品入口验证，后者 116 对基础网格接缝最大误差为 0 米。程序已部署到 `out`；证据位于 `artifacts/attachment-final-build.log`、`artifacts/attachment-*.log` 及两个 `attachment-ui-*` 目录。原始资产未修改，本轮未创建 Git 提交。

## 最新：材质贴图、置换与 Instance 一级选取

补齐 tiny_02 的 LIE 图像库引用、皮肤 SSS 模式／透光颜色贴图和凹凸尺度，接入 Iray／旧 DAZ 置换贴图及带符号的厘米高度范围。Cycles 使用真实置换与凹凸组合，材质和 Morph 编辑不会累加置换；尚未加入 DAZ 自适应细分。场景树仅展示 Instance 一级，点击、高亮、聚焦聚合全部零件，追加相同场景时保留独立身份。GeoGraft 可点击，Base_Circle 超远聚焦、Sun-sky Only UI 和年月日／时分秒控件已修复。

14 项工程测试、9 阶段 OptiX 置换顶点验证、坦克整组选取、四插件选取及 F／侧键聚焦通过。用户已验收坦克和桌椅 instancing，并确认手动调整机位／曝光后渲染效果正常；不将这些调整视作故障或同机位性能对照。最终发布包和源码清单位于 `out`，原始记录在 `artifacts/material-picking-followup/`，详见[本轮报告](Docs/material_picking_focus_followup_cn.md)。资产只读，008 未开始。本批场景兼容性修复、测试与文档按用户要求以中文说明归档至本地 Git，记录见 `git log`。

## 最新：接缝、Group 与新建后环境设置

已补齐 GeoGraft `vertex_pairs` 的最终形变接缝对齐、场景树的 Group 标签和父子关系，以及空 Document 被误当已有场景追加、丢失源环境选项的问题。14 项 CTest 通过；发布包在副屏按 `test7 → 新建 → 内容库打开 14` 检查通过。Golden Palace 44 对接缝误差为 0，四插件共 228 对最大世界坐标误差约 6.664×10⁻⁸ 米；实际 Visible 切换／恢复无几何更新。程序已部署到 `out`，证据在 `artifacts/scene-followup/`，详见[执行记录](Docs/scene_followup_seams_groups_environment_cn.md)。Group 当前恢复层级展示，未新增组级变换／批量显隐。008 未开始，资产只读，本轮未创建 Git 提交。

## 最新：test5／test6／test7／14 场景兼容修复

本轮修复加载、GPU 实例、GeoGraft 遮盖、父组可见性与材质通道遗漏，接入 Sun-sky Only 的 Cycles 近似。四个插件在实际 `test7` 中的关闭／恢复检查已通过，宿主隐藏面保持剔除；`14` 的暗图还追踪到保存为隐藏的走廊组被错误绘制，现已继承父组可见性。实例与阴影图像必须用最终形变后的编辑器核验，不能用仅加载基础笼的 inspect 代替。逐项实测、构建日志、待校准材质边界见[执行记录](Docs/scene_5_6_7_14_repair_cn.md)。用户资产未改动，测试均在副屏，008 未开始；本轮未创建 Git 提交。

最终 `out` 已部署，14 项工程测试及同一发布包的四场景检查均通过。`test5` 连续导航提交约 24.81 帧/秒（189×209 预览、完整视口 758×836），约 37／55 毫秒首响应／恢复；不是物理 Visible FPS 证明。最终原始证据目录为 `artifacts/scene-repair/{test5-release,test6-release,test7-release,14-release}`，源码／二进制清单及命令、计数汇总也在该目录。Sun-sky 亮度和复杂材质仍未通过 Iray Golden，热切换时的缓冲忙日志见执行记录。

## 最新：视口与角色移动性能优化

最终验证与部署已完成：14 项工程检查、32 项导航、异步／手动 Morph、真实 `test3.duf` 性能及 11 条选取／高亮／附件可见性检查通过，`out/DazFastViewer.exe` 已更新。曝光等仅显示编辑继续保留累计采样。旧头部自测对保存姿势和眼镜遮挡的假设也已修正；见下方执行记录。性能数据只代表 `test3.duf`，此前 `6.duf` 的剩余渲染等待未继续复测。

按用户更正，正式测试改用 `H:/G1/Scenes/test3.duf`，仅在副屏操作。已实现尺寸变化复用 Cycles 会话／场景、角色编辑交互预览及最新输入合并、共同移动时碰撞结果复用、局部网格选取树与实例高亮更新。原版实测主要瓶颈分别为约 6.6 秒场景同步和单次最高约 10.4 秒碰撞处理；优化后 6 像素缩放约 99／262 毫秒，角色平移／旋转约 96–101／186–206 毫秒（首张新图／停止后完整分辨率）。真实场景全程一个会话，角色恢复后局部几何和所有实例矩阵精确一致；14 项工程回归通过。详见[执行记录](Docs/interaction_optimization_execution_cn.md)。关闭降噪，最终采样不变；完整分辨率首帧不代表噪点收敛。008 未开始，本轮按用户授权以中文说明提交到本地 Git，记录见 `git log`。

## 最新：内容浏览器搜索、预览与近期使用

后续按用户反馈收紧图标间距、将顶部控件合并为一行，删除统计文字和视口 guide；图标改为每 8 像素一级，保留 64 / 224 像素端点并迁移旧设置。相机 / 角色移动的卡顿已完成源码与最近 `6.duf` 会话分析，发现 resize 重建会话引起约 46–52 秒同步，以及角色移动触发全局选取 / 高亮更新等问题；详见[交互等待分析](Docs/interaction_latency_analysis_cn.md)。渲染优化尚未实施。

007 后增强已实现：默认中等图标、Ctrl＋滚轮切换 21 档图标及列表并保存设置、PNG / TIP 悬停预览、按路径包含搜索及 10 项搜索历史、按 DUF 类型分类的近期使用（各类 20 个、所有 100 个，独立保留）。H:/G1 与 H:/G3 共 175,583 个 DUF，首次索引约 38.8 秒，缓存读取约 313 毫秒；查询响应 31–63 毫秒。14 项工程检查与副屏真实资源浏览通过。实现和刷新边界见[执行报告](Docs/content_browser_execution_cn.md)。008 未开始；本批代码、测试与文档按用户授权归档至本地 Git，提交记录见 `git log`。

剩余的视口等待和角色移动卡顿已整理为可直接用于下一轮提问的[新需求](Docs/interaction_optimization_request_cn.md)。这次提交不代表渲染性能优化已经完成。

## 最新：关闭降噪并复测真实收敛

用户明确要求不开降噪、相机稳定后 15 秒几乎无噪点。此前开启 OptiX 降噪偏离要求；当前两个产品入口均显式关闭降噪，诊断参数不允许开启。视口改为 Blue Noise First / 4096 上限，正常离线使用 Blue Noise Pure；自适应阈值仍为 0.01。真实场景 15 秒原始图较 Tabulated Sobol 改善，但尚未达到用户要求，不得用有降噪的 3.43 秒记录或自适应样本索引倍率宣称达标。HDRI MIS 已真实生效，静止时无几何更新，下一步需要分解光线与着色内核成本；更严阈值未改善本次固定时间结果。13 项工程检查、离线 PNG / EXR 通过并部署，详见[无降噪记录](Docs/raw_convergence_execution_cn.md)。008 未开始。本批修复按用户要求以中文说明归档到本地 Git，提交记录见 `git log`。

## 此前：test.duf 环境采样、附件与数值编辑

修复背景着色器缺少 Cycles BackgroundLight / MIS；离线 PNG 使用场景色调，曝光编辑保留累计采样。旧降噪 / 样本上限设置已由上述更正替代。Fit To 不再叠加穿戴前位移，补齐 Rigid Follow 参考表面、嵌套节点原点和附件碰撞顺序；对象绕资产自身轴心旋转。滑块采用无固定数值边界的相对拖动，文本输入不再被后台刷新覆盖，手动 Morph 保留不限幅状态。13 项工程检查和真实原生 DUF 的初始位置、整体移动、转头 / 腹部姿势、恢复与静止检查通过，已部署 `out`。拖鞋 RMS 从 11.318 降为 0.779 毫米；DBZ 只用于只读比较，产品不依赖它。范围、截图、性能口径和剩余近似见[本轮报告](Docs/test_scene_repair_execution_cn.md)。008 未开始，本批修复与后续原始采样优化一并按用户要求归档到本地 Git。

## 最新复查：A 眼镜层级及穿脸已修复

用户重新导出 `test3.dbz` 并在 Blender 确认位置正确。除 Head 场景树和镜腿定义覆盖外，最终根因是漏读场景通道下限：A 的 `Youth Morph=-0.2` 被资产下限 `0` 错误截断，影响头部网格和 Head 中心。现继承实例 min / max / clamped / step_size，并保持共享缓存隔离。A / B 眼镜对新 DBZ 的 RMS 均约 0.0005 毫米；A 头部约 0.093 毫米，B 人物和眼镜逐顶点不变。12 项回归及原始场景副屏检查通过，已目视确认 A 镜架下缘不再穿脸，并部署到 `out`。最终证据为 `artifacts/test3-glasses/channel-*`，详见[专项记录](Docs/test3_glasses_followup_cn.md)。完整全身 / 服装仍保留近似差异，008 未开始。本轮按用户要求，以中文提交说明将代码、测试与文档归档至本地 Git，提交记录见 `git log`。

## 最新：test3 渲染选项、参数面板及导航

修复深色 Top Coat 导致 B 皮肤发黑、骨骼中心变化导致眼镜偏移及拖鞋生成位移过度平滑；接入 HDRI Environment Options、ToneMapper、独立设置保存 / 载入，改为左右分类参数面板，增加 F 与 WASDQE。12 项工程测试及原始 test3 的副屏选项 / 导航、异步 Morph 界面回归通过，已部署到 `out`。拖鞋局部接触与 Iray 色调 / 材质仍有近似差异，不能宣称完整 Golden；A 的最终定位以本页最新眼镜专项复查为准。详见[本轮执行报告](Docs/test3_scene_repair_execution_cn.md)。008 未开始；本轮与眼镜专项修复一并按用户要求归档至本地 Git。

## 最新：test.duf 可见性、发丝、材质与 A 上衣贴合

本轮已接入节点 Visible 与两个 UI 开关、原生曲线头发、根层级 FitTo 变换跟随、薄壁眼部 / SSS / 分层高光材质及蒙皮后服装碰撞。A 有 GeoGraft，碰撞必须同时保证处于基础人体和可见附加表面的外侧，并反查粗服装面内部的小凸起；仅最近复合表面会选到内层而重新漏碰撞。详细实现、最终证据与近似限制见 [执行报告](Docs/scene_fidelity_execution_cn.md)。10 项工程回归通过；当前工程验证不等于 Iray Golden 或任意姿势完全无穿插。008 未开始。本轮按用户要求以中文提交说明归档到本地 Git，提交记录见 `git log`。

## 最新：用户视觉复测后的脸型 / 服装修复

后续实际视口复测又补齐了眼镜的骨骼挂接跟随，包含重复求值 / 恢复不漂移、子对象传播和追加场景身份隔离。眼镜对 DBZ 的 RMS 为 2.515 毫米，最大 4.654 毫米；人物几何结果未回退。最终 10 项工程回归通过，已部署到 `out`，详见下述报告。

用户确认以 Diffeomorphic DBZ File 导入的截图为准。原生路径修复负输入抵消值提前限幅、带朝向的非均匀缩放补偿、Figure 缩放基值、嵌套 Figure 姿势污染，以及基础 Morph 被 HD / 缺失附件公式整体禁用。服装生成位移改为拓扑连续场。两名角色头部对 DBZ RMS 约 0.0002 毫米，全身 0.070 / 0.125 毫米；服装仍有最高约 24 毫米的局部差异，未实现 DAZ 平滑 / 碰撞 / dForce，未通过完整服装 Golden。详见[本轮报告](Docs/scene_deformation_execution_cn.md)。`scene3-complete-ui` 是此前工程运行证据，已被用户指出视觉错误；新几何证据为 `artifacts/geometry-fixed3*`。008 未开始。本轮按用户要求以中文提交说明归档至本地 Git，提交记录见 `git log`。

## 最新追加：场景删除、资源回收与复合 DUF

本轮按用户反馈增加 Delete / 场景树右键删除模型及子对象、骨骼附件和 Fit To 穿戴物，并支持删除灯光、清空场景。删除与换材质会回收无引用网格 / 材质 / 贴图，重映射骨架、公式及快照；加载报告写盘后释放解析数据。渲染失败后仍能加载新文档。

`H:\G1\Scenes\3.duf` 首先因眼镜 Local 绑定、手机冗余 scale 图失败；检查相同轴的刚性权重后兼容。随后发现三个角色和一双鞋采用内嵌几何，需要沿 `source` 找回 Morph 和蒙皮。现保留内嵌几何并验证拓扑，支持源资产继承，真实骨架数从 27 补全至 31、每个人物各 6,808 项参数。另修复 G8.1 内部骨骼 ID / name 错位导致的 Fit To 歧义，优先唯一 name，再按 ID 消歧或回退。详细证据和边界见[执行报告](Docs/scene_lifecycle_execution_cn.md)。`scene3-ui` 与 `scene3-derived-ui` 为中间结果，最终使用 `scene3-complete-ui`。32 轮副屏生命周期测试见 `artifacts/scene-lifecycle/long-ui`。008 未开始，相关修复与后续形变修复一并以中文提交说明归档。

## 最新状态：007 后插入场景工作流优化，008 未开始

用户已授权在 007 与 008 之间插入多对象场景、DUF 分派、两级射线选取 / 部位 Morph、悬停覆盖、停靠视口 / 动态分辨率和布局持久化。本轮已实现并部署，详见 [执行报告](Docs/scene_workflow_execution_cn.md)。主线阶段不重编号，008 FK / IK 尚未启动。

最新修复：头部使用“人物 → 整个 Head → 眼球 / 嘴唇等细分”的三级选择。Head 聚合后代区域，头颈边界按明确的 DAZ 多边形组划分。带 `conform_target` 的穿戴物跳过视口射线，必须通过场景树选择；未绑定服装仍可点击，普通 parent 关系不算绑定。点击与悬停共用层级解析，选择消息携带对象、骨骼和文档世代。真实四角色 / 八件衣服的射线验证在 `artifacts/head-selection/final`，已部署到 `out`。

之前的二级悬停、清空选择和视口布局证据保留在 `artifacts/hover-part`；当前头部与服装行为以最新报告为准，主线 008 仍未开始。

真实 `H:\G1\Scenes\test2.duf` 的 14 对象 / 14 套蒙皮可加载与渲染。用户随后反馈服装未跟随 Morph，已保留 `conform_target` 并接入原生 Morph 优先、缺项表面位移 / 间隙转移和最终骨骼姿势继承；对应 10 个穿戴物、110,040 个绑定顶点。新的隔离检查允许所属穿戴物变化，无关人物及其穿戴物必须逐顶点不变。详见 [穿戴物跟随报告](Docs/conform_follow_execution_cn.md) 与 `artifacts/conform/final`。场景交互的既有证据在 `artifacts/scene-workflow`，不要重复无关扫描或性能回放。

`face_08.duf` 仍有 6 个非零通道未应用，涉及缺失依赖、跨对象控制和同名歧义；不可将部分应用称为完全还原。Iray 专用灯光 / 材质、保存相机、跨代 AutoFit、服装平滑 / 碰撞、HD 和 Golden 的边界见报告。本轮源码、测试和执行文档按用户授权归档到本地 Git，提交记录见 `git log`。

## 前一阶段：007 两批参数兼容性修复

原 007 已中文提交为 `da5d4f8`。用户随后要求修复一版，并追问 354 项之外的空间；已补充声明资产 URI 恢复，最终 G8 / G8.1 新增可编辑 476 / 614 项，本轮各追加 122 项，无原可编辑条目退化。本次按用户授权，将兼容性修复、验证证据和未生效参数清单以中文归档到本地 Git；提交号见 `git log`。已按用户明确授权关闭旧版窗口并覆盖部署 `out`，不要再次请求关闭许可。

最终证据只认 `artifacts/spec007-compatibility/final`，第一批 `validated` 为中间记录。8 项 CTest、两代各 40 姿势、新增条目取样 / 恢复、30 组数学对照、一次副屏六参数流程已通过，无需继续重复测试。参考脚本补齐多形态及默认控制器贡献，初次失败记录已保留；部分权重来自 Runtime，不是整图独立求值或 DAZ Studio Golden。G8 / G8.1 新增项中 71 / 63 个条件型条目在当前独立取样下无明显位移，未视为视觉验收通过。

实现：有符号 `-1` 计数、基于 SkinBinding 的节点 / 网格确认、同目录唯一内部 ID 恢复、声明 `asset_info.id` 与内部 ID 的唯一匹配、控制器下游诊断。空覆盖、代际屏蔽、歧义、HD 和资产锁定语义不变。当前资源中的 G8.1 `CTRL Vo Xiao Mei` 默认值 1 会影响初始外观，不可偷偷归零。

剩余优先方向：16 个骨骼属性别名结合 008；错误地址的可审查项目映射；睫毛 / 服装跨对象控制结合 FitTo / AutoFollow；真实缺失依赖按清单补包；HD / SubD 单独实施。无需用户全库解压或新增第三方源码。详见 [兼容性报告](specs/007-erc-jcm/compatibility-report.md)与[证据摘要](specs/007-erc-jcm/evidence/compatibility-summary.json)。以下是历史记录。

## 最新状态：Spec 007 / Formula、ERC、JCM 与 005 复测

用户要求先提交 006 再执行 007，并复测此前不能启用的 Morph。006 已中文提交为 `aa8fbc7`。007 当前实施范围已完成、部署并通过工程验证；用户已自行确认大量参数启用、姿势 DUF 工作正常，本次按新授权中文归档。后续修复 Ren Yao 的计数 / 目标绑定及 BGM 的旧文件名引用，不能把此前工程检查视为所有商业参数或 DAZ Studio Golden 已通过。

新增 `src/runtime/formula.*`、`deformation.*`、`src/daz/formulas.*`，统一 Formula → Morph / JCM → Skinning。支持 sum / mult、RPN / 样条、bool 默认值、Alias、循环与脏传播；基础骨架的节点公式也必须读入。两阶段 DQS 接入缩放及绑定中心变化。面板显示最终值，非法编辑保留上一有效状态。

G8 的 Arms Length、Chest Scale、Eyes Closed、HS Sanny Shy、Flex Quad Left 通过非零 / 归零检查与副屏滑块操作；默认可见可编辑从 720 增至 2,687。G8.1 从 745 增至 2,260，使用 FACS Eye Blink；旧 Eyes Closed / 眼睛方向的空覆盖继续保留。不可为了点亮旧项修改资产或绕过覆盖。别名、JCM 开关以及 G8 / G8.1 各 40 个姿势均通过。

最终七项 CTest、12 组独立数值对照、副屏流程通过。独立最大顶点差 `4.631754e-7 m`。源文件 / 程序哈希与结果见 [007 证据摘要](specs/007-erc-jcm/evidence/summary.json)，原始证据只认 `artifacts/spec007/validated`；早期目录是开发中的中间结果。当前无需继续扫描、demo 或性能回放。

剩余边界：DAZ Studio Golden、完整 TCB 资源对照、HD / SubD、TriAx / Blend、指向骨骼属性的面板 Alias、缺失 / 覆盖依赖、FitTo、FK / IK。下一规划为 008 FK / IK，FitTo 实测前再收集 G8 同代服装。详见 [007 报告](specs/007-erc-jcm/execution-report.md)与[预览命令](specs/007-erc-jcm/tests.md)。以下各阶段文字为历史记录，状态以本节为准。

## Spec 006 / 骨架、蒙皮与姿势 DUF 历史记录

用户授权启动 006，并提供 `H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female` 的姿势用于蒙皮检查。当前实施范围已完成并部署；用户已明确要求将 006 以中文归档到本地 Git。此归档不代替完整 DAZ Golden。005 已提交为 `ee2ba05`。

新增 `src/daz/skeleton.*`、`src/daz/pose.*`、`src/runtime/skeleton.*`：G8 / G8.1 各 171 节点、General DualQuat，按节点所属范围匹配姿势 name / id。`SkinningRuntime` 缓存 Morph 输出，应用 / 恢复姿势不累积变形。Qt 新增姿势菜单、Explorer 分派、骨骼树、取景与诊断，`tools/edit_sample.ps1` 新增 `-Pose`。

六项工程 CTest、G8 / G8.1 各 40 姿势、三种姿势的独立 mathutils 数值对照及副屏端到端均通过；最大参考顶点差约 `6.431e-7 m`，恢复误差 0。第一轮副屏发现同名控制器 / Alias 范围问题，修复后复验通过；不要继续无目的重复扫描、demo 或性能回放。当前部署 EXE SHA-256 为 `08feda362e646fb07239785dcf78d9945e52fc96147ee341f07718efed489ad8`。

**边界：** ERC / JCM、体型驱动骨骼中心、骨骼缩放的 DQS、多帧动画、TriAx / Blend、FitTo / IK 和 DAZ Studio Golden 尚未完成。目录中眼睛上下 / 左右两个非零控制器各出现于 33 个文件，明确报告未应用。下一阶段为 007 ERC / JCM；服装样例在 FitTo 验证前再收集，不影响本次姿势验证。详见 [006 执行报告](specs/006-skeleton-skinning/execution-report.md)及[预览命令](specs/006-skeleton-skinning/tests.md)。

## Spec 005 / Qt 编辑器历史记录

**阶段归档：** 用户本轮要求检查 005 完成后以中文提交。已对照总规范第 36 节确认当前阶段实施范围完成，并复核源码 / 部署 / 证据哈希一致。本次提交归档 005；核查过程没有再次运行 demo、全库扫描或性能回放。后续 Formula / Skinning / FitTo / IK 和整体 Golden 保留未完成，见 [阶段完成核查](specs/005-morph-runtime/completion-review.md)。

**最新修订：多内容库与控制器 / 别名发现。** 用户已提供四个根目录，写入本地（Git 忽略的）`DazFastViewer.project.json`；“项目 → 项目设置”可持久修改顺序。内容库配置用于场景依赖和 Morph 发现，脚本不再把样例限制为单库。`-Sample Genesis8` 新增 G8 基础角色入口，`Character` 仍是 G8.1。

参数发现补齐纯控制器、子节点 Alias、外部引用、原始分组，修复多段 gzip 的 57 个资源读取失败。G8 发现 6,012 个参数条目（720 个默认可见且可直接编辑），G8.1 为 6,799 / 745；均有 2 份 Neko count 不一致诊断。四个指定参数在 G8 已找到，但因公式 / 骨骼求值未实现仍只读；旧 Eyes Closed 控制器在 G8.1 被空覆盖屏蔽。新参数树仅为选中项创建编辑控件，支持分组与显示隐藏项。

五项轻量回归、新目录真实核对、新面板副屏编辑验证通过；最后布局 / 搜索计数小改只编译。最新证据见 [多库修复报告](specs/005-morph-runtime/content-libraries-report.md)。以下 225 / 23 / 16 和三项 CTest 是早期单库记录，不再代表当前全库清单。全库首次扫描可能约一分钟，后续应实现目录索引缓存；不要为了计时重复扫描或跑性能测试。

用户已授权进入 005，并补充长期 Explorer、菜单、加载角色、SceneHierarchy、Morph、FitTo 需求；最终 UI 采用 **Qt Widgets**，不再采用 Windows 原生控件作为应用框架。现有 WGL 视口嵌入 Qt，仍独立渲染与 GPU 互操作。Qt 套件为 `C:\Qt\6.10.3\msvc2022_64`；6.11.2 安装是 MinGW，不能与现有 MSVC Cycles 混链。

新增 `out/DazFastViewer.exe` 和 `tools/edit_sample.ps1 -Sample Character|Prop`。中文菜单、内容浏览、选择对象、直接 Morph、变换 / 重置、后台场景切换可用。UI 不直接改网格，带文档世代号的快照交给工作线程求值。当前场景树仅包含可渲染对象，视口固定 960×720。

Morph 发现位于 `src/daz/morphs.*`，纯 C++ 求值位于 `src/runtime/morph.*`；Render IR Delta 增加顶点和实例变换。G8.1 使用经过逐三角形编号验证的 G8 同性别兼容桥，空覆盖文件不会被忽略。当前角色发现 225 个稀疏 Morph，23 个可直接求值、16 个默认可见；其余 Formula / HD 项不可用。支持载入保存的直接 Morph 权重，目标 URI 不匹配不能仅凭顶点数套用。

三项 CTest、真实 Bodybuilder Size 0.5 / 1 / 0 数值（10,905 偏移、误差 0）、副屏 Morph / 变换 / 重置 / 相机及角色换道具均 PASS。相机不求值 Morph，几何始终 2 个；验证未重跑性能基准。证据见 [Spec 005 报告](specs/005-morph-runtime/execution-report.md)。

FitTo / 蒙皮 / ERC / JCM / IK、HD、保存 DUF、完整 Undo、动态视口尺寸和完整骨骼层次尚未实现。[长期规划](Docs/editor_and_genesis8_roadmap_cn.md)定义绑定关系、额外骨骼、体型跟随、菜单与验证顺序。服装样例在进入 006 / FitTo 时再向用户收集，005 不因此阻塞。Spec 004 材质 Golden 和严格 VisibleFPS 仍保留未完成。

Spec 004 中文提交基线为 `be0dbd1`。005 本次按用户新授权归档为中文本地提交，具体提交号见 `git log`；仓库未配置远端。

以下为已验收预览及 Spec 004 的历史记录，若与上述阶段进展不同，以最新状态为准。

更新：2026-09-21。当前已实现独立 Cycles 视口和两个指定 DUF 的静态预览；已获授权进入 Spec 004，完成首批后台材质参考及差异修复，不要将项目描述成尚未构建或不支持任何 DAZ 加载。

用户已于 2026-09-20 明确确认手动验收通过，并要求以中文提交、上传当前工程；验收记录已补充至 Spec 003。归档不需要重跑 demo 或性能测试。

## 用户约束

- 用户已要求实际执行所有开发与验证流程，已确认模型切换并授权继续本阶段；不要重复确认已有授权。
- 停止反复性能回放。只在新改动或具体故障需要时做针对性验证。
- **所有测试 demo 在第二块屏幕运行**。主屏 `DISPLAY1` 为 2560×1440，右侧副屏 `DISPLAY2` 为 1920×1080、起点 `(2560,0)`；默认窗口 1600×900，不抢主屏焦点，缺副屏明确失败。
- 角色：`C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8.1 Basic Female.duf`。
- 道具：`H:\G3\Environments\Architecture\ARK Modern Modular Cafe\Props\Tabletop\ARK MM Cafe - Food Plate with Fries.duf`，实际只有盘子和薯条，无汉堡。
- 用户资产只读引用，不复制进源码、不修改原文件。

## 已完成

`src/render_ir` 为纯 C++ 场景数据层；`src/daz` 解析 JSON / gzip DSON、内容库 URI、Mesh / UV / 常用静态变换与基础材质；`src/cycles` 执行后端映射及相机 / 材质 Delta。原 `src/bench/scene.*` 已由 IR Fixtures 替代并删除。

两个真实文件已无窗口 inspect，且在副屏显示。角色 16,556 顶点 / 32,736 三角形 / 17 材质 / 9 贴图；道具 1,794 / 3,536 / 2 / 5。渲染统计另外加一张地板（2 三角形）。兼容性诊断分别 20 / 3 条。

上述为 Spec 003 记录。Spec 004 接入 Bump 后，角色 / 道具贴图为 16 / 7，诊断为 36 / 5（新增凹凸距离近似提示，不能消除提示来伪装完整兼容）。非透射材质 IOR 修正为 1.5。

`tools/reference/run_reference.py` 以本机 Blender 5.2.2 + DAZ Importer 5.2.0 后台导出节点、检查独立基础顶点 / UV / 材质绑定，再共用三角网格渲染。两资产结构 PASS，已有对照在 `artifacts/material-reference/{prop,character}/comparison.html`，无需重跑查看。完整版图像 Golden 尚未通过，角色仍有显著散射 / 高光差异；详见 [Spec 004 报告](specs/004-material-reference/execution-report.md)。

实际角色导航通过：4 次相机设置（初始 + 旋转、平移、缩放），后端网格创建仍只有人物与地板 2 个；静态场景脏事件只在初始 epoch 2，最终输入与显示 epoch 5 一致。没有运行新的性能轨迹。

修复预览灯光过曝与 sRGB 编码重复。当前固定线性 Rec.709，颜色贴图必须使用 `u_colorspace_scene_linear_srgb`，数据贴图用 `u_colorspace_data`；别改回此版本执行 CPU 编码的 `u_colorspace_srgb`。最终人物截图在 `artifacts/dson-character-final`；旧灰色截图及 Shader / Albedo 诊断保留作失败证据。

启动入口 `tools/view_sample.ps1 -Sample Character|Prop`，默认副屏并生成新的输出目录。`--inspect` 无窗口 / 无 GPU；`--dump-shaders` 无窗口导出材质图与绑定；`--smoke --file` 可离线导出 Combined EXR / PNG 与 Diffuse Color PNG。`tools/build.ps1` 只自动运行轻量 CTest，不启动 demo 或性能回放。

新增 nlohmann/json 3.12.0，MIT，来源及哈希位于 `third_party/nlohmann/SOURCE.md`，许可复制到 `out/licenses/nlohmann-json`。没有复用 Diffeomorphic / DAZ Studio 实现代码。

## 事实边界与下一步

当前是静态基础网格 + 基础 PBR，未实现 SubD / HD、Morph、Skinning / Pose、ERC / JCM、完整 Iray Uber、完整 DAZ 变换 / 相机 / 可见性。自动取景和三盏灯是应用预览环境。报告 `fully_supported=false` 不能隐藏或改成完全兼容。

Spec 002 材质 Delta 已在 Spec 004 道具同一 Session 验证：2 次材质更新、22.233% 像素改变、网格 / 三角形计数与几何指针不变。GPU 字节级上传计数仍未完成；主机网格创建计数不是显存上传证明。

Spec 001 历史 CUDA 提交帧率 21.60–23.00、OptiX 27.42–28.70；严格 Visible FPS 因 PresentMon ETW 权限问题尚未验证。不要为了补结论重复跑分，也不要把提交 FPS 改成 Visible FPS。历史数据须按各自 build-manifest 区分，颜色空间等改动影响可比性。

`SceneIRTest / scene_ir_dson` 已通过，`CameraMailboxTest` 前一阶段已通过。本轮没有重跑 `tests/runtime_checks.py` 的延迟注入 / 最小化流程；相机验证仅复用其窗口定位辅助函数。

下一步继续 Spec 004：独立估计凹凸距离（不可写死样例数值），修正折射粗糙度 / 盘子高光，再对齐皮肤散射与复杂材质；尚不可跳过材质差异直接宣布 Golden 通过。当前无需新增第三方源码。已验收静态预览的基线提交为 `afc7737`；Spec 004 本批改动按用户要求以中文提交到本地 Git，具体版本见提交记录。

## 构建环境与保护事项

- CMake / MSVC：VS 2022 Community，C++20；构建 `build`、程序 `out`。
- Blender 原目录 `D:\Github\blender` 的 main / 5.3 alpha 与空 index（20638 staged deletions）是已有状态，**禁止 reset / clean**。
- 独立工作树 `D:\Github\blender-5.2.2`，commit `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`，分支 `dazfastviewer-blender-5.2.2`；跳过缺失的 LFS 笔刷二进制，不影响已使用源码。
- 独立 Cycles `.research/cycles-v5.2.0`，commit `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba`；Windows 库 `.research/windows-libs-metadata`，commit `60d6e96b917568278d400a4024c98da0fb777338`。
- `tools/prepare_cycles.py` 生成 `.deps/cycles`，移植 35 个 Blender 5.2.2 文件（34 个补丁与 image_maketx），其余保留独立来源；保留 BSD sky / 独立 allocator，不链接 Blender GPL guardedalloc。不要直接修改生成树，应用项目补丁并记录来源。
- CUDA 12.9.41；OptiX `D:\Github\optix-dev`，v9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d`；RTX 4070 Ti SUPER、驱动 591.86。
- 已按用户要求初始化主工程 Git 仓库，默认分支 `main`，提交说明使用中文。构建、运行产物、研究源码和用户模型均不加入版本管理；`out/` 仍是开发暂存，整体发布许可与依赖审计未完成。

详细阶段状态见 [specs/STATUS.md](specs/STATUS.md)，本轮实测见 [Spec 003 执行报告](specs/003-minimal-daz-loader/execution-report.md)。
