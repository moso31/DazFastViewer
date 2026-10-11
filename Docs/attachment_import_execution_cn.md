# 自动穿戴与角色附件挂接

本轮将兼容服装、骨架头发、头部饰品、已保存曲线头发和 GeoGraft 接入当前角色的资源导入流程。原始内容库保持只读。

## 使用方式

1. 加载角色，在场景树或视口选择角色、角色骨骼或它已绑定的附件。
2. 在内容浏览器双击服装／头发／角色附件，或通过“文件 → 添加 / 应用 DUF…”选择资源。场景只有一个可用角色且没有活动选择时自动使用该角色；多角色无选择时提示先选择目标。
3. 后续修改角色的体型、姿势和位置，附件使用已有 Morph、ERC、蒙皮、碰撞及接缝求值。
4. 选择附件，通过“穿戴与附件”菜单或场景树右键的“绑定到角色 / 更换目标…”和“解除挂接”调整关系。同次导入的整套附件一起操作。

解除挂接保留附件自身参数，并恢复独立的基准变换；不会烘焙当前角色形态到附件。GeoGraft 解除／移除后重新计算宿主遮盖面；只关闭 Visible 仍保留绑定及遮盖语义。

## 实现

- `LoadOptions::defer_selection` 只为穿戴导入延迟解析 `name://@selection:`、`id://@selection:` 及骨骼目标。普通完整场景仍要求引用完整有效。
- 追加时保留资产内部引用与原始来源，使用目标角色的稳定实例身份解析外部引用。同步更新 parent、Fit To、Collision Item 和 Rigid Follow。
- 保留资产的 `preferred_base`、`auto_fit_base`、`extended_bases`，根据声明判断兼容性。G8.1 声明的 G8 兼容基型可以使用；没有可靠声明的 Fit To 不靠顶点数或显示名称猜测。
- 在基准空间建立绑定，再由当前快照应用人物的移动、Morph 和姿势。新骨骼附件以静止骨架为绑定参考，避免角色载入时已有姿势而漏掉该姿势。
- `Document::attachments` 保存同次导入的关系与基准矩阵，用于整套解除和更换目标。删除单件后同步清理记录，避免后续操作引用已删除对象。
- 在副本完成依赖和拓扑校验后提交文档；导入、解除和更换目标都在后台运行，失败时保留原场景。
- GeoGraft 校验基础拓扑，并拒绝向同一宿主区域重复挂接其他 GeoGraft。接缝约束、材质及细分继续使用已有实现。
- 支持发丝适配头皮、头皮在场景树中却属于发丝子节点的资产。解除宿主后，将原来直接 Fit To 宿主的 Figure 作为独立根，避免反向依赖循环；重新绑定恢复原有组织关系。

## HD Nipples 产品入口

内容浏览器识别本地产品的 `People/Genesis 8 Female/Anatomy/3feetwolf/HD Nipples - 2.0/HD Nipples for G8F - 2.0.dse`，通过隔离的入口适配器加载同产品提供的基础 DUF。入口适配器不读取、不执行加密脚本，普通 `.dse` 仍明确拒绝。

基础 GeoGraft 挂接成功后，可以在 `HD Textures` 目录使用下面两个纹理入口。碰撞优化、dForce 和其他配套脚本未包含在本次适配中，产品名中的 HD 也不代表新增了 DAZ HD Morph 求值。

## 产品材质功能（2026-10-11）

在内容浏览器双击以下原有入口；选择角色或其已绑定附件均可。适配不解密或运行 DSE，只识别明确列出的产品路径。

- `Create HD Nipples Textures for Current Character`：选择三种颜色、四档乳晕大小、三档腺体，预览产品缩略图后开始。读取当前皮肤通道，从原始产品 UV 表重新生成任务数据，调用已安装的 `preprocessing.exe`。生成左右各七张 4K 主贴图，并为当前皮肤的透明、双高光、粗糙度等附加贴图重采样局部 UV。
- `Apply HD Textures`：优先匹配本客户端为当前皮肤生成的完整纹理组；没有时，查找产品中与当前底图文件名相同的旧纹理目录。旧结果先复制到客户端独立目录，再补齐附加通道。复制原皮参数，并将附件左右局部切换到 HD UV；其他皮肤表面使用对应的身体或手臂材质。
- `GoldenPalace_Material Copy_v2-G8F 8_1`（也包含带 `3-` 前缀的入口）：复制当前身体材质至已绑定的 GoldenPalace v2 本体；已有覆盖层保留专用纹理、透明遮罩与 UV，只同步肤色和散射参数。普通身体覆盖表面与其他角色不受影响。

应用材质支持撤销／重做及 DUFEX 保存重载。烘焙文件位于客户端本地数据目录 `hd-textures` 中，每次创建独立任务目录；取消或失败不发布纹理索引、不修改场景，任务日志为 `process.log`。原产品 DAT 和已有纹理保持只读，取消会结束整个烘焙进程树。

当前支持 Genesis 8 / 8.1 Female 的基础 UV。自定义 UV、皮肤平铺／偏移和 LIE 分层输入会给出明确提示。产品旧缓存只能依据底图文件名匹配，不能证明其他通道与当前皮肤一致；需要准确匹配时先创建新的纹理组。独立 Geometry Shell 穿戴预设的 `@selection` 宿主导入仍受原加载器限制，材质复制可处理已随完整场景加载的覆盖层，不负责创建覆盖层。

## 验证

新增 `AttachmentWorkflowTest`，纳入 `tools/build.ps1`。覆盖：

- 选择引用发现与延迟解析、参数公式地址不误判为穿戴目标。
- G8 → 声明兼容的 G8.1、跨性别拒绝、无目标拒绝。
- 已移动和已保存姿势的角色加载附件、Morph 与姿势继续跟随。
- 骨骼发饰、缺少骨骼场景实例时生成唯一挂接节点、表面刚性跟随。
- 多角色隔离、整套更换目标、解除后恢复人体遮盖面、重叠 GeoGraft 拒绝。
- 刷新参数、删除单件、已有场景的发饰换绑、头发与头皮反向层级。
- HD Nipples 基础入口映射以及任意脚本拒绝。

真实资源验证通过无窗口专项入口运行，检查加载、人物平移、解除及重新绑定：

```powershell
build/vs2022/bin/Release/AttachmentWorkflowTest.exe --actual <角色DUF> <附件DUF> <内容库1> <内容库2>
```

副屏编辑器专项入口经过实际资源打开流程，先平移角色并转头，再添加、解除和重新绑定，等待每一步的渲染版本完成后检查并截图：

```powershell
out/vs2022/Release/DazFastViewer.exe --self-test --file <角色DUF> --wear-test <附件DUF或受支持的产品入口> --output artifacts/attachment-ui
```

验证产物保存在本地 `artifacts/attachment-*.log`、`artifacts/attachment-ui-top/` 等目录，不提交商业资产或截图。

最终构建的 16 项工程测试全部通过，日志为 `artifacts/attachment-final-build.log`，程序已部署到 历史验证的可执行文件。以下真实资源均完成加载、移动宿主、解除和重新绑定：

| 资源 | 宿主 | 结果与日志 |
| --- | --- | --- |
| MK Sleepwear Top | Genesis 8.1 Female | PASS，`attachment-mk-top.log` |
| Genesis 8 Male Genitalia | Genesis 8 Male | PASS，`attachment-genitalia.log` |
| HD Nipples for G8F - 2.0 基础 DUF | Genesis 8.1 Female | PASS，`attachment-hd-nipples.log` |
| KCH Butterfly Hair Deco Wearable | Genesis 8.1 Female | PASS，两个附件，`attachment-hair-deco.log` |
| FE Low Ponytail Base Hair | Genesis 8.1 Female | PASS，四个对象，`attachment-hair-curves-final.log` |

曲线头发首次验证发现解除挂接时头发与头皮形成依赖循环，修复后重新通过；保留的旧失败日志不代表最终状态。

副屏编辑器使用最终发布程序验证了 MK 上衣和 HD Nipples 的 `.dse` 产品入口，两者均完成添加、解除和重新绑定，`editor-check.json` 的状态为 PASS。后者最终 116 对基础网格接缝最大误差为 0 米。报告及截图分别位于 `artifacts/attachment-ui-top/` 和 `artifacts/attachment-ui-hd-nipples/`。这些检查覆盖操作链路和几何一致性，不代表与 DAZ Studio 渲染的逐像素等价。

## 边界

- 没有新增跨代／跨性别 AutoFit、无权重网格自动绑定、dForce 或 DAZ HD Morph。
- 曲线头发使用资产中已保存的曲线与蒙皮，额外渲染发丝生成器仍未实现。
- 高跟鞋所需的脚部姿势、依赖脚本的材质生成和复杂产品操作不由设置 Fit To 自动替代。
- 基础网格接缝位置对齐，不代表细分边界、法线和跨材质纹理已完全等价于 DAZ Studio；碰撞继续属于原生近似。
- 本轮没有新增场景保存格式或 Undo 系统，绑定关系保存在当前编辑文档中。
