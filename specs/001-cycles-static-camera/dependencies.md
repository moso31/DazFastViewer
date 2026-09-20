# 本地依赖与准备方式

## 最新核实（2026-09-20，用户已补齐 NVIDIA 开发组件）

- 研发参考工作树：`D:\Github\blender-5.2.2`；分支 `dazfastviewer-blender-5.2.2`；v5.2.2 / `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`。原 `D:\Github\blender` 保持原状。
- CUDA 根目录：`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9`；已直接运行 `bin\nvcc.exe --version`，输出 release 12.9 / V12.9.41。
- OptiX 根目录：`D:\Github\optix-dev`；实际为 v9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d`，`include/optix.h` 中 `OPTIX_VERSION=90100`。后续构建传入此路径，验证实际 API / driver / kernel 兼容性。
- Blender 工作树源码已就绪，LFS 二进制资产因上游下载 404 暂保留 pointer。Windows 预编译库仍需物化选中的 LFS 文件。

下面保留首轮探测与候选下载方案，CUDA/OptiX 的缺失状态和 8.0 建议已被上述实际版本替代；不需要用户重复获取已提供组件。

## 已核实

- Blender 来源：`D:\Github\blender`，其中存在 v5.2.2 tag；当前工作目录仍为 main / 5.3 alpha。其 index 状态异常，本项目未修改。
- 独立 Cycles 研究 checkout：`.research/cycles-v5.2.0`，commit `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba`。这是独立构建/许可证参考，不是已经验证的 5.2.2 运行时。
- Windows 库研究 checkout：`.research/windows-libs-metadata`，commit `60d6e96b917568278d400a4024c98da0fb777338`。已读取头文件和 `deps.md`；LFS 库文件尚未全部物化，不能直接当作可链接库目录。
- VS 2022 Community 17.14.23；编译器探测实际输出 MSVC 19.44.35222。
- CMake 3.31.6 和 Ninja 1.12.1 位于 VS 内置目录，不在当前 PATH。

```powershell
$vsCMake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $vsCMake --version
```

不要因为 `Get-Command cmake` 无结果就重复安装工具链。

## 仍需具备的开发组件

| 组件 | 本次检查结果 | 建议与来源 |
|---|---|---|
| CUDA Toolkit | PATH 和默认目录均未发现 nvcc | 候选 12.9.x，Windows x64；[NVIDIA 12.9 下载](https://developer.nvidia.com/cuda-12-9-0-download-archive?target_arch=x86_64&target_os=Windows&target_type=exe_network&target_version=11) |
| OptiX SDK / headers | 默认目录未发现 | 当前 v5.2.2 构建要求 ≥8.0.0；先固定 8.0.0 做基线；[官方头文件仓库](https://github.com/NVIDIA/optix-dev)、[SDK 下载](https://developer.nvidia.com/designworks/optix/downloads/legacy) |
| Windows 预编译库 | 缺可用的完整二进制依赖集 | [官方 lib-windows_x64](https://projects.blender.org/blender/lib-windows_x64)，固定下面的 gitlink |

CUDA 候选来自实际源码 `device/cuda/device_impl.cpp` 的版本处理与 [NVIDIA 的 VS 2022 支持文档](https://docs.nvidia.com/cuda/archive/12.9.0/cuda-installation-guide-microsoft-windows/index.html)，最终以 nvcc 编译和 kernel smoke 为准。不要把 nvidia-smi 显示的驱动能力当成 Toolkit 已安装。

如果用户已有 CUDA / OptiX 的自定义安装，只需给出根目录。OptiX SDK 的获取包含 NVIDIA 许可条款；由用户取得并提供安装目录后，记录实际 LICENSE 再使用。本轮没有安装这些组件或替换显卡驱动。

仅获取 OptiX 头文件的候选命令（尚未执行）：

```powershell
git clone --branch v8.0.0 --depth 1 https://github.com/NVIDIA/optix-dev.git D:\Github\optix-dev
```

已通过 `git ls-remote` 核实该标签，commit 为 `f60c1e44f18426f426a2ed948f28515b3cf67b8a`。最终是否只需 headers 要由实际 Cycles 构建验证，不能把它等同于 CUDA Toolkit。

## Windows 库的精确版本

仓库：`https://projects.blender.org/blender/lib-windows_x64.git`。

基线 gitlink：`60d6e96b917568278d400a4024c98da0fb777338`；2026-09-20 查询 `blender-v5.2-release` 也指向该 commit。以后不能仅跟随分支最新值。

已取得元数据，无需用户再次 checkout。准备进入构建时，先审查实际链接子集，再通过 Git LFS 物化所需库；检查 `.lib/.dll` 的内容，防止把 LFS pointer 文本当作二进制。

`deps.md` 报告：OpenImageIO v3.1.13.1、TBB v2022.3.0、OpenEXR 3.4.10、Imath 3.2.2、OpenColorIO 2.5.0、libepoxy 1.5.10、fmt 12.1.0、zlib 1.3.1、zstd 1.5.7。依赖包里还包含很多本 Spec 不需要的组件；实际编译/发布清单必须按链接结果收敛。

## 已做的配置探测

独立 Cycles 源目录：`.research/cycles-v5.2.0`；构建目录：`.research/build-cycles-preflight`。

生成器为 `Visual Studio 17 2022`、`-A x64`；关闭 upstream GUI、Hydra、USD、OSL、Alembic、OpenVDB、NanoVDB、HIP、oneAPI，开启严格选项检查。该探测未添加产品代码，也未构建渲染内核。

结果：编译器 ABI 检测通过，配置阶段找不到 `OpenImageIOConfig.cmake`。日志见 [cmake-preflight.log](evidence/cmake-preflight.log)。缺失依赖未被伪装为成功，不能据此给出 FPS。
