# test3 场景、渲染选项与参数面板修复

更新：2026-09-22。属于 007 后插入修复；008 FK / IK 未开始。

**后续眼镜专项修复：** 补齐 Head 树层级、镜腿定义和场景 Morph 限幅覆盖。使用用户重新导出并经 Blender 验证的 DBZ，A / B 眼镜逐顶点 RMS 均约 0.0005 毫米；以[专项复查](test3_glasses_followup_cn.md)为准。

本次使用实际存在的 `H:\g1\Scenes\test3.duf`。原始 DUF、DSF、DBZ 和贴图保持原样。用户提供的六张图用于外观和界面比较；工程运行通过不等同于 Iray 图像一致性验收。

## 已实现

| 用户反馈 | 原因与处理 |
|---|---|
| B 身体皮肤发黑 | 原先将 Iray Top Coat 反射色当作 Principled Coat Tint 的吸收色，深色涂层会错误吸收底层皮肤。改为独立反射层，读取层模式、IOR 和自定义 Fresnel 参数。修复后 B 面部能正常显示皮肤。 |
| 眼镜相对脸部偏移 | 骨骼挂接遗漏 ERC / Morph 改变的骨骼中心位移。补齐当前与初始中心的差值，保留 inverse bind；重复求值和恢复不会累积位移。 |
| 拖鞋严重穿模 | 自动生成的衣物位移统一做了 24 次平滑，压平了贴近脚面的鞋面。现按静止状态距人体表面的距离调整权重：3 厘米内保留形变，远处逐渐增加平滑。人体顶点不变，近景大面积鞋面穿插明显减少；仍不声称完全无穿插。 |
| 环境光缺失 | 导入无网格的 Environment Options，继承基础资产及场景通道覆盖，解析 HDRI URI 与路径；支持 DAZ 安装内的 `/resources/` 纹理。Cycles 使用全局经纬环境纹理、亮度、颜色、方向及旋转。 |
| ToneMapper 未继承 | 读取并应用启用、EV、快门、光圈、ISO、cm² Factor、白点、亮部、黑部、饱和度、Gamma 和暗角；EV 与快门 / 光圈 / ISO 联动。 |
| 参数面板 | 左侧分类树，右侧参数行；保留 DAZ 分组、搜索、隐藏项、收藏、当前使用、状态和来源提示。每行提供滑块与数值框，布尔 / 枚举使用下拉框。仅创建可见行控件，兼容数千个 Morph。对象总体缩放和 XYZ 变换也纳入分类。 |
| 导航 | F 聚焦当前层级对象；选择骨骼部位时按所属表面与后代区域计算范围。W / S 前后、A / D 左右、Q / E 下上，Shift 加速。需先点击视口获得移动键焦点；场景树中 F 也可使用。 |

Environment Mode 支持 Dome and Scene、Dome Only、Scene Only。Dome Only 会关闭场景灯光；Draw Dome 关闭时隐藏相机背景中的穹顶，保留环境照明与反射。Sun-Sky Only 的原始数据保留，选项显示“待支持”，加载此模式会生成诊断；太阳天空模型尚未实现。

场景树可选择 Environment Options / Tonemapper Options，面板默认打开对应分类。“文件 → 保存环境与色调设置…”写入 `.dfv-render.json`；“载入环境与色调设置…”恢复参数、纹理 URI 和解析路径。这是工程渲染设置文件，不是 DAZ DUF 场景回写。未实现的选项保留值并禁用编辑。

本场景读到 Dome Only，`DTHDR-RuinsB-500.hdr`、Environment Intensity = 2、Environment Map = 2、Dome Orientation Y = −128.1961°、Draw Dome = Off；ToneMapper 为 EV 14、快门倒数 256、F/8、ISO 100。模型加载后不会再用默认工作室灯光覆盖此环境。

## 验证和证据

证据目录：`artifacts/test3-repair/`。构建日志 `build-final.log` 中 12 项 CTest 全部通过；CMake 的旧 FindCUDA 策略提示和 Qt 部署的 DX 编译器提示不影响本次 OptiX / OpenGL 验证。部署程序与构建程序 SHA-256 一致：

`00054e69c0340d44c812bb959f971458c8af169a32cd213252d08ec0ccad15c4`

- 新增 `render_options`：节点继承、场景覆盖、HDRI 路径、Dome Only、曝光联动、设置保存 / 重读、文档快照与资源回收、相机位移和总体缩放。
- `conform_runtime` 增加骨骼中心变化的刚性附件跟随与恢复、近体表形变保留；已有服装跟随 / 平滑、碰撞与恢复检查仍通过。
- `final-options-ui/editor-check.json`：原始 test3 的副屏端到端 PASS，23 个网格，曝光、环境旋转、F、按住 W 后几何更新数为 0。对应三张 `*-panel.png` 展示分类及参数行。
- `final-lazy-ui/editor-check.json`：新版面板的异步 Morph、连续修改、手动应用与目录刷新 PASS。
- `updated-b-front/scene.png`：B 正面皮肤修复检查。`final-a-face/scene.png` 与 `final-feet/scene.png` 为原始完整场景的最终近景。
- `updated-b-diff.json`：B 眼镜与现有 DBZ 的 RMS 从 2.724874 毫米降为 0.000724 毫米，最大误差 0.001104 毫米。
- `feet-before-after-report.json`：独立 Blender BVH 对鞋顶点与面内部共 43,196 个点采样，估计内穿超过 2 毫米的点从 4,591 减至 1,190，超过 0.5 毫米从 6,410 减至 1,817，人体顶点变化为 0。最近面有符号距离不是闭合体积判定；仍有局部异常，不以此宣称零穿插。

`options-ui` 是早期失败记录：测试程序直接切换属性选择、未同步树的当前项，重新选择同一树项没有触发信号，停在第 5 步。现统一树与属性选择，最终以 `final-options-ui` 为准。`feet-contact*` 为被否决的碰撞实验；它会顶坏鞋底，不是最终实现。`fit-view` 为冻结诊断网格的中间截图，最终使用原始场景截图。

## 实际边界

首次使用的 `test3.dbz` 缺少拖鞋，A 几何也存在明显差异，当时尚未定位原因。用户随后重新导出并在 Blender 验证，新参考确认 A 的头部 / 眼镜偏移来自本工程漏读 Morph 的场景下限覆盖；先前关于文件版本的推测不能作为结论。该问题的修复与最新对照见[眼镜专项记录](test3_glasses_followup_cn.md)。完整全身 / 服装几何仍保留近似差异。

ToneMapper 使用实时近似曲线，相对 HDRI 曝光基准为 EV 13。绝对测光、Iray 亮部 / 黑部曲线、暗角光学模型和完整皮肤光谱效果尚未做到一一对应；导入数值一致不代表像素一致。完整 SubD / HD、dForce 和 DAZ 平滑也尚未实现。骨骼位置参数目前用于查看，FK 编辑不在本次 007 后修复范围内。

参数语义参考 DAZ 官方 [Environment](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/interface/panes/render_settings/engine/nvidia_iray/environment/start)、[Tone Mapping](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/interface/panes/render_settings/engine/nvidia_iray/tone_mapping/start) 及 [NVIDIA Iray 手册](https://raytracing-docs.nvidia.com/iray/manual/index.html)。
