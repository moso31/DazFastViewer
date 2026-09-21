# 006 执行报告

日期：2026-09-21。006 的基础骨架、蒙皮与单帧姿势 DUF 实施范围已完成，Release 已部署到 `out/DazFastViewer.exe`；等待用户手动验收。此状态不代表整个 Phase 5 或 DAZ Studio Golden 已完成。

编辑器新增“姿势 → 应用姿势 DUF / 恢复载入姿势”、应用详情、骨骼层级及框选当前对象。Explorer 双击姿势会应用到选中角色。Morph 在蒙皮前计算，恢复姿势保留 Morph，重置对象同时恢复形态与姿势。命令见 [手动预览](tests.md)。

## 验证结果

| 检查 | 结果 |
|---|---|
| Release 编译与部署 | PASS |
| 六项工程 CTest | 6 / 6 PASS；新增骨架 / 姿势数值、失败输入及多角色隔离用例 |
| Genesis 8 批处理 | 用户目录 40 / 40 姿势通过基础蒙皮检查 |
| Genesis 8.1 批处理 | 同目录 40 / 40 姿势通过基础蒙皮检查 |
| 骨架 / 权重 | 各 171 节点（Figure + 170 骨骼），16,556 顶点、36,666 条正权重；无未加权顶点 |
| 姿势覆盖 | 每个角色匹配 11,997 条骨骼通道；每个姿势均改变全部 16,556 顶点，重复应用不重算 |
| 恢复误差 | 两个角色全部姿势均为 0 米 |
| 独立数学对照 | Basking、Fetal Position on Pillow、Stretch and Yawn，各覆盖 G8 / G8.1；最大顶点差 `6.431e-7 m`，约 `0.00064 mm` |
| 副屏编辑器 | PASS；`VG279QL1A`，窗口 `(2729,55,1580,920)` |
| 增量渲染 | 姿势 / 恢复各一次顶点更新；网格始终 2 个；蒙皮累计 3 次（初始、应用、恢复）；相机未触发 Morph / Skinning |
| 终态同步 | requested / presented epoch 均为 6，revision 均为 3；恢复最大位移为 0 |

独立对照使用本机 Blender 5.2.2 的 mathutils 与 NumPy，重新读取原始 DSF 顶点 / 骨架 / 权重和姿势。没有使用 Diffeomorphic 求值或 DAZ Studio 导出，因此这是基础蒙皮的独立数学检查，不是 DAZ 最终形态 Golden。

第一轮副屏联调暴露 Figure 控制器和子骨骼 Alias 同名误判，已按所属节点与 name / id 修复，并增加回归用例；修复后只重跑该副屏流程。首次 CTest 未限定工程范围时，Cycles 自带 `cycles_version` 因未部署通用 `out/cycles.exe` 而未运行；最终使用工程构建脚本规定的六项测试范围，全部通过。没有进行性能回放或重复运行完整内容库基准。

## 当前边界

- 40 个姿势中，33 个含非零 `eCTRLEyesUpDown`、33 个含非零 `eCTRLEyesSideSide`。这些 ERC 控制器尚未应用，逐文件报告与界面详情明确列出；基础骨骼通道全部匹配。批处理的 PASS 不等于每个文件 `fully_applied=true`。
- JCM、体型驱动骨骼中心、完整 ERC、HD / SubD 尚未实现。大幅弯曲的关节外形可能与 DAZ 最终结果不同，留给 007 及后续 Golden。
- 当前 DualQuat 只支持刚性骨骼变换；含非单位骨骼缩放的预设整次拒绝。对象缩放仍可使用。Linear 为数学实现与合成验证，未另取真实 Linear 角色验证。
- 只支持时间 0 的单键姿势，不支持多帧动画、TriAx / Blend、独立缩放权重、跨骨架重定向、FitTo 或 IK。角色实例的初始世界放置仍由既有 Instance 变换负责；非单位 Figure 根变换的完整 DAZ 场景对照仍待扩展。
- 床姿势中的绝对骨骼位移保留原值；预览地板不进行碰撞 / 贴合。截图是交互采样结果，不作为材质质量或性能 Gate。

## 证据

可入库摘要与源码 / 程序 / 报告哈希见 [evidence/summary.json](evidence/summary.json)。程序 SHA-256：`08feda362e646fb07239785dcf78d9945e52fc96147ee341f07718efed489ad8`。

完整本地证据：`artifacts/spec006/{g8,g81}/batch-report.json`、`reference-report.json`、`ctest-final.log`、`editor-final/editor-check.json`、`editor-final/editor-render.json`、`editor-final/editor-pose.png`、`editor-final/editor-pose-reset.png`。原始商业资源、网格导出和截图不加入 Git。
