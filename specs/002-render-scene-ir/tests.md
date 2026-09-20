# 验证与证据

更新：2026-09-20。不执行重复性能回放。

| 项目 | 结果 | 证据 |
|---|---|---|
| Release 构建 | PASS | `build/compile-dson-color.log` 及后续最终构建日志 |
| IR / DSON 校验 | PASS | `SceneIRTest`，CTest `scene_ir_dson`；索引越界、非有限相机 / 材质被拒绝 |
| 实际资产静态渲染 | PASS | Spec 003 两个 DUF 的副屏预览记录 |
| 角色相机 Delta | PASS | `artifacts/dson-character/camera-check.json` |
| 相机更新不重建网格 | PASS | 创建 2 个网格（资产 + 地板），4 次相机设置（初始 + 3 次输入）；静态脏 epoch 只有初始 2 |
| 停动后的最终相机 | PASS | 最终输入和呈现 epoch 均为 5 |
| 材质 Delta 图像 | NOT_RUN | 仅完成接口与入参校验 |
| 完整 GPU 资源上传审计 | NOT_RUN | 主机对象创建计数不能替代上传字节计数 |
| 严格 Visible FPS | 未完成 | 沿用 Spec 001 状态；本轮预览没有 FPS 验收区间 |

角色相机检查通过 Win32 消息触发旋转、平移、缩放，只向所启动进程的副屏窗口发送消息，不移动系统鼠标。见 [`check_dson_preview.py`](../../tools/check_dson_preview.py)。本次运行发生于后续 sRGB 修复前，颜色修复后的静态截图另存，未重复相机回放。

CTest 使用自建合成资源，不要求用户付费模型进入源码仓库。`tests/runtime_checks.py` 的延迟注入 / 最小化完整流程仍为 NOT_RUN，不能因相机检查复用了其中窗口查找函数而标记通过。
