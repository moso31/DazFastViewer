# 第三方源码使用记录

日期：2026-09-20。当前已生成本地开发二进制；未完成发行包的完整传递依赖审计。

## 实际实施更新

- 独立 Cycles v5.2.0 为构建基础；35 个 Blender 5.2.2 文件按已有三方差异清单接入，逐文件 SPDX 为 Apache-2.0 或 BSD-3-Clause，保留版权头及 SHA256。详见 `.deps/cycles/dfv-source-manifest.json`。
- 已知 Blender guardedalloc 与 GPL sky 路径未使用。独立支持库中采用带 Apache / BSD / MIT 声明的实际文件，不能将 Blender 同名文件的来源混用。
- 更正早期筛查：独立仓库的 `src/cmake/dependency_targets.cmake` **存在 GPL-2.0-or-later 声明**，早期搜索遗漏了 `.cmake` 文件。构建入口已改用项目原创 `cmake/dependency_targets.cmake`，不 include 该文件。不把整棵独立仓库笼统标成 Apache；外部构建脚本与最终链接代码应分别审计。
- 用户提供的 CUDA 12.9 / OptiX 9.1 已实际用于本机编译；其 NVIDIA 协议及 SDK 头文件不因此转为开源许可。未打包 Toolkit、OptiX SDK 或 NVIDIA 驱动。
- 实际 PE 导入闭包已记录于 `evidence/runtime-imports.json`，包含 OpenImageIO、OpenColorIO、TBB、epoxy、OpenEXR、Imath、OpenJPH、AOM 等。该闭包不覆盖静态或动态加载依赖；完整 LICENSE / NOTICE 仍是发行前待办。
- zstd 采用可选 BSD 许可路径；不链接 Windows pthreads 库。关闭 OSL、USD、OpenVDB、Embree、OIDN 等本阶段未用能力。
- PresentMon 2.5.1（Intel，MIT）仅作为外部观测工具下载至 `.research/presentmon`，未链接或打包；LICENSE.txt 保留。尝试采集因当前账户 ETW 权限不足失败。

以下保留实施前的逐项来源研究记录，其中“当前未复制、尚未构建”等表述是历史状态，以本节和执行报告为当前状态。

## Blender 内 Cycles：算法参考候选

- Repository：`https://github.com/blender/blender.git`
- Revision：当前 `b2e052b7172ac80708d1c5205dfc155365ce5d4f`；候选基线 v5.2.2 / `d13f752e3b9c4f8c261cda552b1021f8bcc0382c`。
- File or Module：`intern/cycles/{device,session,scene,integrator,kernel,app,util,...}`；最终采用文件待逐项列出。
- License：已读文件多数为 Apache-2.0；例如 `kernel/closure/bsdf_microfacet.h` 为 BSD-3-Clause。仓库根 COPYING 为 GPL，不能把整个仓库统一标为 Apache。
- Copyright：Blender Foundation、Blender Authors、NVIDIA Corporation 等，按实际文件保留。
- Planned Use：研究生命周期、相机更新和设备代码；候选渲染算法基线。
- 是否复制源码：当前未纳入产品；未来只采用审核过的文件并保留来源和版权。
- 是否修改：可能有集成与观测补丁，尚未实施。
- 是否进入发布二进制：最终选用部分计划进入；当前无制品。

## 不采用的 Blender 混合路径

同一 v5.2.2 中发现：

| 文件/模块 | 文件声明 | 依赖证据 |
|---|---|---|
| `intern/guardedalloc/MEM_guardedalloc.h` | GPL-2.0-or-later；NaN Holding BV | cycles/cmake/macros.cmake 在非独立仓库模式加入 bf_intern_guardedalloc |
| `intern/guardedalloc/intern/mallocn.cc` | GPL-2.0-or-later；Blender Authors | 上述库的实现源码 |
| `intern/sky/include/sky_nishita.h` | GPL-2.0-or-later；Blender Authors | cycles/scene/shader_nodes.cpp 包含该头 |
| `intern/sky/source/sky_math.h` | GPL-2.0-or-later | 天空模型内部依赖 |
| Blender 根构建与部分 CMake | GPL-2.0-or-later | 与实际运行时链接代码分开记录，不把构建脚本标签直接当作所有输出的标签 |

当前仅研究，不复制、不修改、不纳入发布。若以后选择复用此路径，必须按总体规范第 5 节先提交具体清单并取得确认。用户提供源码目录与允许阅读不等于批准任意许可证代码进入闭源核心。

## 独立 Cycles：构建与支持库候选

- Repository：`https://github.com/blender/cycles.git`
- Revision：v5.2.0 / `3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba`。
- File or Module：根构建、`src/`、`third_party/{cuew,sky}`；禁用不需要的后端。
- License：根 LICENSE 为 Apache-2.0；`third_party/cuew/include/cuew.h` 为 Apache-2.0；独立版 `sky_nishita.h`、`sky_math.h` 和 sky CMake 为 BSD-3-Clause；sky 其余文件包含 Apache-2.0 / MIT / BSD 声明，需逐项保留。
- Copyright：Blender Foundation（例如 cuew 2011–2014）、Blender Authors（例如 sky_nishita 2020–2025），以及各文件贡献者。
- Planned Use：提供独立构建及明确标注宽松许可证的支持库；不链接 Blender guardedalloc。
- 是否复制源码：目前仅完整研究 checkout；候选构建时按清单引用或组装。
- 是否修改：拟进行构建接入、5.2.2 对齐和 profiling 补丁；尚未修改。
- 是否进入发布二进制：选中 Cycles 和支持库计划进入；当前无制品。

对研究树 src / third_party 的 C/C++/CMake 做过 GPL SPDX 搜索，未找到匹配；这只是一项筛查，不能替代完整 LICENSE / NOTICE 审计。不得把这里的 BSD 文件声明自动应用到 Blender 仓库中的同名 GPL 文件。

## Windows 预编译依赖

- Repository：`https://projects.blender.org/blender/lib-windows_x64.git`
- Revision：`60d6e96b917568278d400a4024c98da0fb777338`。
- Planned Use：构建 Cycles；发布时仅打包实际需要的库和许可文件。
- 是否复制源码：研究 checkout 包含头文件与元数据；未加入产品核心。
- 是否修改：否。
- 是否进入发布二进制：选中部分计划链接/随包，当前未完成清单，不视为批准整包发布。

| 模块 | 版本（deps.md） | 已读许可证据 | Copyright |
|---|---|---|---|
| OpenImageIO | v3.1.13.1 | oiioversion.h：Apache-2.0 | OpenImageIO contributors |
| TBB | v2022.3.0 | oneapi/tbb.h：Apache-2.0 | Intel 2005–2025 |
| libepoxy | 1.5.10 | epoxy/gl.h：MIT 文本 | Intel 2013 |
| OpenEXR | 3.4.10 | ImfVersion.h：BSD-3-Clause | OpenEXR contributors |
| OpenColorIO | 2.5.0 | OpenColorIO.h：BSD-3-Clause | OpenColorIO contributors |
| zlib | 1.3.1 | zlib.h：zlib 许可文本 | Jean-loup Gailly / Mark Adler |
| pugixml | 1.10 | pugixml.hpp：MIT | Arseny Kapoulkine；原始 pugxml Kristen Wegner |
| zstd | 1.5.7 | zstd.h：BSD 或 GPLv2 可选，候选选择 BSD 分支 | Meta Platforms |

这张表只记录已读头文件。库中插件、静态传递依赖、随包 DLL 的完整 NOTICE 仍待审计；遇未知或混合许可不直接链接/发布。zstd 的替代许可选择也需在最终清单写清，不能误标为必须 GPL 或忽略 BSD 的通知义务。

## NVIDIA 开发组件

2026-09-20 更新：CUDA Toolkit 已安装，`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9\bin\nvcc.exe` 已核实为 12.9.41；实际 Toolkit 许可与再分发文件清单仍待审核。

OptiX 已提供：

- Repository：`https://github.com/NVIDIA/optix-dev.git`。
- Revision：v9.1.0 / `f1f6dd803f3159992d248178f6e09421c6eb8b6d`。
- 本地目录：`D:\Github\optix-dev`。
- File or Module：`include/`，当前读取 `include/optix.h`、`LICENSE.txt` 与 `license_info.txt`。
- License：仓库为 NVIDIA SDK 协议；license_info.txt 指明不同头文件使用 BSD-3 或 NVIDIA Proprietary，`optix.h` 明确为 `LicenseRef-NvidiaProprietary`。实际复用前按总体规范第 5 节记录选用文件及许可范围。
- Copyright：NVIDIA Corporation & Affiliates，例如 optix.h 标注 2009–2024。
- Planned Use：作为 Cycles OptiX 后端的构建头文件，先验证 9.1 与基线的兼容性。
- 是否复制源码：仅使用用户提供的研究 checkout，尚未复制进产品。
- 是否修改：否。
- 是否进入发布二进制：后续可能编译使用头文件代码；当前无制品，打包范围待审核。

来源入口见 [dependencies.md](dependencies.md)。开发工具、头文件、driver 与允许再分发的 runtime 文件需要分别记录；组件已提供不等于实际构建或发布审计通过。

## 最终采用清单需要补齐

逐项记录仓库、tag、commit、文件路径、内容摘要、SPDX/许可文本、Copyright、NOTICE、修改补丁、链接方式、是否打包。链接器输入及运行时 DLL 依赖必须与清单一致。任何未核实项继续保持未完成，不能因能够编译就自动放行。
