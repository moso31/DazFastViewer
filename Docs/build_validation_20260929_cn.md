# Windows 构建验证记录（2026-09-29 / 2026-09-30）

本记录只描述本次机器上实际执行的结果，不复用原开发机的验证结论。构建入口见 [构建文档](build_windows_cn.md)。

## 环境

| 项目 | 实际值 |
| --- | --- |
| 工作目录 | `D:/DazFastViewer` |
| OS | Windows 11 专业版，10.0.26200 |
| CPU | Intel Core i7-11800H |
| RAM | 约 24 GB |
| GPU | NVIDIA GeForce RTX 3060 Laptop GPU，计算能力 8.6 |
| 驱动 | 556.12，未修改 |
| Visual Studio | Community 2022 17.14.36616.10 |
| MSVC | 19.44.35219，v143，x64 |
| Windows SDK | 10.0.26100.0 |
| Python | 3.10.2 x64，工程内虚拟环境 |
| CMake | 自动安装 4.2.3；原 PATH 中的 3.22.2 未使用 |
| Git | 自动发现 VS 附带的 2.51.0.windows.2 |
| Qt | 自动安装 6.10.3 MSVC 2022 x64 |
| CUDA | 自动下载项目内 nvcc 12.9.41 / cudart 12.9.37 / cccl 12.9.27 |
| OptiX | 自动下载 9.1.0 头文件 |

## 已验证的环境准备

- `tools/bootstrap.ps1 -Proxy http://127.0.0.1:7890` 完成：固定 Git 提交、89 个 Windows 库 LFS 对象（约 137 MiB）、Jolt / OptiX / CUDA 归档、Qt。代理地址为本机设置，不是工程默认值。
- `tools/bootstrap.ps1 -Offline` 完成：不再下载依赖，检查已有提交、LFS 物化和必要工具 / Qt 文件。
- `build.ps1 -VisualStudio 2022 -ConfigureOnly -BuildDir 'build/path with spaces'` 完成：含空格的构建目录配置成功。
- 未通过构建脚本修改 PATH，直接用工程 CMake 执行 `cmake --preset vs2022 -B build/direct-preset -DCYCLES_CUDA_BINARIES_ARCH=sm_86`，配置 / 生成成功；VS 附带 Git 和本地 Python 也能被发现。
- Python 构建工具测试 5 项通过，覆盖离线损坏缓存拒绝、校验缓存复用、归档路径穿越拒绝、空格 / Unicode 文件名解压和 CMakeCache 路径解析。
- PowerShell 脚本语法检查通过；VS / Git / Python / CMake 自动发现正常。
- CMake 能识别 `vs2022` / `vs2026` 两个 preset；本机未安装 VS 2026，显式选择 2026 时脚本给出缺失工具链错误，不误用 VS 2022 冒充成功。

## 本次编译与测试

执行命令：

```powershell
.\tools\build.ps1 -VisualStudio 2022 -Configuration Release -Jobs 4
```

构建自动识别本机 GPU，选择 `sm_86`，保留 Release 优化、Jolt SIMD、Cycles 渲染配置。

| 检查 | 结果 |
| --- | --- |
| VS 2022 x64 Release 全量目标 | 通过；生成编辑器、命令行基准程序、测试和 CUDA / OptiX 内核 |
| 运行文件暂存 | 通过；Qt 插件、中文翻译、Windows 库、Release CRT、压缩内核、许可文件和构建清单已整理 |
| CTest | **31 / 31 通过**，一次完整运行 21.56 秒；包含原有 29 项应用回归、构建工具测试组、上游 Cycles 版本测试 |
| 构建工具测试组 | 5 项子测试通过，已纳入 CTest |
| 迁移运行目录 | 复制到 `artifacts/runtime relocated`，把 PATH 限制为 Windows / System32 后，设备枚举和编辑器 `--help` 均以 0 退出 |
| OptiX 冒烟 | 未渲染；以 1 退出，明确报告驱动过旧、要求 R590+ |

这次干净环境暴露并修复了：新版 CMake 的模块扫描与同名源文件冲突、残留的 pthread 链接项、Qt 中文翻译部署缺少 `lconvert`、上游 `cycles_version` 测试错误地从安装目录寻找程序。最终统一构建脚本返回 0。

主程序位于 `out/vs2022/Release/DazFastViewer.exe`。迁移检查验证了可执行文件及启动依赖可加载，**不代表 GPU 画面、交互性能或画质对照已经通过**。

### 根目录解决方案补充验证

主程序解决方案现生成于 `D:\DazFastViewer\DazFastViewer.sln`，默认启动项目为 `DazFastViewer`。已检查 69 个项目引用全部有效，直接使用 MSBuild 对根目录 SLN 构建 `DazFastViewer / Release / x64`，退出码为 0。IDE 构建会自动把依赖部署到 `build/vs2022/bin/Release`；以此目录为工作目录、PATH 仅保留 Windows / System32 启动主程序 `--help`，退出码为 0。含空格构建目录的 SLN 导出也已核对路径有效。

以上验证覆盖解决方案引用、实际构建和启动依赖加载；未通过 Visual Studio UI 点击 F5，也未绕过上述驱动限制验证 GPU 渲染。日志为 `.deps/root-solution-configure.log`、`.deps/root-solution-build.log`、`.deps/root-solution-launch.log`。

部署 IDE 启动依赖后，额外为 CTest 显式设置了开发套件的 `QT_PLUGIN_PATH`，保证无窗口测试能找到 `offscreen` 插件；主程序正常显示配置不变。最终重跑 **31 / 31 通过**，耗时 30.45 秒。

本机保留的日志：

- `.deps/bootstrap.log`、`.deps/bootstrap-offline.log`
- `.deps/configure-spaces.log`
- `.deps/configure-direct-preset.log`、`.deps/gpu-prerequisite.log`
- `.deps/build-vs2022.log`
- `.deps/bootstrap-exit-check.log`、`.deps/build-exit-check.log`（直接检查子进程退出码，均为 0）
- `.deps/runtime-smoke.json`
- `build/vs2022/Testing/Temporary/LastTest.log`

这些目录由 `.gitignore` 排除；运行目录中的 `build-manifest.json` 会记录实际配置和文件哈希。

## 驱动限制及未验证范围

本机驱动 **556.12 低于 OptiX 9.1 要求的 R590**，因此需要用户更新显卡驱动后继续 GPU 验证。依据是 [NVIDIA OptiX 9.1 官方发布说明](https://forums.developer.nvidia.com/t/optix-9-1-release/354119)。工程没有降级 OptiX、改变采样 / 材质设置、关闭 SIMD 或回退 CPU 来规避该限制。

驱动更新后执行：

```powershell
.\tools\check_environment.ps1 -RequireGpu
& .\out\vs2022\Release\CyclesViewportBench.exe --devices
& .\out\vs2022\Release\CyclesViewportBench.exe --smoke --device OPTIX --monitor 1 --output artifacts/smoke-optix
```

VS 2026 实机编译、Debug / RelWithDebInfo 全量构建、其他 GPU 架构、Qt 从源码构建，以及新驱动下的画质 / 性能基准尚未在本机验证。VS 2026 的配置采用 CMake 4.2 生成器和 v143 工具集，文档明确了安装要求；不能把配置支持等同于实机编译通过。

## 2026-09-30：Windows/NVIDIA 迁移修复与多架构 Release

将构建迁移改动接入当前工作区，修复主程序强制副屏启动、嵌入视口固定显示器、首次显示时的 DPI 边框偏移。编辑器根据实际 OpenGL 上下文选择对应的 OptiX 设备，导出 `NvOptimusEnablement` 请求 NVIDIA GPU；默认设置、日志和缓存使用用户目录。保留既有项目文件及显式路径的优先级。

实际构建命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap.ps1 -Offline
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -VisualStudio 2022 -Configuration Release -CudaArchitectures Common -Jobs 4
```

| 验证 | 本次结果 |
| --- | --- |
| 干净 CMake 配置 | `build/compatibility check` 新目录成功；含空格，调用环境的 PATH 中没有 Git，工程自动发现 VS 附带 Git |
| Release 完整构建 | 成功；七种原生 CUDA 内核 `sm_75/80/86/89/90/100/120` 和三个 OptiX PTX 内核齐全 |
| 再次增量构建 | 成功；修复 DPI 偏移后未重新编译 GPU 内核。第一次约 37 分钟，增量约 62 秒（均含部署和测试；耗时只代表本次机器） |
| 最终 CTest | **32/32 通过，20.17 秒**；包含新增窗口位置测试及六项 Python 构建工具子测试 |
| 原生单屏窗口 | 真实 WGL 上下文创建、释放、重建以及超出工作区的嵌入窗口通过 |
| 双显卡设备匹配 | 实测 OpenGL renderer 为 RTX 3060 Laptop；CUDA/OpenGL 查询返回同一设备，ordinal 0 |
| 运行包迁移 | 复制到 `artifacts/portable-release-check/runtime with space 测试`；子进程 PATH 仅保留 Windows/System32，移除 Qt/CUDA/Python 环境变量；`--help`、`--devices` 均返回 0 |
| 文件完整性 | 迁移包 68 个 EXE/DLL/内核文件与构建清单 SHA256 一致，含全部十个压缩内核 |
| 普通及额外 150% 缩放 | 完整编辑器均进入渲染设备检查；窗口分别为 `[169,55,1580,920]`、`[0,0,1280,688]`，均位于对应工作区内 |
| OptiX 渲染 | **未通过验证**；本机仍为 556.12，设备枚举明确报告驱动过旧。编辑器自检以 1 退出并记录 R590+ 要求，不能算渲染成功 |

最终运行包为 `out/vs2022/Release`（本次约 165 MiB）。主程序 `DazFastViewer.exe` 的 SHA256：`df0e8c86d8ff015ed5aa9f52cca30d984ce474dfd97126a278191850366a503b`。

本次证据：`artifacts/release-build-20260930.log`、`artifacts/release-incremental-20260930.log`、`artifacts/fresh-configure-20260930.log`、`artifacts/window-native-20260930.log`、`artifacts/environment-check-20260930.log`、`artifacts/portable-release-check/results.json`。默认构建已改为 `Common`；本机快速开发可显式用 `-CudaArchitectures Auto`。

本次验证不覆盖 Windows 10 实机、其他 NVIDIA 显卡实机、多块 NVIDIA 显卡同时安装、真实多屏混合 DPI 和新驱动下的画质/性能。相关架构已编译；离屏恢复、负坐标和不同逻辑分辨率有算法回归，不能代替所有显示器组合的实机测试。驱动更新后可继续运行上面的 OptiX 冒烟命令，以及单屏可运行的 GPU 专项目标。
