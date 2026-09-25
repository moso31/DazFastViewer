# PowerPose 松手后的白模保留

2026-09-25。用户反馈 PowerPose 松手后会短暂显示编辑前的渲染图，造成姿势跳回的错觉；要求与 IK 一样，保留最后白模直到新姿态首帧完成。

## 原因与修改

此前 PowerPose 已设置“正在恢复”状态，但恢复逻辑仍读取 `PoseDrag` 的输入版本和代理网格。没有先用过 IK 时，这个版本通常为 0，现有旧图便可能满足退出等待的条件；使用过 IK 时，又可能取到上一次 IK 的白模。

现在恢复状态独立记录本次工具、场景版本和输入版本。PowerPose 等待期间绘制自己的最后白模，IK 继续绘制自己的最后白模；只有本次提交已经应用且对应渲染版本的首帧就绪，才切回正常渲染。白模覆盖期间不将底下的旧渲染图记为已呈现。取消操作继续恢复编辑前画面。

修改集中在 `src/editor/renderer.cpp`，没有改变姿态计算、模板映射或渲染采样设置。

## 验证与发布

新增的副屏检查位于现有 `powerpose_test.inl`：每次收到提交、而新输入尚未应用时，断言仍在白模等待，并保存 `*-waiting.png`；最终须完成向新渲染版本的切换。保留已有骨骼／整体变换、约束、取消和几何恢复检查。

工程规定的 20 项 CTest 已通过。构建日志位于 `artifacts/009-powerpose/white-hold/release-build.log`；日志中的 CMake CMP0146 警告不影响编译、测试与部署结果。

带服装、GeoGraft 和 35,638 根发丝的复杂场景通过 15 项副屏交互检查。其中 12 次提交分别保存拖动末帧及松手等待帧，12 对完整窗口截图逐像素一致；日志中 12 次提交后的首张正常呈现均属于新的渲染版本。另 3 次取消正常，所有操作恢复后的逐顶点误差为零。证据位于 `artifacts/009-powerpose/white-hold/powerpose`，像素及渲染版本核对位于同级 `white-frame-checks.json`。

这次只针对首帧交接与共享 IK 路径验证，没有重复模板方向或全库性能测试；复杂场景沿用此前提取的角色 A 及挂接物。

Genesis 8 Female 的既有 IK 副屏回归也通过，完成至 stage 27（14 条检查），普通 IK 与固定角度 IK 的首张正常呈现均属于新的渲染版本。证据位于 `artifacts/009-powerpose/white-hold/ik`。两组 GUI 都在副屏 `VG279QL1A`，不移动系统光标。

已部署 `out/DazFastViewer.exe`，SHA-256 为 `f0faf1497e40e88fbd0946923971877e71861e60caa83c8837894eaf9e13aa79`；发布清单的 161 项源码哈希与工作区一致。验证摘要见 [white-proxy-hold.json](evidence/white-proxy-hold.json)。重启程序后生效，本轮未创建 Git 提交。
