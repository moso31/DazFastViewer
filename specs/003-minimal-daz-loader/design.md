# 实现与兼容性边界

## 加载与解析

[`loader.cpp`](../../src/daz/loader.cpp) 使用 nlohmann/json 3.12.0（MIT）和既有 zlib。新增依赖的来源、SHA256 和许可证在 [`third_party/nlohmann`](../../third_party/nlohmann/SOURCE.md)；暂存运行文件时复制许可到 `out/licenses/nlohmann-json`。

Repository 缓存文档并记录实际读取的依赖。URI 分离路径与 fragment 后做 UTF-8 百分号解码；以 `/` 开头的路径相对内容库根目录。相对路径优先相对引用文档查找，再查内容库；不支持网络 URI。解析后的引用不得以 `..` 越过根目录。

文档解压上限 512 MiB；JSON 类型、引用、数组 count、材质槽、顶点和 UV 索引均校验。当前读取压缩输入后才检查输入长度，该上限不是通用不可信输入的流式内存安全保证。

Node 从 `node_library` 合并实例覆盖；按 channel ID 合并变换通道。Geometry 按 URI 与各材质槽 UV Set 组合缓存。四边形拆成两个三角形；UV 接缝使用 `(polygon_index, geometry_vertex_index)` 查找，最后写为三角形的三个独立 UV。

## 静态变换

已实现常用 pivot / orientation / rotation-order / scale / translation 的静态矩阵组合，随后进行坐标基变换。父节点循环或缺失会报错。

当前没有完整实现 DAZ 的旋转 / 缩放分离和 `inherits_scale` 补偿；没有骨骼蒙皮求值。用户两份输入的几何根节点为静态默认变换，不能据此证明任意非均匀缩放层次正确。骨骼带非零平移 / 旋转、未实现的缩放补偿会记录诊断。骨骼缩放、约束、Formula / ERC 等仍不在覆盖范围内。

## 材质

先合并 material_library 与实例覆盖，再生成简单 PBR 材质。支持 diffuse 颜色与贴图、Glossy Roughness、Metallic Weight、Cutout Opacity、Refraction Weight / Index、Normal Map。金属度 / 折射权重当前仅映射标量，不支持对应贴图。贴图按路径与颜色空间去重。

未映射通道逐项写到 `materials.unmapped_channels`，包括用户角色的 SSS、Translucency、Bump、Top Coat 等。因此当前肤质、眼睛与 Iray / DAZ 最终效果存在差异。多个 UV Set 可按材质槽选取；没有实现每张贴图各自的 UV 变换、UDIM 资源组、LIE 或着色器插件。单张贴图在 UV 整数块外按 repeat 采样，不宣称完整 UDIM 支持。

## 明确未实现

- SubD / HD、Morph、Skinning、Pose、ERC / JCM、约束、GeoGraft 合并。
- DAZ 相机、灯光、渲染设置、动画与完整节点可见性语义；应用自行取景、添加预览环境。
- 通用 Asset Preset 应用、完整相对引用继承来源追踪、任意复杂内容库组合。
- Blender / DAZ Reference 的数值和图像对照。

报告中的 `fully_supported` 只表示当前解析器没有发出诊断，不是对整个 DSON 规范的合规证明。

## 官方格式依据

- [Asset Addressing](https://docs.daz3d.com/public/dson_spec/format_description/asset_addressing/start)
- [Geometry](https://docs.daz3d.com/public/dson_spec/object_definitions/geometry/start)
- [Polygon](https://docs.daz3d.com/public/dson_spec/object_definitions/polygon/start)
- [UV Set](https://docs.daz3d.com/public/dson_spec/object_definitions/uv_set/start)
- [Node](https://docs.daz3d.com/public/dson_spec/object_definitions/node/start)

只依据格式说明和用户本地文件独立实现；未复制 Diffeomorphic 或 DAZ Studio 源码。
