# 尚不能有效调整的 Morph：分类汇总与完整清单

更新：2026-09-21。仅整理 `artifacts/spec007-compatibility/final` 的已完成验证结果，没有重新扫描内容库、运行 demo 或性能测试。

- [Genesis 8 Female 完整清单：826 条](remaining-morphs-g8.md)
- [Genesis 8.1 Female 完整清单：2,028 条](remaining-morphs-g81.md)
- [两代完整 CSV：2,854 条，可用 Excel 筛选](remaining-morphs.csv)

CSV 每行包含名称、所属节点、DAZ 分组、显示状态、类型、源文件、内部 ID、完整原因、未解析依赖及取样结果。同名但不同节点 / 文件的参数分别列出；两代角色存在大量共用资产，合计不代表不同资源的去重数量。

| 原因分类 | G8 条目数 | G8.1 条目数 |
| --- | --- | --- |
| 公式引用缺失、被覆盖或属性不支持 | 160 | 1247 |
| 别名目标未解析 | 6 | 6 |
| 下游参数受阻 | 9 | 12 |
| 没有有效输出 | 47 | 69 |
| 骨骼属性别名尚未支持编辑 | 16 | 16 |
| 目标顶点数不匹配 | 14 | 8 |
| HD 尚未支持 | 310 | 414 |
| 资产明确锁定 | 193 | 193 |
| 禁止手动编辑小计 | 755 | 1965 |
| 单独取样无明显形变（待组合验证） | 71 | 63 |
| 本清单总计 | 826 | 2028 |

## 查找时的解释

- 锁定项可能由 ERC / JCM 自动驱动，只是不应手动修改；保留锁定语义，不计作修复故障。
- “公式引用问题”并不全是缺文件，也包括错误地址、G8.1 空覆盖及属性支持限制。
- “无明显形变”仅来自新增可编辑项的独立取样：输入 0.5 / 1，按自身范围约束，最大位移低于 1e-6 米。尚未逐项证明配套体型、姿势或开关能触发，不能认定永久无效。
- 当前目录之外的未安装 Morph 不在清单内。已发现但读取失败的文件见各角色附录。
- Ren Yao、BGM Ava、BGM Big Girl Base、AW - Yuki Teen FBM、Mariko 和 L All Nails Length 已验证可调整，不再列为受阻项。

## G8 中便于识别的例子

- 引用受阻：Amber Smile 01、Heidi Kiss、Heidi Tongue Out 02。
- 别名目标受阻：HS Lurys Unhappy；Z FI 01 Terror G8F、02 Fright、03 Distress、04 Panic、05 Dread 对应条目。
- 下游受阻：Liya、Paisley、Shuang、LY Sterling、QX Ferin、Neko HD Details、Liloo HD Details、GamerGirl Expression 07、Skin Crease Controller。
- 无有效输出：BGM Heavy Clothing Smoother、Body Shapes - Children Clothing Helper、AG Jenna Lashes、Alice Eyelashes 等。
- 骨骼别名：不同肩、前臂、腕和大腿节点上的 Twist、Bend、Front-Back 等。
- HD 受阻：Naomi HD Details、Naomi Head、Alice Head、Nyao Body 等；名称不一定含 HD。
- 单独取样无明显形变：MCMArinaEyesClosedL / R、MCMMeiAsakuraEyesClosedL / R、Thigh bend Both+ / Both- 等。

## 读取失败的补充清单

`eJCMNekoEyesClosedL.dsf`、`eJCMNekoEyesClosedR.dsf`：声明的差值 count 与实际差值条目数量不一致。两个角色均引用 `H:/G3/data/DAZ 3D/Genesis 8/Female/Morphs/Raiya/Neko/` 中这两个源文件；不计入 755 / 1,965 个禁用条目。

## 证据快照

以下哈希用于确认清单对应的目录和取样报告版本。完整修复过程见 [兼容性报告](compatibility-report.md)。

| 证据文件 | SHA-256 |
| --- | --- |
| artifacts\spec007-compatibility\final\g8\morph-catalog.json | e75e1fb89b27ecd8546acf46c8f5461a3efbe092eb2043a78640a62055dd3ad7 |
| artifacts\spec007-compatibility\final\g8\regression-report.json | 40e6e4202184f6887ab10e4298f718ae5d07faaec23bd29defa370c478859a36 |
| artifacts\spec007-compatibility\final\g81\morph-catalog.json | 6601538744a9ba2386b93778aeec52526c996147475d166510857f062446c240 |
| artifacts\spec007-compatibility\final\g81\regression-report.json | 50992294c02bcfdf824b14ec70a8e8f5272ebaeef11a51fe4d84b4991ef509d6 |
