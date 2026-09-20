# Spec 004 首批执行报告

日期：2026-09-21。**参考链路和首批修复已完成；Spec 004 整体仍在进行中，未通过完整材质 Golden 验收。**

## 本轮交付

- 原生 `--export-scene` 导出可审查 Render IR 场景，包含三角形、逐角 UV、材质、贴图、实例、相机和灯光，不需要 GPU 设备。离线渲染导出同一契约。
- Blender 5.2.2 LTS + DAZ Importer 5.2.0 独立后台参考工具；以 `UNIQUE` 导入基础网格，不需要 DBZ，不应用细分或变形，不保存用户偏好。
- 参考材质节点、输入 / 输出常量、连接、节点组、Principled 参数、贴图及色彩空间导出；新增材质与线性图像差异报告、本地 HTML 对照页。
- 独立 Runtime 新增 Bump 高度贴图和 Normal Map 叠加。显式高度范围按厘米转换为米；没有范围时暂用 1 毫米并报告 `bump_distance_approximation`。
- 非透射材质不再把 DAZ `Refraction Index` 当作表面高光 IOR；保持 1.5，与本轮参考输出一致。
- 材质 Delta 在同一 Session 内进行图像及几何对象检查。

参考工具只调用本机插件公开 API，未复制插件实现到工程或运行库。商业模型、贴图、渲染和材质图留在被 Git 忽略的 `artifacts`，不提交到仓库。

## 验证结果

两资产均为 640×480、64 samples、OptiX、seed 1337；关闭降噪、自适应采样，最大反弹 8、漫反射 / 高光 4、透射 / 透明 8。输出线性 Rec.709 EXR 与 Standard/sRGB PNG。参考先独立导入并检查基础顶点、UV 和三角形材质绑定，再共用 Runtime 三角网格进行材质对照。

| 项目 | 道具 | 角色 |
|---|---:|---:|
| 基础顶点数 | 1,794 | 16,556 |
| 最大顶点误差（米） | 1.539e-8 | 1.335e-7 |
| 最大逐角 UV 误差 | 0 | 0 |
| 三角形 / 材质组绑定 | PASS | PASS |
| Runtime 贴图资源数 | 7 | 16 |
| 兼容性诊断数 | 5 | 36 |
| 整图线性 RGB MAE | 0.003681 | 0.006863 |
| 整图线性 RGB RMSE | 0.008074 | 0.027083 |
| 资产投影包围矩形 MAE | 0.009072 | 0.022165 |

整图误差包含随机采样噪声，也受大面积背景稀释；包围矩形仍包含背景和空隙，不是对象遮罩。以上指标仅作差异记录，未设 Golden 合格阈值。道具外观接近参考，角色参考图明显更暗且散射噪声更强，不能据整图 MAE 较小宣称材质一致。

`camera_mailbox`、`scene_ir_dson` 均 PASS（0.12 秒合计）。新增针对凹凸强度覆盖、单位转换、数据色彩空间、缺省距离诊断、纹理索引和非有限参数的检查。

道具材质 Delta：同一 Session 仅加载一次，2 个材质更新，22.233% 像素变化超过 RGB 差值和 0.03；线性 RGB MAE 0.030864。网格仍为 2 个、纹理资源仍为 7 个、几何对象指针和三角形计数不变，**PASS**。GPU 上传字节仍是 `NOT_MEASURED`，不能据此声称没有任何上传。

本轮正式渲染为：两资产各一张 Runtime 与一张 Blender 参考，道具另有一张 Delta 后图像。其余导出 / API 调试没有渲染。没有启动桌面窗口，没有性能回放，没有重跑严格 VisibleFPS。

## 尚未对齐的材质语义

1. **凹凸距离**：Runtime 缺省 0.001 米；参考盘子约 0.00055828、薯条 0.00031516、角色 Body 0.00057714 米。后续须独立实现合理的几何 / UV / 纹理尺度估计，不能把这些样例常数写死到加载器。
2. **角色皮肤**：参考使用散射、高光权重 / 颜色和额外节点组，多数皮肤材质有两类参考贴图尚未绑定至 Runtime；包括 SSS / specular。不能只靠调色补偿亮度差异。
3. **眼睛**：EyeMoisture / Cornea 的 Runtime 粗糙度为 0.263736，参考为 0，下一步须区分折射粗糙度与表面高光粗糙度。
4. **道具高光**：盘子参考 Specular IOR Level 为 1、Tint 为 0.7；Runtime 尚为默认值。仅有完整贴图路径并不等于完整语义一致。
5. Emission、完整 Top Coat / Dual Lobe、PBR Skin、透射 / 体积与所有有贴图的参数尚未全面实现或验收。SubD / Morph / 骨骼及通用相机等仍属后续阶段。

## 查看与复现

直接打开现有结果，无需重跑：

- [道具对照](../../artifacts/material-reference/prop/comparison.html)
- [角色对照](../../artifacts/material-reference/character/comparison.html)
- [材质 Delta 记录](../../artifacts/material-reference/prop/runtime/material-delta-check.json)

需要新对照时运行 `python tools/reference/run_reference.py`；需要单独导出结构使用 `--export-only`。`--sample prop --verify-delta` 可重现材质增量检查。脚本遇到导入、纹理、几何、设备或渲染错误会返回非零状态，不自动回退 CPU。

简要机器可读证据见 [evidence/summary.json](evidence/summary.json)。详细图像及商业资产衍生物仅在本地保存。
