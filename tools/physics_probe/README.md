# 快速物理独立探针

仅用于 [可行性调研](../../Docs/fast_physics_feasibility_cn.md)，不接入主程序，也不部署 `out`。测试刚体落地／无地面下落，以及移动固定边的三档布料与简单障碍碰撞。它不验证真实 DAZ 资产、头发、Qt 交互或 Cycles 性能。

本机已验证：Windows x64、VS 2022、MSVC 19.44、CMake 3.31.6、Jolt v5.6.0，提交 `e77f175595e64cb44218cc9d9d56fc365ad0e36a`。

在仓库根目录用 PowerShell 执行（已有源码时跳过 clone）：

```powershell
git clone --depth 1 --branch v5.6.0 https://github.com/jrouwe/JoltPhysics.git .research/JoltPhysics-v5.6.0
git -C .research/JoltPhysics-v5.6.0 rev-parse HEAD
$physicsCmake = 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
& $physicsCmake -S tools/physics_probe -B .research/build-physics-probe -G 'Visual Studio 17 2022' -A x64
& $physicsCmake --build .research/build-physics-probe --config Release --target DfvPhysicsProbe --parallel 2
& .research/build-physics-probe/Release/DfvPhysicsProbe.exe
```

每条输出为 JSON。退出码 0 表示全部数值检查通过，1 表示失败。`prepare_ms` 包含约束生成和软体创建；每步计时包含锚点蒙皮和物理更新，舍弃前 30 步，统计后 330 步；不包含结果检查或图形处理。使用一个调用线程及一个引擎工作线程，不启用 GPU。

默认使用距离式弯曲约束。以下命令保留本次调研发现的不稳定参数组合，**预期退出码为 1**，用于复现局限，不属于通过项：

```powershell
& .research/build-physics-probe/Release/DfvPhysicsProbe.exe --dihedral
```

编译设置仅作用于这个独立 CMake 项目；Jolt 使用静态库和动态 MSVC CRT，不下载额外图形 SDK。Jolt 默认启用的部分 SIMD 选项适合本机 CPU，不能据此承诺所有 x64 机器兼容。正式集成需重新核对目标 CPU、主程序编译选项与 Jolt 许可分发。
