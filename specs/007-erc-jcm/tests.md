# 预览与验证

在 CMD 中进入工程并启动 Genesis 8 Female，编辑器只在第二屏打开：

```cmd
cd /d D:\Github\DazFastViewer
powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Genesis8
```

选中左侧角色，在右侧依次搜索 `Arms Length`、`Chest Scale`、`Eyes Closed`、`HS Sanny Shy` 和 `Flex Quad Left`，选择参数后调整下方滑块。可输入 `0.5`、`1`、`0`，检查变化和恢复；面板“最终值”显示 ERC 求值结果。“显示隐藏参数”可查看资源隐藏的 JCM 与开关。

通过“姿势 → 应用姿势 DUF…”选择 Bed Hogs II 目录中的文件，JCM 随骨骼变化求值。“重置选中对象”恢复载入参数和姿势。“恢复载入姿势”保留 Morph，两个操作的用途不同。

```cmd
powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Genesis8 -Pose "H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female\BH2 Basking.duf"
```

G8.1 使用 `-Sample Character`，闭眼搜索 **Eye Blink**。旧 Eyes Closed / 眼睛方向控制器被 G8.1 空文件覆盖；不要因此修改内容库优先级或拷回旧控制器。姿势应用详情会明确列出不适用于本代的旧控制器。

## 已执行的工程检查

`tools/build.ps1` 编译、部署并运行七项轻量 CTest，不启动 demo。`FormulaRuntimeTest` 无参数执行合成单元检查，提供资产参数时执行参数、Alias、JCM 开关、姿势与恢复检查：

```cmd
build\Release\FormulaRuntimeTest.exe ^
 "C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8 Basic Female.duf" ^
 "artifacts\spec007\manual-check\g8" ^
 "H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female" ^
 "H:\g1" "H:\g3" "C:\Users\Public\Documents\My DAZ 3D Library" "C:\Users\xatia\Documents\DAZ 3D\Studio\My Library"
```

将角色文件换成 `Genesis 8.1 Basic Female.duf`、输出换成另一个目录即可检查 G8.1。最终已完成的数据位于 `artifacts/spec007/validated/{g8,g81}`，每个目录包含 `formula-report.json`、`morph-catalog.json`、`regression-report.json` 及独立参考输入。

```cmd
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python-exit-code 1 --python tools\reference\formula_reference.py -- --directory artifacts\spec007\validated
```

参考报告为 `artifacts/spec007/validated/reference-report.json`；执行时不打开 Blender 窗口、不保存用户偏好。该数学对照不替代 DAZ Studio Golden。

副屏端到端检查入口为 `out\DazFastViewer.exe --file "角色.duf" --project DazFastViewer.project.json --formula-test --output "新目录"`，当前专项入口使用 G8；最终已通过的五参数截图和 JSON 在 `artifacts/spec007/validated/editor`。已有证据可直接查看，不必为查看结果重复扫描和渲染。
