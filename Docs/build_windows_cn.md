# Windows 构建与新机器配置

本文是当前构建入口。`specs/001-cycles-static-camera/` 中的原机器记录用于追溯，不再是新机器的安装步骤。

## 快速生成可分发的 Release

在项目根目录打开 PowerShell。构建机需要 VS 2022 的“使用 C++ 的桌面开发”（v143 和 Windows SDK）及 Python 3.10+ x64；Git 可以使用 VS 自带版本。

```powershell
# 首次准备依赖；已有依赖时可用 -Offline 检查，无需重复下载
powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap.ps1

# 配置、编译、整理运行包并执行测试；此后修改源码只需重复这一条
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -VisualStudio 2022 -Configuration Release
```

完成后运行 `out\vs2022\Release\DazFastViewer.exe`。**复制或压缩整个 `Release` 文件夹，不能只取 EXE**：DLL、`platforms`、`lib`、其他 Qt 插件和许可文件都是运行包的一部分。目标机器无需 VS、Python、Qt 开发套件或 CUDA Toolkit，但必须满足下述系统、CPU 和显卡驱动要求。初次编译七种 CUDA 架构耗时较长，后续增量构建复用已有内核。构建不需要可用的 OptiX 驱动；实际渲染需要。

只在本机开发时可加 `-CudaArchitectures Auto` 缩短首次编译时间。准备分发时用默认的 `Common` 再构建，部署步骤会拒绝缺少任何目标内核的运行包。

## 1. 支持范围与兼容原则

工程支持 **Windows x64 + MSVC + NVIDIA GPU**。当前编辑器使用 OptiX，命令行基准程序支持显式选择 OptiX 或 CUDA。暂不支持 Linux、macOS、Windows ARM64、AMD/Intel GPU 渲染或 CPU 渲染回退。

普通编辑器支持单屏、多屏及不同 DPI，首次启动使用主屏，之后保留可见的已保存位置；移除屏幕后会把离屏窗口移回可用区域。自动交互诊断在有副屏时优先使用副屏，无副屏时使用主屏。嵌入视口跟随宿主窗口，不再固定要求第二屏或 1580×920 工作区。

新安装的默认项目设置和日志写入用户本地数据目录，Cycles 缓存也位于用户目录，运行包不要求安装目录可写。已有 EXE 上级目录的 `DazFastViewer.project.json` 继续读取，显式 `--project` / `--output` 路径仍优先。内容库路径需在新机器的“项目设置”中指定。

兼容性以不影响画质和运行性能为优先：保留现有 Cycles、采样、材质、物理引擎及 SIMD 优化；不以降低分辨率、采样数、关闭功能或改为 CPU 渲染解决环境问题。仅在验证效果与性能均不受影响时才采用旧依赖。当前没有把低版本驱动 / OptiX 组合列为已验证回退方案，不符合要求时应升级对应组件。

| 项目 | 要求 / 默认版本 |
| --- | --- |
| 操作系统 | Windows 10/11 x64；建议 Windows 11 |
| CPU | x64，支持 AVX2、FMA、F16C、BMI/TZCNT、LZCNT；保留 Jolt 上游默认优化 |
| 内存、磁盘 | 建议 16 GB 以上内存，预留 15 GB 以上磁盘；多套 Debug/Release 和多 GPU 架构需要更多空间 |
| Visual Studio | VS 2022 17.14，或 VS 2026；Community / Professional / Enterprise / Build Tools 均可发现 |
| C++ 工具集 | **MSVC v143**，本机实测 14.44；VS 2026 同样选择 v143 |
| Windows SDK | 安装 Windows 10/11 SDK；本机使用 10.0.26100.0 |
| Python | 3.10+，64 位；用于依赖准备和运行文件部署 |
| Git | Git for Windows，或 Visual Studio 附带 Git；无需全局安装 Git LFS |
| CMake | 4.2+；脚本在工程内安装 4.2.3 |
| GPU | OptiX 9.1 支持的 Turing 或更新 NVIDIA GPU；建议 RTX 20/30/40/50 系列。显存需求取决于场景 |
| NVIDIA 驱动 | **R590 或更高**，这是固定的 OptiX 9.1 的要求；`nvidia-smi` 中的 CUDA 数字不代表 Toolkit 已安装 |

VS 2026 的生成器 `Visual Studio 18 2026` 从 CMake 4.2 起提供；CUDA 12.9 的 Windows 编译器支持针对 VS 2022，因此这里将 **IDE 版本与编译器工具集分开**：VS 2026 使用 v143，不默认采用 v145，也不添加 `allow-unsupported-compiler`。参考 [CMake 生成器说明](https://cmake.org/cmake/help/v4.2/generator/Visual%20Studio%2018%202026.html)、[CUDA 12.9 Windows 安装说明](https://docs.nvidia.com/cuda/archive/12.9.0/cuda-installation-guide-microsoft-windows/index.html)、[NVIDIA OptiX 9.1 发布要求](https://forums.developer.nvidia.com/t/optix-9-1-release/354119)。

## 2. 第一次使用：安装基础工具

这些步骤只需要在没有基础开发环境时执行。已有 VS / Python / Git 可直接进入下一节。

1. 安装 Visual Studio 2022 或 2026，在 Visual Studio Installer 中选择 **使用 C++ 的桌面开发**。
2. 在“单个组件”中确认安装 **MSVC v143 - VS 2022 C++ x64/x86 build tools**、Windows 10/11 SDK。VS 2026 尤其需要检查 v143，不要只安装默认 v145。安装器需要管理员权限，可能要求重启。
3. 安装 Python 3.10+ 和 Git。脚本会优先发现 Python Launcher `py`，并能找到 VS 附带的 Git，不要求它们已经加入全局 PATH。

有 winget 时，可以在 PowerShell 安装：

```powershell
winget install --id Python.Python.3.12 -e
winget install --id Git.Git -e
# 没有完整 IDE、只需要命令行构建时，可以安装 VS 2022 Build Tools：
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

安装完成后重新打开终端。无 winget 时，从 [Visual Studio](https://visualstudio.microsoft.com/downloads/)、[Python](https://www.python.org/downloads/windows/)、[Git for Windows](https://gitforwindows.org/) 官方下载并安装。

显卡驱动请从 [NVIDIA 官方驱动入口](https://www.nvidia.com/Download/index.aspx) 按显卡和操作系统更新，或使用设备厂商支持的 R590+ 驱动。依赖脚本不修改显卡驱动，不执行系统重启。

程序通过 `NvOptimusEnablement` 请求 NVIDIA 高性能 GPU，编辑器按实际 OpenGL 上下文选择匹配的 OptiX 设备。Windows 中手动设置的应用图形偏好仍可能覆盖此请求。若提示显示设备不匹配，请在 Windows“设置 → 系统 → 屏幕 → 显示卡”中将实际运行的 `DazFastViewer.exe`（使用基准程序时还有 `CyclesViewportBench.exe`）设为高性能 NVIDIA GPU，再重启应用。参考 [NVIDIA 图形处理器设置](https://www.nvidia.com/content/Control-Panel-Help/vLatest/en-us/mergedProjects/nv3d/Manage_3D_Settings_%28reference%29.htm) 和 [CUDA/OpenGL 设备查询](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__GL.html)。

## 3. 一次准备，随后一条命令构建

在工程根目录执行；普通 PowerShell 5.1 或 PowerShell 7 均可，不需要先打开 Developer PowerShell。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\bootstrap.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -VisualStudio 2022
```

VS 2026：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -VisualStudio 2026
```

`-ExecutionPolicy Bypass` 只作用于本次 PowerShell 进程，不修改系统执行策略。省略 `-VisualStudio` 时选择发现的较新 VS；机器同时安装两个版本时建议明确指定。

准备脚本会：

1. 检查 VS C++、Python、Git，发现缺失时给出具体安装说明。
2. 创建 `.deps/python` 虚拟环境，安装固定的 CMake 4.2.3 和 aqtinstall 3.3.0；不修改系统 Python。
3. 从 Blender 官方 Git 服务器取得固定提交的 Cycles、Blender 参考源码和 Windows 库元数据。
4. 仅下载需要的 Windows 库，通过公开 Git LFS batch API 物化二进制，并核对 Git 指针记录的 SHA256 和长度；无需额外安装 Git LFS。
5. 下载并校验 Jolt、OptiX 头文件、NVIDIA 官方 CUDA 12.9 编译组件压缩包，解压到 `.deps`。此 CUDA 目录是项目使用的编译工具子集，不是包含 Nsight 等工具的系统完整安装。
6. 使用 aqtinstall 安装 Qt 6.10.3 的 `qtbase`、`qtsvg`、`qttools`、`qttranslations`，包含 MSVC x64 的 Release/Debug 库。Qt Tools 提供部署中文翻译所需的 `lconvert.exe`。
7. 生成不进入版本管理的 `.deps/local.cmake`，记录本地工具路径。

构建脚本会配置 CMake、构建默认目标、暂存 EXE / DLL / GPU 内核，再执行全部已注册 CTest。任何一步失败都会返回非零退出码。首次会编译 Jolt、Cycles 和 GPU 内核，耗时明显长于后续增量构建。

VS 2022 Release 的依赖位于 `.deps`，构建位于 `build/vs2022`，运行包位于 `out/vs2022/Release`，工程根目录的 `DazFastViewer.sln` 引用该构建目录。依赖脚本可以重复执行；已准备的源码和归档可复用，下载中断可重试，不会删除用户已有的构建目录或自动切换版本不匹配的源码 checkout。

```text
DazFastViewer.sln         根目录的 Visual Studio 主程序入口（自动生成）
.deps/
  python/                  Python 虚拟环境、CMake、aqtinstall
  sources/{cycles,blender,jolt}/
  windows-libs/            固定版本的头文件、LIB、DLL
  qt/6.10.3/msvc2022_64/
  cuda/                   CUDA 12.9 编译组件
  optix/                  OptiX 9.1 头文件
  cycles/                 由 prepare_cycles.py 生成的带项目补丁源码
  downloads/              已校验的归档缓存
  local.cmake             本机路径
build/vs2022/             VS 2022 工程、编译产物、CTest 日志
build/vs2026/             VS 2026 的独立构建目录
out/vs2022/Release/       可启动的程序、DLL、lib/*.zst、构建清单
out/vs2022/Debug/         Debug 开发运行目录
```

不要将 `.deps`、`build`、运行包、生成的 `DazFastViewer.sln` 或 `CMakeUserPresets.json` 提交到仓库。不要把原机器的 CMakeCache、Python 虚拟环境复制到另一台机器继续使用；绝对路径和工具链可能已改变。新机器执行构建脚本会重新生成根目录解决方案。

## 4. 常用构建选项

```powershell
# Debug（与 Release 共用 VS 解决方案，但运行文件分目录）
.\tools\build.ps1 -VisualStudio 2022 -Configuration Debug

# 带调试信息的优化构建
.\tools\build.ps1 -VisualStudio 2022 -Configuration RelWithDebInfo

# 仅生成解决方案，随后在 Visual Studio 中打开
.\tools\build.ps1 -VisualStudio 2022 -ConfigureOnly

# 内存较小的机器减少并发；不改变最终程序优化或画质
.\tools\build.ps1 -VisualStudio 2022 -Jobs 2

# 自定义目录；切换生成器/工具集时必须换 BuildDir
.\tools\build.ps1 -VisualStudio 2022 -BuildDir D:\dfv-build\vs2022 -OutputDir D:\dfv-run\Release

# 已确认的高级 CMake 覆盖
.\tools\build.ps1 -VisualStudio 2022 -CMakeOptions @('-DDFV_QT_ROOT=D:/SDK/Qt/6.10.3/msvc2022_64')
```

`-SkipTests` 适合本地快速编译，不代表测试通过。`-SkipStage` 跳过运行文件整理，裸编译目录不保证能直接启动完整渲染；CTest 自己配置测试所需的 DLL 路径。

`-CudaArchitectures Common` 是默认值，构建 `sm_75;sm_80;sm_86;sm_89;sm_90;sm_100;sm_120` 原生 CUDA 内核，覆盖常见 NVIDIA 架构。OptiX 的 PTX 内核另行生成，由目标驱动编译。仅为本机开发时可选择自动检测：

```powershell
.\tools\build.ps1 -VisualStudio 2022 -Configuration Release -CudaArchitectures Auto
```

`Auto` 使用 `nvidia-smi --query-gpu=compute_cap` 检测本机所有 NVIDIA GPU；例如 RTX 3060 为 `sm_86`。未检测到 GPU 时采用 `Common` 列表。**Auto 构建不是默认的多机分发包**。

为多台机器准备运行目录时必须显式包含目标架构，不能只复制为本机 GPU 编译的内核：

```powershell
.\tools\build.ps1 -VisualStudio 2022 -CudaArchitectures 'sm_75;sm_80;sm_86;sm_89;sm_90;sm_100;sm_120'
```

新增架构若不受 CUDA 12.9 支持，应升级并验证工具链；不要通过关闭内核或降低渲染质量绕过错误。编译多个架构会增加时间和磁盘占用。支持架构不等于已在每种显卡上完成画面或性能测试。

## 5. 在 Visual Studio 中工作

先执行准备脚本，再用下面的命令生成根目录解决方案：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -ConfigureOnly -VisualStudio 2022
# 使用 VS 2026 时将 2022 改为 2026
```

打开工程根目录的 **`DazFastViewer.sln`**，本机为 **`D:\DazFastViewer\DazFastViewer.sln`**。选择 **x64 / Release**，默认启动项目为 **DazFastViewer**，按 **F5** 调试或 **Ctrl+F5** 启动。若已有 `.vs` 用户设置覆盖了默认项目，右键 `DazFastViewer` →“设为启动项目”。构建全部测试等目标时可构建整个解决方案或 `ALL_BUILD`。

主程序链接后会自动把 Qt 插件、依赖 DLL、GPU 内核及许可文件部署到 `build/vs2022/bin/<配置>`，IDE 从此目录运行刚构建的 EXE，工作目录也已设置。首次只构建主程序无需先构建基准程序。这个部署步骤同样适用于 VS 2026；`-SkipStage` 会禁用它，恢复正常启动前重新运行不带该选项的构建命令。

根目录 SLN 引用 `build/vs2022` 或 `build/vs2026` 中的实际项目文件，不把编译中间文件散落在根目录。每次运行构建脚本以及 IDE 构建都会刷新入口；**最后一次配置脚本或构建所选的构建目录决定根目录 SLN 指向哪套工程**。切换 VS 版本或自定义构建目录时，先关闭当前解决方案，重新执行对应版本的 `build.ps1 -ConfigureOnly`，再打开根目录 SLN。不要手动复制构建目录中的 SLN，相对项目路径会失效。

工程也提供 `CMakePresets.json`。在配置了 CMake 4.2+ 的 VS“打开文件夹”工作流中选择 `vs2022` 或 `vs2026`。VS 自带 CMake 若过旧，应在其 CMake 设置中选用 `.deps/python/Scripts/cmake.exe`，或采用上述生成解决方案方式。

命令行等价操作：

```powershell
$cmake = "$PWD/.deps/python/Scripts/cmake.exe"
& $cmake --preset vs2022
& .\.deps\python\Scripts\python.exe tools\export_solution.py --build-dir build/vs2022
& $cmake --build --preset vs2022-release
& .\.deps\python\Scripts\ctest.exe --preset vs2022-release
& .\.deps\python\Scripts\python.exe tools\stage_runtime.py --build-dir build/vs2022 --configuration Release
```

直接使用 preset 时默认构建全部上述 GPU 架构；可以在首次配置加 `-DCYCLES_CUDA_BINARIES_ARCH=sm_86` 等参数，或创建本地 `CMakeUserPresets.json`。不要在一个已有构建目录中切换 VS 生成器。IDE 启动依赖自动部署到编译目录；需要更新 `out/vs2022/Release` 时，再运行上面的 `stage_runtime.py` 或完整的 `build.ps1`。部署脚本默认读取 CMake 配置的安装路径；Debug 等配置使用各自的目录。

有意将同配置的运行包迁移到新的构建目录时，可在 `stage_runtime.py` 命令中加 `--replace-build`，更新运行包归属和部署清单。该选项仍拒绝混用不同配置的运行包。

调试时在同一个根目录 SLN 中切换至 **Debug / x64** 即可，构建与启动目录会同步切换。Debug 用于开发，不用于衡量 Release 性能，不作为可分发包；目标机器必须有相应的调试 CRT。默认启动项目由 [CMake VS_STARTUP_PROJECT](https://cmake.org/cmake/help/v4.2/prop_dir/VS_STARTUP_PROJECT.html) 设置。

## 6. 自定义安装、代理与离线使用

已有完整 Qt / CUDA 安装时：

```powershell
.\tools\bootstrap.ps1 -QtRoot 'D:\SDK\Qt\6.10.3\msvc2022_64' -CudaRoot 'D:\SDK\CUDA\v12.9'
```

Qt 要选择 MSVC x64 套件，不能混用 MinGW、ARM64 或 32 位库。修改依赖路径后建议使用新的构建目录，避免旧 `Qt6_DIR` 等 CMake 缓存继续生效。其他可覆盖的缓存变量包括 `DFV_BLENDER_SOURCE`、`DFV_STANDALONE_SOURCE`、`DFV_LIB_DIR`、`DFV_JOLT_SOURCE`、`OPTIX_ROOT_DIR`、`Python3_EXECUTABLE`。源码仍须包含锁定提交；只改变目录不意味着可以任意改变版本。

网络需要代理时：

```powershell
# 地址仅为示例，替换为自己的代理；只在脚本进程内生效。
.\tools\bootstrap.ps1 -Proxy 'http://127.0.0.1:7890'
```

本机实测：GitHub 主站不可直接连接，但 `projects.blender.org` 和 GitHub 的 `codeload.github.com` 可访问。脚本使用官方入口，不依赖非官方镜像。旧 Python 自带 pip 遇到 Windows 系统 HTTPS 代理报 `check_hostname requires server_hostname` 时，明确传入 HTTP CONNECT 代理地址可解决；不要关闭 TLS 证书校验。

依赖全部准备完成后，可在同一台机器上验证离线模式：

```powershell
.\tools\bootstrap.ps1 -Offline
.\tools\build.ps1 -VisualStudio 2022
```

离线模式不运行 pip / aqt 下载，缺少提交、源码、工具、Qt 或尚未物化的 LFS 对象会报错。网络隔离的新机器应提前准备 Python/Git/VS 安装包、PyPI wheel 缓存、固定提交的源码和已物化 Windows 库、Qt/CUDA/OptiX/Jolt 安装目录，再使用上述路径覆盖。Python venv 不可直接跨机器复制；用目标机器 Python 重新创建，再用 `pip install --no-index --find-links <wheel目录>` 安装工具。下载缓存中的归档也可复用。

## 7. 依赖版本与手工恢复

完整 URL、提交、归档 SHA256 以 [tools/dependencies.lock.json](../tools/dependencies.lock.json) 为准，不跟随分支最新版本。

| 依赖 | 固定版本 / 提交 | 获取方式 |
| --- | --- | --- |
| 独立 Cycles | v5.2.0 / `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba` | Git，项目脚本生成补丁树 |
| Blender 参考源码 | v5.2.2 / `d13f752e3b9c4f8c261cda552b1021f8bcc0382c` | Git，只 checkout `intern/cycles`，不构建完整 Blender |
| Windows 库 | `60d6e96b917568278d400a4024c98da0fb777338` | Blender 官方预编译库，Git LFS |
| Jolt Physics | v5.6.0 / `e77f175595e64cb44218cc9d9d56fc365ad0e36a` | 官方源码归档，由主工程自动编译 |
| Qt | 6.10.3，MSVC 2022 x64 | aqtinstall / Qt 官方安装器 |
| CUDA | 12.9.0，nvcc 12.9.41 | NVIDIA 官方 redist：nvcc、cudart、cccl |
| OptiX | 9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d` | NVIDIA 官方头文件归档，无需构建 SDK 示例 |

Windows 库子集含 OpenImageIO、TBB、OpenEXR、Imath、OpenColorIO、epoxy、fmt、zlib、zstd、pugixml、OpenSubdiv 和图像编解码依赖。OpenSubdiv 的 CPU 库用于项目自己的细分功能，即使关闭 Cycles 内置 OpenSubdiv 选项也不能删除它。`nlohmann/json` 头文件已随仓库提供。

### 7.1 Git / Git LFS 手工获取

以下以默认目录为例；已有目录不重复 clone：

```powershell
git clone --no-checkout https://projects.blender.org/blender/cycles.git .deps/sources/cycles
git -C .deps/sources/cycles checkout 3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba

git clone --no-checkout https://projects.blender.org/blender/blender.git .deps/sources/blender
git -C .deps/sources/blender sparse-checkout set intern/cycles
git -C .deps/sources/blender checkout d13f752e3b9c4f8c261cda552b1021f8bcc0382c

# 手工方式使用 Git LFS；Git for Windows 安装器可以安装它。
$env:GIT_LFS_SKIP_SMUDGE = '1'
git clone --no-checkout https://projects.blender.org/blender/lib-windows_x64.git .deps/windows-libs
git -C .deps/windows-libs checkout 60d6e96b917568278d400a4024c98da0fb777338
git -C .deps/windows-libs lfs install --local
git -C .deps/windows-libs lfs pull --include='OpenImageIO/**,tbb/**,epoxy/**,opencolorio/**,openexr/**,imath/**,openjph/**,fmt/**,zlib/**,zstd/**,pugixml/**,aom/**,jpeg/**,png/**,webp/**,openjpeg/**,pystring/**,opensubdiv/**'
Remove-Item Env:GIT_LFS_SKIP_SMUDGE
```

只 clone 库仓库不够：几百字节、以 `version https://git-lfs.github.com/spec/v1` 开头的 `.lib/.dll` 是指针，不是库文件。主工程需要真正的二进制。完整 Blender 依赖从源码重建涉及大量传递依赖，本工程的受支持路径是固定的官方预编译包；不提供未经验证的 vcpkg/Conan 替代组合。若确需重建，应依据固定 Blender 源码的 `build_files/build_environment` 和 [Blender 官方依赖构建说明](https://developer.blender.org/docs/handbook/building_blender/dependencies/) 完成，并将安装布局指向 `DFV_LIB_DIR`，之后重新验证 ABI、画质和性能。

### 7.2 Qt 自动下载不可用时

使用 Qt 官方安装器安装 **Qt 6.10.3 / MSVC 2022 64-bit**，至少包括 Qt Base、Qt SVG、Qt Tools、Qt Translations；确认 `bin/lconvert.exe` 存在，然后传入 `-QtRoot`。Qt Creator 不是构建本工程的必要组件。

需要从源码编译 Qt 时，在 **x64 Native Tools Command Prompt for VS 2022** 中安装好 CMake、Ninja、Python，获取 Qt 6.10.3 官方源码后执行（示例路径请替换；Qt 自身源码路径应短且无空格）：

```bat
mkdir D:\qt-build
cd /d D:\qt-build
D:\qt-src\configure.bat -prefix D:\qt-local\6.10.3 -opensource -confirm-license -shared -debug-and-release -nomake examples -nomake tests -submodules qtbase,qtsvg,qttools,qttranslations
cmake --build . --parallel 4
cmake --install .
```

`-confirm-license` 表示接受对应开源许可，请按实际使用方式选择许可证。随后 `bootstrap.ps1 -QtRoot D:\qt-local\6.10.3`。详见 [Qt 6.10 Windows 源码构建文档](https://doc.qt.io/qt-6.10/windows-building.html)。

### 7.3 CUDA、OptiX、Jolt 手工获取

- CUDA：可从 [CUDA 12.9 下载归档](https://developer.nvidia.com/cuda-12-9-0-download-archive) 安装完整 Toolkit，传入 `-CudaRoot`；或按锁文件下载三份 redist ZIP，核对 SHA256，将各 ZIP 的顶层目录内容合并到同一个 CUDA 根目录。需要 `bin/nvcc.exe`、`include`、`lib/x64`、`nvvm` 等子目录，不能只复制 nvcc.exe。
- OptiX：从锁文件中的 NVIDIA 官方归档取得 9.1 头文件，去掉 ZIP 最外层目录，解压到 `.deps/optix`，保证存在 `include/optix.h`。无需构建示例；使用本身受 NVIDIA SDK 许可约束。
- Jolt：从锁文件中的官方源码归档取得 v5.6.0，去掉最外层目录，解压到 `.deps/sources/jolt`。主工程通过 `add_subdirectory` 自动构建，禁止混用其他编译器/CRT 的预编译 Jolt。

## 8. 启动、验证与迁移运行目录

```powershell
# 基础工具及驱动检查；RequireGpu 会在驱动不符合要求时返回失败
.\tools\check_environment.ps1 -RequireGpu

# 枚举真实设备，确认存在 OPTIX
& .\out\vs2022\Release\CyclesViewportBench.exe --devices

# 使用内置测试场景，不需要 DAZ Studio 或付费资产
& .\out\vs2022\Release\CyclesViewportBench.exe --smoke --device OPTIX --monitor 1 --output artifacts/smoke-optix

# 启动编辑器
& .\out\vs2022\Release\DazFastViewer.exe
```

在编辑器的项目设置中添加本机 DAZ 内容库，或启动时传入 `--file <DUF文件> --content-root <内容库根目录>`。`configs/project.example.json` 的内容库默认留空，不能继续使用原机器的 H 盘和个人用户名路径。读取真实资产需要用户自己提供内容库；构建和默认 CTest 不需要安装 DAZ Studio。

单独运行测试：

```powershell
& .\.deps\python\Scripts\ctest.exe --test-dir build/vs2022 -C Release --output-on-failure
& .\.deps\python\Scripts\python.exe -m unittest discover -s tests -p test_build_tools.py -v
```

CTest 覆盖普通 CPU/Qt 回归，不能代替 GPU 冒烟与画质 / 性能基准。`RendererQualityTest` 等 GPU 专项目标默认不参与构建，需要时显式构建并按相应测试说明运行。没有合格驱动时可以完成构建与普通测试，但不能声称 GPU 渲染验证通过。

部署脚本按实际 CMakeCache 中的 Qt、库目录和配置收集文件，不再引用旧机器环境报告；`build-manifest.json` 记录本次依赖锁、编译配置、源代码组装信息和运行文件 SHA256。Release/RelWithDebInfo 包含应用本地 MSVC CRT，Debug 仅用于开发。

迁移时复制整个 `out/vs2022/Release` 目录，不能只复制 EXE。目标机器需要相同的 CPU 指令集支持、符合要求的 NVIDIA 驱动以及已包含在包中的 GPU 原生架构。Cycles 缓存位于 `%LOCALAPPDATA%/DazFastViewer/cache`，默认编辑器日志位于 Qt 的应用本地数据目录（Windows 通常为 `%LOCALAPPDATA%/DazFastViewer/DazFastViewer/artifacts`），不写入原工程源码目录；`--output` 可显式覆盖日志位置。运行文件迁移验证不等于完成商业发行审计；原有第三方许可证和对应源码提供要求继续适用。

## 9. 故障排查

| 现象 | 处理 |
| --- | --- |
| 找不到 VS 或 MSVC | Installer 中安装 C++ 桌面工作负载、v143、Windows SDK；明确 `-VisualStudio` |
| VS 2026 找不到 v143 | 在 VS 2026 单个组件中安装 v143；不强行改用 v145 绕过 CUDA 检查 |
| `Unknown generator Visual Studio 18 2026` | 使用工程准备的 CMake 4.2.3；PATH 中旧 CMake 不能生成 VS 2026 工程 |
| `generator does not match` | 使用另一构建目录；不要复用不同 VS 的 CMakeCache |
| 找不到 Python / 打开 Microsoft Store | 安装真实 Python，脚本会跳过 WindowsApps 占位程序 |
| `prepare_cycles.py` 报 WinError 2 / 多个项目报 MSB8066 | 使用本版 CMake 配置和构建脚本重新生成工程，自动发现 VS 附带 Git；不要混用旧分支的 SLN/CMakeCache |
| 启动报“缺少第二屏” | 使用本版重新生成的 EXE；当前编辑器已支持主屏和单屏启动 |
| GitHub 超时、pip 代理错误 | 使用 `-Proxy`，检查官方源可访问性；完整错误会保留，不静默跳过依赖 |
| `.lib` 无效 / DLL 是 LFS 指针 | 重新运行 bootstrap，或按手工 LFS 命令物化对应包 |
| Qt 平台插件缺失 | 运行 stage_runtime.py；保留 `platforms/qwindows.dll`，Debug 使用对应调试插件 |
| windeployqt 在生成中文翻译时 `Process failed to start` | 安装 Qt Tools，确认 `bin/lconvert.exe`；不要删除翻译部署步骤绕过 |
| 首次 CTest 缺 DLL | 重新配置当前 CMake；测试环境按本次库路径生成，不需要借用旧 out |
| `cl.exe` 耗尽内存 / C1060 | 用 `-Jobs 2`；脚本限制 MSVC 文件级并发，不修改优化级别 |
| 找不到 OptiX 设备 / `UNSUPPORTED_ABI_VERSION` | 更新到 R590+ 驱动，确认使用 NVIDIA GPU；不回退 CPU |
| 内核缺失 / 运行时要求 nvcc | 为目标 GPU 的 `sm_XX` 重新构建并暂存，复制完整 `lib` 目录 |
| 暂存 PermissionError | 关闭正在使用目标目录 DLL 的程序，再运行；脚本不强制结束用户进程 |
| CPU 上出现 illegal instruction | 检查 AVX2/FMA 等最低要求；不通过关闭 SIMD 默认牺牲性能 |
| 真实场景缺贴图 / DSF | 在项目设置中配置本机内容库，不是重新安装构建依赖 |

2026-09-30 的多架构 Release、32 项测试、单屏/缩放及迁移运行结果见 [构建验证记录](build_validation_20260929_cn.md#2026-09-30windowsnvidia-迁移修复与多架构-release)。当前本机驱动仍为 556.12，尚未完成 OptiX 渲染验证。
