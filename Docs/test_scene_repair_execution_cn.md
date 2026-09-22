# test.duf 环境采样、挂接与参数编辑修复

日期：2026-09-22。阶段仍为 007 后修复，008 未启动。

> 后续更正：用户明确要求关闭降噪，以相机稳定后 15 秒的原始采样画面为目标。视口和离线输出均已关闭降噪。下文含降噪的历史截图、3.43 秒细化记录不能作为原始采样收敛证据；新的测量与结论见[无降噪复测](raw_convergence_execution_cn.md)。

测试输入为 `H:\G1\Scenes\test.duf`，用户描述中的 `H:\g1\scene\test.duf` 在本机不存在。产品加载、变形及截图均使用原始 DUF / DSF；没有用 DBZ 替换网格。已有 `test.dbz` 仅用于独立、只读的顶点对照。

## 修复内容

### 环境与色调

基线导入报告已能读取 `Environment Options` 和 `Tonemapper Options`。该场景实际使用 Dome Only、`DTHDR-RuinsB-500.hdr`、Environment Intensity 2、Environment Map 2、EV 13、ISO 100，穹顶 X / Y 朝向约为 67.40997° / 3.345177°，Draw Dome 关闭。因此不能把问题简单归为“没有解析到两个节点”。

实际缺陷位于 Cycles 适配层：之前只建立背景着色器，没有创建背景光对象，HDRI 缺少直接光重要性采样。现在同时创建 `BackgroundLight`，按环境模式启用 MIS。灰色背景与环境照明分别处理，关闭 Draw Dome 时 HDRI 仍能照亮角色。

编辑器从固定 64 样本改为最多 1024 样本，开启 OptiX 降噪和自适应采样，降噪从 16 样本开始，自适应最低 32 样本、阈值 0.01。离线 PNG 接入与视口一致的曝光、白点、亮部、暗部、饱和度和 Gamma；EXR 保持线性。基准程序可用 `--raw-sampling` 关闭降噪及自适应，性能回放继续保留原始采样口径。

曝光等纯显示参数不再重建环境或清空累计样本；环境参数仍触发正确的增量重新采样。渲染线程只在快照修订变化时复制编辑状态，只提交有变化的灯光，避免持续复制大量 Morph 数据和无关重置。截图等待实际呈现的样本数，避免把正在计算的样本误记为已经显示。

背景采样与降噪的用途可参阅 [Cycles 官方降噪与噪点优化说明](https://docs.blender.org/manual/id/5.1/render/cycles/optimizations/reducing_noise.html)。参数基线含义见 [DAZ Iray Environment 文档](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/interface/panes/render_settings/engine/nvidia_iray/environment/start)。

### 拖鞋与小附件

拖鞋保存的 Y Translation 为 1.230652 厘米；旧逻辑将这个穿戴前位移再叠加到 Fit To 目标上，使最终鞋子整体偏高约 11.258 毫米。现在 Fit To 直接继承目标的世界变换，普通子节点再继承替换后的父变换，并检测关系循环。

纽扣、发圈、玩偶使用 `studio/node/rigid_follow`，中间节点没有网格。现在读取 Follow Target、参考顶点及刚性旋转设置，在 Morph、蒙皮和碰撞后按参考表面拟合位置与朝向；嵌套挂接使用目标节点原点。附件保留自身编辑，并随角色整体移动及相应姿势更新。自身具有碰撞修改器的附件在最终挂接位置再检查碰撞，静止输入不重复更新。普通骨骼附件可穿过无网格祖先找到有效骨骼；场景树也显示在最近有效祖先下。

### 旋转、滑块与文本输入

对象编辑采用资产的 `center_point`、`orientation`、旋转顺序和父坐标系，围绕自身轴心旋转、缩放。保存旋转与编辑值按相同欧拉顺序组合，平移沿父坐标系应用。零编辑直接保持原矩阵，避免重复求值漂移。

数值滑块改为相对拖动标尺，越过标尺范围仍可继续增减；支持 Shift 精调、滚轮和方向键。右侧文本框允许直接提交数值。手动输入的 Morph 不再被资产 min / max、公式或运行时重复截断；未编辑的保存参数仍保留原 ERC 限幅语义，避免改变场景初始外观。手动应用、撤销待应用值和重置同时维护这一状态。

修复后台求值刷新覆盖正在输入的文字：文本框有未提交修改时不回写显示值，回车或失去焦点后提交。负缩放可用；只保留有限数值、不可逆的零缩放、正 Gamma 等数学有效性检查，以及布尔 / 枚举类型本身的取值规则。

## 验证与证据

- `tools/build.ps1`：13 项工程检查全部通过，含新增 Qt 参数控件测试。日志：`artifacts/test-scene-repair/build-final.log`。
- 最后调整输入框宽度与灯光数值范围后，重建编辑器并再次通过 `parameter_controls`；125 像素输入框避免无限范围的 Qt 尺寸提示挤占滑轨，380 像素宽面板已有布局检查。
- 控件测试实际输入超原范围值和负数，在输入途中触发多次刷新，验证失焦提交和滑块越界拖动。
- 旋转测试以非零轴心、朝向、父旋转 / 缩放及 ZXY 顺序建立 DUF，对照重新载入编辑后的资产矩阵。
- 跟随测试覆盖无网格祖先、刚性表面 Morph、旋转拟合、整体移动、恢复、重复求值及追加场景身份隔离。
- 真实场景、最终副屏截图及选项交互结果记录在 `artifacts/test-scene-repair`；最终记录见下表，早期失败和中间截图不作为最终验收证据。

| 检查 | 最终结果 / 文件 |
|---|---|
| 原始 test.duf 初始挂接、整体移动、转头 / 腹部姿势、恢复及静止 | PASS，`fidelity-final.json` |
| 原生几何导出及重复求值 | PASS，`native-final.json`；未读取 DBZ |
| 环境 / 色调面板、曝光不重置、环境重新采样、F / WASDQE | PASS，`options-final/editor-check.json`，stage 8，几何 / 实例更新均为 0 |
| 异步 Morph、手动应用、刷新、重试 | PASS，`lazy-final/editor-check.json`，stage 6 |
| 三角色整场与拖鞋近景 | PASS，`full-final/scene.png`、`feet-final/scene.png`，均为实际已呈现 176 样本 |
| 发饰背面近景 | PASS，`hair-back-final/scene.png`；已目视确认玩偶位于发圈处。`hair-final` 为被头发遮挡的正面观察角度，不用于判断附件可见性 |
| 离线 OptiX PNG / EXR 输出 | PASS，`offline-smoke`；仅用于输出路径检查，不作真实场景性能证明 |

此次 RTX 4070 Ti SUPER、774 × 894 视口的整场截图，在重新框选三角色后，从该相机状态首个已呈现的 1 样本帧到 176 样本帧约 **3.43 秒**，已开启降噪 / 自适应。包含读取资产、求值、纹理上传等流程的首场景首帧约 **73.21 秒**，两者不能混为一谈。这是一次实际运行记录，不是统一质量下与 Iray 的速度倍率；相机、材质实现及降噪方法尚未建立同口径对照。原始时间戳在 `full-final/events.csv`，摘要在 `evidence-summary.json`。

已核对 `out` 与 `build/bin/Release` 可执行文件一致。最终编辑器 SHA-256 为 `7e65de82be55bd0ed36db56b0f515a4d8085ee0d498b5fab91a9c4613fd22a31`，基准程序为 `ec9acc577b8b2cb26abf94f643897a42217f9c93e3322ac3d786ecd022890791`。本批修复与后续关闭降噪、原始采样优化按用户要求一并归档到本地 Git，提交记录见 `git log`。

现有 DAZ 几何对照：

| 对象 | 顶点 RMS 误差 | 最大误差 |
|---|---:|---:|
| RNKids Blooms Slippers | 0.779 毫米（修复前 11.318 毫米） | 3.006 毫米 |
| SU Fashion Long Jeans Button | 2.682 毫米 | 5.402 毫米 |
| FE Low Ponytail Base Hair Tie | 1.895 毫米 | 2.132 毫米 |
| FE Low Ponytail Base Hair Doll | 2.125 毫米 | 2.223 毫米 |

对照程序为 `tools/reference/compare_geometry.py`；原生导出和结果分别为 `native-final.json` / `reference-final.json`。参考 DBZ SHA-256：`a81a17cb5f45fce324194dceb8286b081b3cb9829fc078cf33fc2514d208cdb5`。DBZ 与 DUF 保存时间不同，该对照是定位和量化证据，不能代替用户提供的 Iray 图像验收。

## 保留边界

Iray 与 Cycles 的材质及色调曲线仍有近似差异，当前 EV 采用相对曝光基准；未宣称像素一致或完成绝对测光校准。刚性跟随本轮覆盖参考场景使用的完整 / 禁用旋转及不随表面缩放模式，其他轴向缩放模式明确报告尚未支持。

鞋子和小附件的局部毫米级差异仍存在；A 全身、服装碰撞、SubD / HD、专用灯光与材质的既有差异仍按相关阶段报告保留。开启降噪不是原始蒙特卡洛噪声完全收敛的证明；本轮也没有重新运行整套性能基准或证明 Camera-only ≥20 Visible FPS。
