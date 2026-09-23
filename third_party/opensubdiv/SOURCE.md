# OpenSubdiv 依赖记录

- Repository：https://github.com/PixarAnimationStudios/OpenSubdiv
- Revision：v3_7_0；本地头文件 `OPENSUBDIV_VERSION_NUMBER=30700`。
- Binary：Blender 官方 `lib-windows_x64`，commit `60d6e96b917568278d400a4024c98da0fb777338`。
- File / Module：`opensubdiv/far`、`opensubdiv/sdc`，`osdCPU.lib` / `osdCPU_d.lib`。
- License：修改版 Apache License 2.0（以附带 `LICENSE.txt` 为准）。
- Copyright：Pixar、DreamWorks Animation LLC 等；见 `NOTICE.txt` 及原始头文件。
- Planned Use：从 DAZ 原始多边形构造均匀细分模板，插值形变后的顶点和 UV。
- 是否复制实现源码：否；调用已安装头文件与静态库。
- 是否修改第三方源码：否。
- 是否进入发布二进制：是，静态链接 CPU 库；许可和 NOTICE 随 `out/licenses/opensubdiv` 分发。

库的 LFS 物化命令：`git -C .research/windows-libs-metadata lfs pull --include="opensubdiv/lib/osdCPU.lib,opensubdiv/lib/osdCPU_d.lib" --exclude=""`。
