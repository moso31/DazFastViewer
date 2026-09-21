# 验证与手动预览

工程回归运行 `tools/build.ps1`，包含新增 `skeleton_pose` 与既有五项 CTest。不会自动启动 demo 或性能回放。新增验证包含旋转枢轴、orientation、旋转次序、父子位移、LBS 缩放继承、DQS 体积与半球、非法权重 / 通道、同名 Alias、Morph 与姿势组合、重复应用、两角色隔离及恢复。

真实批处理由 `build\Release\SkeletonPoseTest.exe <角色.duf> <姿势目录> <输出目录> [内容库...]` 执行。输出每个姿势的匹配、未应用参数、顶点变化和恢复误差；三个代表姿势的网格导出只存入被 Git 忽略的 `artifacts`。

独立数学检查命令：

```cmd
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python-exit-code 1 --python tools\reference\pose_reference.py -- --directory artifacts\spec006
```

手动预览可在 CMD 执行：

```cmd
cd /d D:\Github\DazFastViewer
powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_sample.ps1 -Sample Genesis8 -Pose "H:\G1\People\Genesis 8 Female\Poses\Bed Hogs II for Genesis 8 Female\BH2 Basking.duf"
```

编辑器只在第二屏启动。加载后选择角色，使用“姿势 → 应用姿势 DUF…”切换目录中的文件，也可从“内容浏览器”双击姿势文件。应用后自动取景；“视图 → 框选当前对象”可重新取景。点击“姿势应用详情”查看未生效项，“姿势 → 恢复载入姿势”恢复骨骼并保留当前 Morph，“重置选中对象”同时重置形态与姿势。

可用 `-Sample Character` 改为 G8.1。`out\DazFastViewer.exe --file <角色.duf> --pose <姿势.duf>` 是直接入口；`--pose-test <姿势.duf>` 仅用于一次应用 / 截图 / 恢复 / 相机脏传播检查后自动退出。

未开启 JCM 时肘、膝等大幅弯曲可能缺少 DAZ 修正形态；眼睛控制器尚未求值。床姿势的绝对位移保留原值，本阶段不做床面贴合、碰撞或 IK；这些情况应与基本骨骼蒙皮故障区分。
