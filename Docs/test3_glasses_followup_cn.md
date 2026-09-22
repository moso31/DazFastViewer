# test3 双眼镜专项复查

更新：2026-09-22。用户确认其他本轮功能无问题，指出 A 眼镜层级错误、镜架穿脸。随后重新导出 `H:/g1/Scenes/test3.dbz`，并在 Blender 确认位置正确。本次使用该文件对照原生 DUF 求值；DBZ 仅用于验证。

## 根因与修复

1. **Head 层级缺失。** 场景树原来只登记网格对象的实例 ID，未登记骨骼实例 ID。B 眼镜的 `parent` 是 `#head`，A 眼镜是 `#head-3`。现在将每个骨骼的 `scene_id` 映射到对应树项，分别挂接到所属人物的 Head。选中深层对象时展开父级，并提供横向滚动。
2. **镜腿铰链定义遗漏。** Skeleton Loader 漏读场景对 `center_point`、`end_point`、`orientation` 等字段的覆盖。两副眼镜都保存了新的 Frame / Left Arm / Right Arm 定义，A 另有左右镜腿 −3° / +3° 旋转，必须使用保存的轴心和轴向。现已继承这些字段，不使用 `preview` 缓存代替输入。
3. **A 的体型参数被错误限幅。** A 将 `Youth Morph` 保存为 `-0.2`，同时把 `min` 改为 `-0.2`。原导入器只继承 `current_value`，保留资产默认下限 `0`，导致运行时结果为 `0`。这既改变头部网格，也漏掉 Head 及祖先骨骼的 ERC 中心调整，使镜架与脸部相交。现在继承场景的 `min`、`max`、`clamped`、`step_size`；共享目录缓存保留资产原值，所有实例覆盖均在复制后应用，避免人物之间污染。

DAZ 的通道定义将数值、上下限和是否限幅分别保存，参见官方 [channel_float 定义](https://docs.daz3d.com/public/dson_spec/object_definitions/channel_float/start)。此次按实际 DUF 覆盖字段修复；没有添加 A 专用位置补偿，也没有读取 DBZ 来替换渲染几何。

## 与新 DBZ 对照

参考 SHA-256：`987795c25719f3591fb83b82ebba98627d976345611da11d645060e86db0ffe0`。每副眼镜逐一比较全部 8,440 个顶点，单位为毫米。

| 对象 | 修复前 RMS | 修复后 RMS | 修复后最大误差 |
|---|---:|---:|---:|
| A 眼镜 | 7.177236 | 0.000500 | 0.000961 |
| B 眼镜 | 0.000478 | 0.000478 | 0.001110 |

A 头部基础网格区域（基础坐标高度大于 1.6 米）的 RMS 为 0.092794 毫米，B 为 0.000514 毫米；B 的人物和眼镜相对此次修复前逐顶点不变。A 的 Head 中心恢复到 DAZ 参考值，`Youth Morph` 最终值恢复为 `-0.2`。

新参考确认之前的偏差来自本工程，不能再用旧 DBZ 的时间差解释。首次复查只修正层级和铰链时，A 仍有约 7.177 毫米平移差，属于中间结果。

## 验证与证据

- `artifacts/test3-glasses/channel-full-build.log`：重新构建全部目标，12 项 CTest 全部通过并部署到 `out`。外层 PowerShell 因 CMake 开发警告显示非零返回；日志中的编译、CTest 和部署步骤均已完成。
- `scene_workflow` 新增端到端通道覆盖用例，覆盖负下限、上限、关闭限幅、默认字段继承、网格形变和 Head 中心 ERC；四实例按正反顺序发现，并分别执行同步 / 延迟目录路径，验证缓存隔离。已有铰链、骨骼附件与恢复测试继续通过。
- `channel-after.json`：原生 DUF 求值顶点；`channel-dbzdiff.json`：新 DBZ 逐顶点对照；`channel-head-diff.json`：头部区域、中心和参数值对照。
- `channel-ui/editor-check.json`：原始完整场景在副屏检查通过，两副眼镜的树父项均验证为所属人物的 Head。`A-head-attachment.png` 与 `B-head-attachment.png` 已目视复核；A 镜架下缘完整可见，先前穿入脸颊的现象消失。渲染报告无错误。

部署文件：`out/DazFastViewer.exe`；SHA-256：`6b52f2538224cc17e2a943cad997ad1ffbae3d820ee75f6eefb80a72bdaa8344`。重新启动该程序并载入原始 `test3.duf` 即可使用，无需先导入 DBZ。

本专项验证两副眼镜及头部对齐，不代表完整 Iray / 全场景 Golden。A 全身基础网格对 DBZ 仍有 20.641 毫米 RMS，B 为 0.893 毫米；这些数据保留在对照报告中，不能将眼镜误差数字扩展到全身或全部服装。008 未开始。本轮按用户要求，以中文提交说明将代码、测试和文档归档至本地 Git；提交记录见 `git log`。
