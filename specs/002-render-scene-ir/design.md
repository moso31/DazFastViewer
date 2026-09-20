# 设计与实现边界

数据流：`DSON Loader / Bench Fixtures → ir::Scene → CyclesAdapter → Cycles Session → Display`。

## 数据与资源

[`scene.h`](../../src/render_ir/scene.h) 定义纯 C++ 数据结构和校验接口。字符串 ID 用于追溯，容器索引用于本次加载内的引用；尚未设计跨场景加载、删除后复用的句柄系统。

[`adapter.cpp`](../../src/cycles/adapter.cpp) 创建 Cycles Shader、Mesh、Object 和 AreaLight。以 `(mesh index, material bindings)` 缓存 Cycles 网格；同一几何、相同材质绑定的实例共享网格，不同绑定会建立独立的后端网格，避免错误改写另一实例的材质。

三角形保存材质槽索引，UV 按三角形角点写入 `ATTR_STD_UV / UVMap`。ImageTexture 显式连接 UVMap；法线贴图使用相同 UV 名称，由 Cycles 生成切线。

贴图以绝对路径和颜色语义引用。当前渲染空间固定为线性 Rec.709，颜色贴图使用 `u_colorspace_scene_linear_srgb`，数据贴图使用 `u_colorspace_data`。前者在当前源码中保留 sRGB 字节，在 SVM 采样时解码一次。不要改回 `u_colorspace_srgb`：本轮确认该版本的此路径在 CPU 转换时执行 sRGB 编码，导致输入贴图过亮、肤色偏灰。对应证据位于生成源码 `util/image_metadata.cpp`、`util/colorspace.cpp`、`kernel/svm/image.h`，以及 Spec 003 执行报告。

## 增量与线程

`Delta` 当前只有相机和指定材质替换。调用者持有 Cycles Scene 锁；相机修改后按已有调度执行 Session reset。相机数据不包含 Mesh、Shader 或 Texture 创建路径。所有 Delta 参数先校验再修改；不保证显存不足等运行时故障下的完整事务回滚。

AdapterStats 记录后端网格创建数量、相机更新和材质更新。它是主机对象创建证据，不等于 GPU 字节上传计数。实际角色相机验证还检查 `scene_data_dirty` 只出现于初次载入。

## 预览环境

无 DAZ 相机和光照求值时，由应用自动取景并添加地板、环境与三盏面积灯。面积灯功率随包围盒尺度平方变化，使厘米级道具与米级人物曝光相近。预览环境不会写回 DUF。

窗口创建前枚举显示器，主屏排第一，其余按设备名排序，默认选择第二块。窗口和隐藏 GL 窗口从创建开始位于目标屏，显示时不主动抢焦点。当前测试布局是右侧 1920×1080、DPI 96；其他 DPI / 多于两屏的布局仍需专门验证。
