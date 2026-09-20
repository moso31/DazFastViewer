# 验证方法与实际结果

## 自动校验

`SceneIRTest`（CTest 名称 `scene_ir_dson`）通过。合成输入覆盖 UTF-8 路径与百分号编码、内容根目录、JSON / gzip、外部 DSF、共享网格与实例材质绑定、四边形三角化、UV 接缝、厘米 / 轴转换、`current_value`、无效 count、越界顶点、非有限相机 / 材质拒绝。最近一次耗时约 0.05 秒。

这些测试不是完整 DSON 合规测试，也没有验证完整 DAZ 变换或材质等价性。

## 用户指定真实资产

| 输入 | 基础顶点 | 三角形 | 材质 | 已映射贴图 | 诊断 | 结果 |
|---|---:|---:|---:|---:|---:|---|
| Genesis 8.1 Basic Female | 16,556 | 32,736 | 17 | 9 | 20 | 解析及副屏预览通过 |
| ARK Food Plate with Fries | 1,794 | 3,536 | 2 | 5 | 3 | 解析及副屏预览通过 |

表中不含预览地板。完整路径在总体规范开头，实际文件中道具只有盘子与薯条，未发现汉堡网格。

预览采用 OptiX、1600×900、64 samples，不启用 denoise。日志记录第二屏 `\\.\DISPLAY2`，窗口外框 `(2712,46)–(4328,985)`；角色相机检查还通过 Win32 读取并校验了窗口位置。退出时截图会有一次回读；它不计入运行中的 GPU interop 回退字节数。

运行经过了针对已发现过曝与角色偏灰问题的修正复查。没有运行自动性能轨迹或新的一组 FPS 基准。具体故障和证据见 [execution-report.md](execution-report.md)。

## 仍未验证

- 与 Blender / DAZ 导入器的材质、几何和完整变换 Golden 对照。
- 完整 Iray Uber、SubD / HD、Morph / Skinning 等未实现语义。
- DAZ 相机导入、其他 DPI 布局、第二屏缺失的实际拔屏测试。
- 材质 Delta 的运行时图像验收与纹理文件替换。
- Spec 001 的 PresentMon Visible FPS 和 `tests/runtime_checks.py` 完整流程。
