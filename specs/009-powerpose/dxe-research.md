# PowerPose DXE 解析研究

日期：2026-09-25。结论：**本机的 G8／G8.1 PowerPose DXE 已能用独立 Python 程序解码。** 不需要启动 DAZ Studio，也不修改其插件、模板或场景。

## 找到的方法

公开检索未找到可直接采用的 DAZ PowerPose DXE 专用解码器，不能据此断言不存在。已找到可复用的 [python-twofish](https://pypi.org/project/twofish/)：它封装 Niels Ferguson 的 Twofish 实现，支持独立的 16 字节加解密块，项目列明 BSD 3-Clause 许可证。本次只将它安装在 `.research/powerpose-python`，未加入生产依赖。

本机插件的只读静态分析显示，PowerPose 读取 DXE 后调用分块解码，将末尾零字节去掉，再交给 XML 解析器。识别结果为 **Twofish-256、ECB 独立块、零填充、UTF-8 XML**。标准库解密后得到合法的 `template_suite`／`template_file`，并能重新编码为逐字节相同的原 DXE。

实现见 [decode_templates.py](../../tools/powerpose/decode_templates.py)。脚本根据已验证 DLL 的 SHA-256 确定读取偏移，从本机 DLL 读取 32 字节常量；不把常量内容写进日志或报告。未知 DLL 哈希明确拒绝，不盲用旧版偏移。

已验证插件：`C:/Program Files/DAZ 3D/DAZStudio4/plugins/PowerPose/dzpowerpose.dll`，SHA-256 为 `eca69b6042fbdb76e72abb30dbbb66c09abd7cb49042511ac9c1293be4a7acb9`。静态定位：文件偏移 `0xDD810`，对应 RVA `0xDFA10`。解码入口 RVA `0x4D1F0`，读取 XML 的调用点在 RVA `0x4F9D0` 内，分块入口 RVA `0x61030`；这些仅是当前文件的研究定位，不是跨版本 ABI。

另外检查过本机 DAZ Studio 6 Public Build 插件：简单套用常量相对位置不能解出有效文件，因此**没有**将该版本加入脚本支持列表。已有 Studio 4 插件可解本机 G8.1 模板，不需要继续依赖这一猜测。

Python 3.12 不再提供旧包装器使用的 `imp`；本工具通过 `importlib` 定位 `_twofish` 并调用其 ctypes ABI，没有修改安装包源码。每次执行先进行公开的 256 位 Twofish 已知答案校验。

## 复现

在仓库根目录的 PowerShell 中执行：

```powershell
python -m pip install --target .research/powerpose-python twofish==0.3.0

python tools/powerpose/decode_templates.py `
  'C:/Users/Public/Documents/My DAZ 3D Library/data/DAZ 3D/Genesis 8/Female 8_1/Tools/PowerPose' `
  --plugin 'C:/Program Files/DAZ 3D/DAZStudio4/plugins/PowerPose/dzpowerpose.dll' `
  --output artifacts/009-powerpose/g81

python tools/powerpose/build_review.py `
  --template-dir 'C:/Users/Public/Documents/My DAZ 3D Library/data/DAZ 3D/Genesis 8/Female 8_1/Tools/PowerPose' `
  --figure 'C:/Users/Public/Documents/My DAZ 3D Library/data/DAZ 3D/Genesis 8/Female 8_1/Genesis8_1Female.dsf'
```

Twofish 扩展在本机 Python 3.12／MSVC 下已成功构建。不同机器需要能构建该扩展的编译环境。静态研究所用 capstone 不是解码脚本的执行依赖。

G8 样本源目录为 `H:/G3/data/DAZ 3D/Genesis 8/Female/Tools/PowerPose`，输出位于 `artifacts/009-powerpose/g8`。脚本只选择索引及 Body／Hands／Head，明确不读取 Face.dxe。输出目录不得位于输入目录中。

## 实际验证结果

以下为最初女性模板的研究记录。实施时另对 G8／G8.1 男性各 4 个文件完成解码与回写校验，四套合计 16 个文件；男性的 Head 额外属性点同样排除，Face 文件始终未读取。男性点位比对见 [male-template-check.json](evidence/male-template-check.json)，人工确认后的工具轴与程序验收见 [实施记录](execution-report.md)。

| 模板 | G8 XML 字节 | G8.1 XML 字节 | 节点 | 节点组 | 属性点 | 导航 |
|---|---:|---:|---:|---:|---:|---:|
| Templates | 658 | 662 | — | — | — | — |
| Body | 18525 | 18527 | 26 | 8 | 2 | 2 |
| Hands | 20653 | 20655 | 32 | 12 | 0 | 2 |
| Head | 10091 | 10093 | 4 | 1 | 14 | 2 |

8 个文件都通过 UTF-8／XML 根类型校验与解密→重加密精确比对，处理前后源 SHA-256 一致。报告包含源路径、密文／明文哈希、字节数与点数，见 [G8 证据](../../artifacts/009-powerpose/g8/decode-report.json)和 [G8.1 证据](../../artifacts/009-powerpose/g81/decode-report.json)。副本摘要保存在 [evidence/decode-summary.json](evidence/decode-summary.json)。

另外验证了空文件、截断密文、损坏内容和错误密钥的拒绝路径。Edge 无界面检查遍历了全部 91 个点，验证三页切换、八方向说明呈现、记录恢复及 JSON 导出；JavaScript 语法检查通过。结果见 [review-checks.json](evidence/review-checks.json)。这些检查只证明解码器和核对页工作正常，不是 DAZ 真实拖动验收，也未运行或宣称生产 PowerPose 测试通过。

G8 与 G8.1 三个页面的 XML 差异仅为 `bg_file`；索引中的名称与文件名前缀不同。Head 的 14 个属性点不在本轮五点界面范围内，生成器保留其审计记录，但从清单和核对图过滤。

## 对实现有用的格式信息

| XML 结构 | 实现含义 |
|---|---|
| `width`、`height`、`x`、`y` | 统一模板坐标；本机三页均为 480 × 720 |
| `template_points/tpl_data` | 模板导航，目标由 `tpl_label` 决定 |
| `node_points/node_data` | 一个节点、最多四个鼠标轴绑定 |
| `node_group_points/group_data/group_members/node_data` | 每个成员独立保存四个鼠标轴绑定，不能只用一个共同轴或权重 |
| `property_points/prop_data` | 各鼠标轴可指定不同节点及属性 |
| `lmb_horiz`／`lmb_vert`／`rmb_horiz`／`rmb_vert` | 左键水平／垂直、右键水平／垂直 |
| `*_prop` | 目标属性，可能是轴标记、轴序标记或参数名／标签 |
| `*_sign` | `neg` 为负号；缺省按正号解释，仍须验证鼠标总方向 |
| `*_size` | 该绑定的量程参数；缺省在审阅数据中暂按 300，保留显式／缺省区别 |
| `*_label` | Controls 的显示文本，不应替代真实 `*_prop` |

`xrot` 与 `xRot` 都存在，轴标记应忽略大小写；不能把所有属性名一律改成小写后丢弃原文。节点匹配需覆盖 ID、name、name_aliases、label，以及 Figure 特殊入口；同名节点必须限定在当前角色中。

插件导入了 `getToolXRotControl`／`getToolFirstAxisRotControl` 等接口。官方 [DzNode API](https://docs.daz3d.com/public/software/dazstudio/4/referenceguide/scripting/api_reference/object_index/node_dz)把它们定义为工具应使用的旋转控制，与直接的骨骼旋转读取分开。因此，**解出 XML 不等于所有工具通道已完整适配**：上臂／大腿／前臂的 Bend 与 Twist 拆分仍需动态核对。

程序提取结果保存在 [review-data.json](../../artifacts/009-powerpose/review-data.json)，包含原始字面值、每方向绑定、基础 DSF 通道范围、锁定状态及候选工具代理；可作为下一阶段配置格式的输入参考。

这已经消除了手工猜坐标、成员、方向标记、右键缺省绑定和组速度的主要误差来源。剩余人工验证集中在真实鼠标效果与 DAZ 工具语义，而不是重新抄写 85 个点。
