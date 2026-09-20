# 设计

`daz` 层发现文件并解析为纯 C++ Morph 元数据和稀疏差值；`runtime` 层管理对象属性、Active Set 与独立基础顶点；`Render IR Delta` 表达顶点和实例矩阵变化；`CyclesAdapter` 负责原有网格的更新、法线失效及 BVH 标记。Qt 不直接修改网格或 Cycles。

编辑器为独立 `DazFastViewer.exe`，已有 `CyclesViewportBench.exe` 保留无 UI 的测试入口。Qt MainWindow / DockWidget / Model-View 形成菜单、Explorer、SceneHierarchy、Properties 的外壳。首轮复用固定分辨率原生 WGL 视口；后续独立增加渲染分辨率与动态窗口尺寸管理，不把 QWidget 重绘当作渲染提交。

场景解析与 Morph 扫描在后台执行。UI 发送带文档世代号的属性快照，工作线程合并短时间内的重复编辑；场景切换时旧文档命令不得作用于新对象。场景节点和属性用稳定 ID，行号仅作视图索引。

Genesis 8.1 继承 Genesis 8 Morph 需要明确族谱和拓扑验证；空覆盖文件保留屏蔽语义，不仅凭顶点数匹配。服装 `auto_follow` 元数据在此阶段保留，实际求值交给后续 FitTo 依赖图。

Qt 使用动态链接，记录所用模块及许可证；不引入额外 GPL-only 模块。原有 Runtime 数据结构不依赖 Qt，可供后续命令行、测试与其他 UI 使用。

项目配置独立存储为版本化 JSON，`ProjectSettings` 使用 QSaveFile 原子写入。路径去重不改变用户优先级；CLI 根目录只影响本次有效配置，用户在菜单保存时才持久化。保存后后台重新加载，按对象 / 参数 ID 恢复兼容值；不将参数行号作为身份。

参数目录先按各根目录优先级合并同代文件，再应用 G8.1 覆盖。参数元数据与稀疏差值分离：纯控制器、Alias、HD-only 和未验证拓扑项也进入目录，只有完整支持的直接 Morph 进入当前求值路径。外部引用递归发现使用已访问集合，查找期间缓存路径解析；报告分开统计节点依赖、参数依赖、缺失引用、被覆盖引用和解析错误。

参数面板采用分组树和单一编辑区，避免为数千项创建成千上万个常驻滑块。原始 `group` 和所属节点保留，搜索支持名称、ID、分组与所属节点；别名保留为单独的入口，当前只读，后续统一到目标通道身份。详细核查见 [多库与参数发现修复报告](content-libraries-report.md)。
