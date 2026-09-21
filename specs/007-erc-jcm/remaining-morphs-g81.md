# Genesis 8.1 Female 尚不能手动有效调整的参数清单

依据：2026-09-21 最终兼容性验证快照。本清单包含控制器、形态和子节点别名，保留隐藏项；同名条目不合并。

共 2028 条：1965 条禁止手动编辑（其中 1053 条默认可见），另有 63 条在新增项独立取样中无明显形变。

“禁止手动编辑”包含资产主动锁定，不能将全部条目都理解为程序故障。独立取样仅覆盖本次新增可编辑集合，不是所有可编辑参数的全范围视觉验收。G8.1 的旧控制器可能受代际覆盖影响。

源文件、内部 ID、完整原因、未解析 URI 和取样数值见 [完整 CSV](remaining-morphs.csv)，可按下表编号定位。

| 分类 | 数量 |
| --- | --- |
| 公式引用缺失、被覆盖或属性不支持 | 1247 |
| 别名目标未解析 | 6 |
| 下游参数受阻 | 12 |
| 没有有效输出 | 69 |
| 骨骼属性别名尚未支持编辑 | 16 |
| 目标顶点数不匹配 | 8 |
| HD 尚未支持 | 414 |
| 资产明确锁定 | 193 |
| 单独取样无明显形变 | 63 |

## 公式引用缺失、被覆盖或属性不支持（1247 条）

需要逐项区分错误地址、代际覆盖及真正缺失的依赖；不能统一视为缺文件。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-0001 | !Aging Body Controller | Genesis8Female | /Full Body/Real World/Aging Morphs | 控制器 | 是 |
| G81-0002 | 01 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0003 | 01 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0004 | 02 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0005 | 02 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0006 | 03 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0007 | 03 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0008 | 04 EJ Midori Expressionq | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0009 | 04 EJ Midori Expressionq | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0010 | 05 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0011 | 05 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0012 | 06 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0013 | 06 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0014 | 07 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0015 | 07 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0016 | 08 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0017 | 08 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0018 | 09 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0019 | 09 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0020 | 10 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G81-0021 | 10 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G81-0022 | AlineEyeFixLeft | geometry | /Hidden/People/Marcius/Aline | 稀疏形态 | 否 |
| G81-0023 | AlineEyeFixRight | geometry | /Hidden/People/Marcius/Aline | 稀疏形态 | 否 |
| G81-0024 | Amber Afraid | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0025 | Amber Afraid | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0026 | Amber Confident | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0027 | Amber Confident | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0028 | Amber Grin | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 未验证形态 | 是 |
| G81-0029 | Amber Grin | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0030 | Amber Mouth Open OH | Genesis8Female | /Hidden/People/Amber (3DU) | 控制器 | 否 |
| G81-0031 | Amber Open Smile | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 未验证形态 | 是 |
| G81-0032 | Amber Open Smile | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0033 | Amber Shy Smile | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0034 | Amber Shy Smile | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0035 | Amber Silly Face | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0036 | Amber Silly Face | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0037 | Amber Smile 01 | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0038 | Amber Smile 01 | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0039 | Amber Surprised | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0040 | Amber Surprised | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0041 | Amber Tongue Out | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0042 | Amber Tongue Out | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0043 | Amber Unsure | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G81-0044 | Amber Unsure | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G81-0045 | AutoLife_Age000s | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-0046 | AutoLife_Age000s Shape Null | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-0047 | AutoLife_Age005s Assist | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-0048 | AutoLife_Age005s YH Assist | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-0049 | BendfixAbdoLowbackUpfrontSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0050 | BendfixAbdolowerbackSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0051 | BendfixAbdolowerbackSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0052 | BendfixAbdolowfrontUpbackSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0053 | BendfixLThigh90SH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0054 | BendfixLthighSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0055 | BendfixRThigh90SH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0056 | BendfixRthighSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0057 | Breast Helper Super Fixer | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-0058 | BuggaBoo_nostrilsmoothHD | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0059 | Cadee Bite Lower Lip | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0060 | Cadee Bite Lower Lip | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0061 | Cadee Eyes Squint | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0062 | Cadee Eyes Squint | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0063 | Cadee Happy Face | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0064 | Cadee Happy Face | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0065 | Cadee Laugh | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0066 | Cadee Laugh | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0067 | Cadee Mouth Kiss | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0068 | Cadee Mouth Kiss | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0069 | Cadee Mouth OH | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0070 | Cadee Mouth OH | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0071 | Cadee Mouth OO | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0072 | Cadee Mouth OO | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0073 | Cadee Mouth Open Wide | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0074 | Cadee Mouth Open Wide | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0075 | Cadee Mouth Relaxed Open | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0076 | Cadee Mouth Relaxed Open | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0077 | Cadee Mouth Sneer | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0078 | Cadee Mouth Sneer | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0079 | Cadee Smile 01 | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0080 | Cadee Smile 01 | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0081 | Cadee Smile 02 | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0082 | Cadee Smile 02 | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0083 | Cadee Surprised | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0084 | Cadee Surprised | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0085 | Cadee Tongue Out 01 | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0086 | Cadee Tongue Out 01 | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0087 | Cadee Tongue Out 02 | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0088 | Cadee Tongue Out 02 | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0089 | Cadee Toothy Smile | Genesis8Female | /Pose Controls/Head/Expressions/Cadee (3DU) | 控制器 | 是 |
| G81-0090 | Cadee Toothy Smile | head | /Pose Controls/Head/Expressions/Cadee (3DU) | 别名 | 是 |
| G81-0091 | Camila Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-0092 | Carmelita Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-0093 | Chefei's head | geometry | /People | 稀疏形态 | 是 |
| G81-0094 | Chloe Face | geometry | /Detroit Chloe | 稀疏形态 | 是 |
| G81-0095 | CTRLMD_N_eCTRLEyesClosed_1 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-0096 | CTRLMD_N_Value_1_AM_Z | geometry | /Hidden/CTRLMDs | 稀疏形态 | 否 |
| G81-0097 | CTRLMD_N_YRotate_22 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-0098 | CTRLMD_N_YRotate_n22 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-0099 | DDGamerGirlPose03 | Genesis8Female | /Pose Controls/Full Body/DD Gamer Girl Poses | 控制器 | 是 |
| G81-0100 | DifaEyesClosed_100_L | geometry | /Hidden/People/Difa/eJCMs | 稀疏形态 | 否 |
| G81-0101 | DifaEyesClosed_100_R | geometry | /Hidden/People/Difa/eJCMs | 稀疏形态 | 否 |
| G81-0102 | eCTRLAciciaEyesClosedL | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0103 | eCTRLAciciaEyesClosedR | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0104 | eCTRLAciciaMouthOpen | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0105 | eCTRLAciciaMouthSmile | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0106 | eCTRLAciciaMouthSmileOpen | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0107 | eCTRLAciciaMouthSmileSimpleL | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0108 | eCTRLAciciaMouthSmileSimpleR | geometry | /Hidden/People/Acicia/eJCMs | 稀疏形态 | 否 |
| G81-0109 | eCTRLAuroreMouthSmile | Genesis8Female | /Hidden/People/Aurore/eCTRLs | 控制器 | 否 |
| G81-0110 | eCTRLCOReineEyesClosedL | geometry | /Hidden/People/CO Reine/eJCMs | 稀疏形态 | 否 |
| G81-0111 | eCTRLCOReineEyesClosedR | geometry | /Hidden/People/CO Reine/eJCMs | 稀疏形态 | 否 |
| G81-0112 | eCTRLCOReineLipsPart | geometry | /Hidden/People/CO Reine/eJCMs | 稀疏形态 | 否 |
| G81-0113 | eCTRLCOReineMouthSmile | geometry | /Hidden/People/CO Reine/eJCMs | 稀疏形态 | 否 |
| G81-0114 | eCTRLCOReineMouthSmileOpen | geometry | /Hidden/People/CO Reine/eJCMs | 稀疏形态 | 否 |
| G81-0115 | eCTRLEyesClosedL_GCC_DOA_Momiji | geometry | /Hidden/People/GCC/DOA Momiji | 稀疏形态 | 否 |
| G81-0116 | eCTRLEyesClosedL_GCC_DOA_Nanami | geometry | /Hidden/People/GCC/DOA Nanami | 稀疏形态 | 否 |
| G81-0117 | eCTRLEyesClosedR_GCC_DOA_Momiji | geometry | /Hidden/People/GCC/DOA Momiji | 稀疏形态 | 否 |
| G81-0118 | eCTRLEyesClosedR_GCC_DOA_Nanami | geometry | /Hidden/People/GCC/DOA Nanami | 稀疏形态 | 否 |
| G81-0119 | eCTRLGCCDOAKasumiEyesClosedL | geometry | /Hidden/People/GCC/DOA Kasumi | 稀疏形态 | 否 |
| G81-0120 | eCTRLGCCDOAKasumiEyesClosedR | geometry | /Hidden/People/GCC/DOA Kasumi | 稀疏形态 | 否 |
| G81-0121 | eCTRLGCCDOAKasumiMouthOpen | geometry | /Hidden/People/GCC/DOA Kasumi | 稀疏形态 | 否 |
| G81-0122 | eCTRLGraceYongBrowDownL | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G81-0123 | eCTRLGraceYongBrowDownR | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G81-0124 | eCTRLGraceYongMouthCornerBackL | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G81-0125 | eCTRLGraceYongMouthCornerBackR | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G81-0126 | eCTRLHannMeiCornerBackL | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G81-0127 | eCTRLHannMeiCornerBackR | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G81-0128 | eCTRLHannMeiMouthCornerDownL | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G81-0129 | eCTRLHannMeiMouthCornerDownR | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G81-0130 | eCTRLHannMeiMouthSmile | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G81-0131 | eCTRLHannMeiMouthSmileOpen | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G81-0132 | eCTRLKanade8Angry | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0133 | eCTRLKanade8EyelidsLowerUpDownL | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0134 | eCTRLKanade8EyelidsLowerUpDownR | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0135 | eCTRLKanade8EyesClosedL | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0136 | eCTRLKanade8EyesClosedR | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0137 | eCTRLKanade8Frown_HD | Genesis8Female | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0138 | eCTRLKanade8LipsPart | Genesis8Female | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0139 | eCTRLKanade8LipsPartCenter | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0140 | eCTRLKanade8LipsPuckerWide | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0141 | eCTRLKanade8MouthFrown | Genesis8Female | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0142 | eCTRLKanade8MouthOpen | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0143 | eCTRLKanade8MouthSmile | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0144 | eCTRLKanade8MouthSmileOpen | geometry | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G81-0145 | eCTRLMintoMouthSmileSimple_n100_L | Genesis8Female | /Hidden/People/Minto | 控制器 | 否 |
| G81-0146 | eCTRLMintoMouthSmileSimple_n100_R | Genesis8Female | /Hidden/People/Minto | 控制器 | 否 |
| G81-0147 | eCTRLMouthOpen_GCC_DOA_Momiji | geometry | /Hidden/People/GCC/DOA Momiji | 稀疏形态 | 否 |
| G81-0148 | eCTRLMouthOpen_GCC_DOA_Nanami | geometry | /Hidden/People/GCC/DOA Nanami | 稀疏形态 | 否 |
| G81-0149 | eCTRLMouthRuoXiSmileSimpleL | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0150 | eCTRLMouthRuoXiSmileSimpleR | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0151 | eCTRLNamCheeksBalloon | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G81-0152 | eCTRLNamEyesSquint | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G81-0153 | eCTRLNamMouthCornerBackL | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G81-0154 | eCTRLNamMouthCornerBackR | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G81-0155 | eCTRLNamMouthCornerDownL | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G81-0156 | eCTRLNamMouthCornerDownR | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G81-0157 | eCTRLReineMouthFrown | geometry | /Hidden/People/CO Reine/eJCMs | 稀疏形态 | 否 |
| G81-0158 | eCTRLRinEyesClosedL | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0159 | eCTRLRinEyesClosedR | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0160 | eCTRLRinMouthFrown | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0161 | eCTRLRinMouthOpen | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0162 | eCTRLRinMouthSmile | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0163 | eCTRLRinMouthSmileOpen | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0164 | eCTRLRinMouthSmileSimpleL | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0165 | eCTRLRinMouthSmileSimpleR | geometry | /Hidden/People/Rin/eJCM | 稀疏形态 | 否 |
| G81-0166 | eCTRLRuoXiEyesClosedL | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0167 | eCTRLRuoXiEyesClosedR | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0168 | eCTRLRuoXiEyesRelaxL | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0169 | eCTRLRuoXiEyesRelaxR | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0170 | eCTRLRuoXiMouthFrown | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0171 | eCTRLRuoXiMouthSmile | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0172 | eCTRLRuoXiMouthSmileOpen | geometry | /Hidden/People/RuoXi/eJCMs | 稀疏形态 | 否 |
| G81-0173 | eCTRLRuyunEyesClosedL | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0174 | eCTRLRuyunEyesClosedR | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0175 | eCTRLRuyunLipsPart | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0176 | eCTRLRuyunLipsPartCenter | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0177 | eCTRLRuyunLipsPuckerWide | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0178 | eCTRLRuyunMouthFrown | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0179 | eCTRLRuyunMouthSmile | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0180 | eCTRLRuyunMouthSmileOpen | geometry | /Hidden/People/Ruyun/eJCMs | 稀疏形态 | 否 |
| G81-0181 | eJCM_Bridget8_BrowUp-DownL | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0182 | eJCM_Bridget8_BrowUp-DownR | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0183 | eJCM_Bridget8_EyesClosedL | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0184 | eJCM_Bridget8_EyesClosedR | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0185 | eJCM_Bridget8_Fear | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0186 | eJCM_Bridget8_Glare | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0187 | eJCM_Bridget8_Happy | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0188 | eJCM_Bridget8_Pain | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0189 | eJCM_Bridget8_Rage | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0190 | eJCM_Bridget8_Scream | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0191 | eJCM_Bridget8_Serious | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0192 | eJCM_Bridget8_SmileFullFace | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0193 | eJCM_Bridget8_SmileOpenFullFace | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0194 | eJCM_Bridget8_Surprised | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0195 | eJCM_Bridget8_Triumph | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0196 | eJCM_Bridget8_vOW | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0197 | eJCM_Bridget8_Wink | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G81-0198 | eJCMAiko8Afraid | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0199 | eJCMAiko8Angry | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0200 | eJCMAiko8Bereft | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0201 | eJCMAiko8BrowSqueeze_HDLv1L | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0202 | eJCMAiko8BrowSqueeze_HDLv1R | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0203 | eJCMAiko8CheekCreaseL | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0204 | eJCMAiko8CheekCreaseR | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0205 | eJCMAiko8CheeksBalloonPucker | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0206 | eJCMAiko8Concentrate | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0207 | eJCMAiko8Confident | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0208 | eJCMAiko8Confused | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0209 | eJCMAiko8Desire | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0210 | eJCMAiko8Disgust | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0211 | eJCMAiko8Excitement | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0212 | eJCMAiko8EyesClosedL | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0213 | eJCMAiko8EyesClosedR | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0214 | eJCMAiko8Fear | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0215 | eJCMAiko8Flirting | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0216 | eJCMAiko8Happy | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0217 | eJCMAiko8Ill | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0218 | eJCMAiko8LipsPuckerWide | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0219 | eJCMAiko8MouthOpen | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0220 | eJCMAiko8MouthSmile_HDLv1 | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0221 | eJCMAiko8MouthSmileOpen_HDLv1 | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0222 | eJCMAiko8MouthSmileSimpleL | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0223 | eJCMAiko8MouthSmileSimpleR | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0224 | eJCMAiko8Pain | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0225 | eJCMAiko8Pleased | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0226 | eJCMAiko8Pouty | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0227 | eJCMAiko8Rage | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0228 | eJCMAiko8Sarcastic | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0229 | eJCMAiko8Scream | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0230 | eJCMAiko8Silly | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0231 | eJCMAiko8SmileFullFace | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0232 | eJCMAiko8SmileOpenFullFace | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0233 | eJCMAiko8Snarl | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0234 | eJCMAiko8Surprised | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0235 | eJCMAiko8Tired | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G81-0236 | eJCMAiko8Triumph | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0237 | eJCMAiko8vOW | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0238 | eJCMAiko8Wink | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G81-0239 | EJCMAnnaLiAfraid | geometry | /Hidden/People/AnnaLi | 稀疏形态 | 否 |
| G81-0240 | eJCMAnnaLiMouthFrown | geometry | /Hidden/People/AnnaLi | 稀疏形态 | 否 |
| G81-0241 | eJCMAnnaLiMouthOpen | geometry | /Hidden/People/AnnaLi | 稀疏形态 | 否 |
| G81-0242 | eJCMAnnaLiMouthSmileOpen | geometry | /Hidden/People/AnnaLi | 稀疏形态 | 否 |
| G81-0243 | eJCMAuroreAfraid_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0244 | eJCMAuroreAngry_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0245 | eJCMAuroreEyesClosedL | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0246 | eJCMAuroreEyesClosedR | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0247 | eJCMAuroreFear_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0248 | eJCMAuroreHappy_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0249 | eJCMAuroreLipsPuckerWide | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0250 | eJCMAuroreMouthOpen | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0251 | eJCMAuroreMouthSmile | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0252 | eJCMAuroreMouthSmileOpen | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0253 | eJCMAuroreMouthSmileSimpleL | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0254 | eJCMAuroreMouthSmileSimpleR | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0255 | eJCMAurorePain_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0256 | eJCMAurorePouty_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0257 | eJCMAuroreRage_HD_div2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0258 | eJCMAuroreScream_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0259 | eJCMAuroreShock_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0260 | eJCMAuroreSmileFullFace_HD_div2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0261 | eJCMAuroreSmileOpenFullFace_HD_div2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0262 | eJCMAuroreSurprise_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-0263 | eJCMBillieEyesClosedL | Genesis81FemaleGeom | /Hidden/People/Billie | 稀疏形态 | 否 |
| G81-0264 | eJCMBillieEyesClosedR | Genesis81FemaleGeom | /Hidden/People/Billie | 稀疏形态 | 否 |
| G81-0265 | eJCMBuggaEyeclose_L | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0266 | eJCMBuggaEyeclose_R | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0267 | eJCMCapricaEyesClosedL | Genesis81FemaleGeom | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0268 | eJCMCapricaEyesClosedR | Genesis81FemaleGeom | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0269 | eJCMCeciliaLauEyesClosedL | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0270 | eJCMCeciliaLauEyesClosedR | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0271 | eJCMCeciliaLauMouthOpen | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0272 | eJCMCeciliaLauMouthSmile | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0273 | eJCMCeciliaLauSmileOpen | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0274 | eJCMCeciliaLauSmileSimpleL | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0275 | eJCMCeciliaLauSmileSimpleR | geometry | /Hidden/People/CeciliaLau/eJCMs | 稀疏形态 | 否 |
| G81-0276 | eJCMCO_IchigoEyeClosed_100_L | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0277 | eJCMCO_IchigoLipsPart | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0278 | eJCMCO_IchigoLipsPartCenter | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0279 | eJCMCO_IchigoLipsPuckerWide | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0280 | eJCMCO_IchigoLipTopUp-DownL | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0281 | eJCMCO_IchigoLipTopUp-DownR | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0282 | eJCMCO_IchigoMouthOpen_100 | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0283 | eJCMCO_IchigoMouthSidetoSide_100_L | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0284 | eJCMCO_IchigoMouthSidetoSide_100_R | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0285 | eJCMCO_IchigoMouthSmile | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0286 | eJCMCO_IchigoMouthSmileOpen_100 | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0287 | eJCMCO_IchigoMouthSmileSimpleL | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0288 | eJCMCO_IchigoMouthSmileSimpleR | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0289 | eJCMCO_IchigoSmileFullFaceHD | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0290 | eJCMDifaJawSide-Side_100 | geometry | /Hidden/People/Difa/eJCMs | 稀疏形态 | 否 |
| G81-0291 | eJCMGabriela8_Angry_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0292 | eJCMGabriela8_BrowSqueezeL_HDLv2 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0293 | eJCMGabriela8_BrowSqueezeR_HDLv2 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0294 | eJCMGabriela8_CheeksBalloonPucker | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0295 | eJCMGabriela8_EyesClosedL | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0296 | eJCMGabriela8_EyesClosedR | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0297 | eJCMGabriela8_Fear_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0298 | eJCMGabriela8_Happy_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0299 | eJCMGabriela8_LipsPuckerWide | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0300 | eJCMGabriela8_MouthSmile_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0301 | eJCMGabriela8_MouthSmileOpen_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0302 | eJCMGabriela8_NoseCompression_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0303 | eJCMGabriela8_Pain_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0304 | eJCMGabriela8_Rage_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0305 | eJCMGabriela8_Scream_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0306 | eJCMGabriela8_SmileFullFace_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0307 | eJCMGabriela8_SmileOpenFullFace_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0308 | eJCMGabriela8_Triumph_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0309 | eJCMGabriela8_Wink_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-0310 | eJCMGraceYongBrowInnerUpL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0311 | eJCMGraceYongBrowInnerUpR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0312 | eJCMGraceYongCheekCreaseL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0313 | eJCMGraceYongCheekCreaseR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0314 | eJCMGraceYongCheeksBalloonL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0315 | eJCMGraceYongCheeksBalloonPucker | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0316 | eJCMGraceYongCheeksBalloonR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0317 | eJCMGraceYongCheeksSuckInL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0318 | eJCMGraceYongCheeksSuckInR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0319 | eJCMGraceYongEyelidsUpperDownL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0320 | eJCMGraceYongEyelidsUpperDownR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0321 | eJCMGraceYongEyesSquintL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0322 | eJCMGraceYongEyesSquintR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0323 | eJCMGraceYongJawSide-Side | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0324 | eJCMGraceYongMouthOpen | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0325 | eJCMGraceYongMouthSide-SideL | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0326 | eJCMGraceYongMouthSide-SideR | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0327 | eJCMGraceYongMouthSmileOpen | geometry | /Hidden/People/PCGraceYong/eJCMs | 稀疏形态 | 否 |
| G81-0328 | eJCMHannMeiCheeksBalloonL_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0329 | eJCMHannMeiCheeksBalloonPucker_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0330 | eJCMHannMeiCheeksBalloonR_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0331 | eJCMHannMeiCheeksSuckInL_n100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0332 | eJCMHannMeiCheeksSuckInR_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0333 | eJCMHannMeiEyeClose_100_L | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0334 | eJCMHannMeiEyeClose_100_R | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0335 | eJCMHannMeiEyeSquint_100_L | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0336 | eJCMHannMeiEyeSquint_100_R | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0337 | eJCMHannMeiLipsPucker_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0338 | eJCMHannMeiLowerEyelidUp_100_L | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0339 | eJCMHannMeiLowerEyelidUp_100_R | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0340 | eJCMHannMeiMouthBareTeeth_n100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0341 | eJCMHannMeiMouthCornerUp_100_L | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0342 | eJCMHannMeiMouthCornerUp_100_R | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0343 | eJCMHannMeiMouthFrown_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0344 | eJCMHannMeiMouthOpen_100 | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0345 | eJCMHannMeiMouthSide-SideL | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0346 | eJCMHannMeiMouthSide-SideR | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0347 | eJCMHannMeiMouthSmileSimple_100_L | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0348 | eJCMHannMeiMouthSmileSimple_100_R | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0349 | eJCMHannMeiUpperEyelidDn_100_L | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0350 | eJCMHannMeiUpperEyelidDn_100_R | geometry | /Hidden/People/RSHannMei/eJCM | 稀疏形态 | 否 |
| G81-0351 | eJCMMarisEyesRelax | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0352 | eJCMMarisLipsClosed-BareTeeth | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0353 | eJCMMarisLipsPart | Genesis8Female | /Hidden/People/Maris/eJCM | 稀疏形态 | 否 |
| G81-0354 | eJCMMarisLipsPartCenter | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0355 | eJCMMarisMouthSmile | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0356 | eJCMMarisMouthSmileSimple | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0357 | eJCMMarisMouthSmileSimpleL | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0358 | eJCMMarisMouthSmileSimpleR | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G81-0359 | eJCMMiraiEyesClosedL | geometry | /Hidden/People/Mirai | 稀疏形态 | 是 |
| G81-0360 | eJCMMiraiEyesClosedR | geometry | /Hidden/People/Mirai | 稀疏形态 | 是 |
| G81-0361 | eJCMMrsChow8_BrowSqueezeL_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0362 | eJCMMrsChow8_BrowSqueezeR_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0363 | eJCMMrsChow8_Excitement_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0364 | eJCMMrsChow8_EyesClosedL | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0365 | eJCMMrsChow8_EyesClosedR | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0366 | eJCMMrsChow8_Fear_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0367 | eJCMMrsChow8_Happy_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0368 | eJCMMrsChow8_LipsPuckerWide | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0369 | eJCMMrsChow8_Pain_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0370 | eJCMMrsChow8_Rage_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0371 | eJCMMrsChow8_Scream_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0372 | eJCMMrsChow8_SmileFullFace_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0373 | eJCMMrsChow8_SmileFullFaceOpen_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0374 | eJCMMrsChow8_Triumph_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0375 | eJCMMrsChow8_Wink_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-0376 | eJCMNamBrowSqueezeL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0377 | eJCMNamBrowSqueezeR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0378 | eJCMNamCheekEyeFlexL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0379 | eJCMNamCheekEyeFlexR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0380 | eJCMNamCheeksBalloonL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0381 | eJCMNamCheeksBalloonPucker | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0382 | eJCMNamCheeksBalloonR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0383 | eJCMNamCheeksSuckInL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0384 | eJCMNamCheeksSuckInR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0385 | eJCMNamEyeClose_100_L | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0386 | eJCMNamEyeClose_100_R | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0387 | eJCMNamEyelidsLowerUpDownL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0388 | eJCMNamEyelidsLowerUpDownR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0389 | eJCMNamEyelidsUpperDownL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0390 | eJCMNamEyelidsUpperDownR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0391 | eJCMNamEyesSquintL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0392 | eJCMNamEyesSquintR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0393 | eJCMNamLipsPart | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0394 | eJCMNamLipsPartCenter | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0395 | eJCMNamLipsPressed | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0396 | eJCMNamLipsPucker | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0397 | eJCMNamLipsPuckerWide | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0398 | eJCMNamMouthBareTeeth | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0399 | eJCMNamMouthCornerForwardL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0400 | eJCMNamMouthCornerForwardR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0401 | eJCMNamMouthCornerUpL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0402 | eJCMNamMouthCornerUpR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0403 | eJCMNamMouthFrown | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0404 | eJCMNamMouthOpen | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0405 | eJCMNamMouthSide-SideL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0406 | eJCMNamMouthSide-SideR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0407 | eJCMNamMouthSmile | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0408 | eJCMNamMouthSmileOpen | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0409 | eJCMNamMouthSmileSimpleL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0410 | eJCMNamMouthSmileSimpleR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0411 | eJCMNamMouthWideL | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0412 | eJCMNamMouthWideR | geometry | /Hidden/People/CSNam/eJCMs | 稀疏形态 | 否 |
| G81-0413 | eJCMXiaLipsPart | geometry | /Hidden/People/Xia | 稀疏形态 | 否 |
| G81-0414 | eJCMXiaLipsPartCenter | geometry | /Hidden/People/Xia | 稀疏形态 | 否 |
| G81-0415 | eJCMXiaSmile | geometry | /Hidden/People/Xia | 稀疏形态 | 否 |
| G81-0416 | eJCMXiaSmileOpen | geometry | /Hidden/People/Xia | 稀疏形态 | 否 |
| G81-0417 | eJCMXiaSmileSimpleL | geometry | /Hidden/People/Xia | 稀疏形态 | 否 |
| G81-0418 | eJCMXiaSmileSimpleR | geometry | /Hidden/People/Xia | 稀疏形态 | 否 |
| G81-0419 | eJCMZhiSmile | geometry | /Hidden/People/Zhi | 稀疏形态 | 否 |
| G81-0420 | eJCMZhiSmileOpen | geometry | /Hidden/People/Zhi | 稀疏形态 | 否 |
| G81-0421 | eJMCO_IchigoEyeClosed_100_R | geometry | /Hidden/People/CO Ichigo/eJCMs | 稀疏形态 | 否 |
| G81-0422 | Elisa v2 | geometry | /Full Body | 稀疏形态 | 是 |
| G81-0423 | Elsie Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-0424 | Elsie Smile 1 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0425 | Elsie Smile 1 | head | /Pose Controls/Head/Expressions | 别名 | 是 |
| G81-0426 | Elsie Smile 2 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0427 | Elsie Smile 2 | head | /Pose Controls/Head/Expressions | 别名 | 是 |
| G81-0428 | Elsie Smile Simple | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0429 | Elsie Smile Simple | head | /Pose Controls/Head/Expressions | 别名 | 是 |
| G81-0430 | Eyes Closed Deep | Genesis8Female | /Pose Controls/Head/Eyes | 控制器 | 是 |
| G81-0431 | GamerGirl Expression 02 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G81-0432 | GamerGirl Expression 04 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G81-0433 | GamerGirl Expression 06 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G81-0434 | GamerGirl Expression 08 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G81-0435 | GamerGirl Expression 10 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G81-0436 | Gayll Mouth AH | Genesis8Female | /Pose Controls/Head/Expressions/Gayll (3DU) | 控制器 | 是 |
| G81-0437 | Gayll Mouth AH | head | /Pose Controls/Head/Expressions/Gayll (3DU) | 别名 | 是 |
| G81-0438 | GluteFixBendthigh90LSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0439 | GluteFixBendthigh90RSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0440 | GluteFixBendthighS2SLSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0441 | GluteFixBendthighS2SLSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0442 | GluteFixBendthighS2SRSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0443 | GluteFixBendthighS2SRSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-0444 | GS PlusSize - Body 1 | geometry | /Full Body/People/GuaiamuStudio - Morphs | 稀疏形态 | 是 |
| G81-0445 | Heavy LAbdo Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G81-0446 | Heavy LAbdo Details Neg and Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G81-0447 | Heavy Lower Details &amp; LAbdoBend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G81-0448 | Heavy U&amp;L Details Neg and Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G81-0449 | Heidi Kiss | Genesis8Female | /Pose Controls/Head/Expressions/Heidi (3DU) | 控制器 | 是 |
| G81-0450 | Heidi Kiss | head | /Pose Controls/Head/Expressions/Heidi (3DU) | 别名 | 是 |
| G81-0451 | Heidi Tongue Out 02 | Genesis8Female | /Pose Controls/Head/Expressions/Heidi (3DU) | 控制器 | 是 |
| G81-0452 | Heidi Tongue Out 02 | head | /Pose Controls/Head/Expressions/Heidi (3DU) | 别名 | 是 |
| G81-0453 | Hide Eyes | Genesis8Female | /General | 控制器 | 是 |
| G81-0454 | HikaruMCMcloseLeye | geometry | /Hidden/MCM | 稀疏形态 | 否 |
| G81-0455 | HikaruMCMcloseReye | geometry | /Hidden/MCM | 稀疏形态 | 否 |
| G81-0456 | HS Keicy Sexy Lip 2 | Genesis8Female | /Pose Controls/Head/Expressions/Keicy Expression | 控制器 | 是 |
| G81-0457 | HS Keicy Sexy Lip 2 | head | /Pose Controls/Head/Expressions/Keicy Expression | 别名 | 是 |
| G81-0458 | HS Sanny Gental Smile | geometry | /Pose Controls/Head/Expressions/Sanny Expression | 稀疏形态 | 是 |
| G81-0459 | HS Vivy Lovely Balloon | Genesis8Female | /Pose Controls/Head/Expressions/Vivy Expression | 控制器 | 是 |
| G81-0460 | HS Vivy Sexy Lip | Genesis8Female | /Pose Controls/Head/Expressions/Vivy Expression | 控制器 | 是 |
| G81-0461 | HS YOYO Emotion Angry | Genesis8Female | /Pose Controls/Head/Expressions/YOYO Expression | 控制器 | 是 |
| G81-0462 | HS YOYO Emotion Sexy Pucker | Genesis8Female | /Pose Controls/Head/Expressions/YOYO Expression | 控制器 | 是 |
| G81-0463 | HS YOYO Emotion Smile Open | Genesis8Female | /Pose Controls/Head/Expressions/YOYO Expression | 控制器 | 是 |
| G81-0464 | HS YOYO Emotion Smile open1 | Genesis8Female | /Pose Controls/Head/Expressions/YOYO Expression | 控制器 | 是 |
| G81-0465 | HS YOYO Emotion Smile open2 | Genesis8Female | /Pose Controls/Head/Expressions/YOYO Expression | 控制器 | 是 |
| G81-0466 | i3DSS 01 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0467 | i3DSS 01 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0468 | i3DSS 02 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0469 | i3DSS 02 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0470 | i3DSS 03 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0471 | i3DSS 03 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0472 | i3DSS 04 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0473 | i3DSS 04 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0474 | i3DSS 05 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0475 | i3DSS 05 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0476 | i3DSS 06 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0477 | i3DSS 06 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0478 | i3DSS 07 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0479 | i3DSS 07 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0480 | i3DSS 08 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0481 | i3DSS 08 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0482 | i3DSS 09 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0483 | i3DSS 09 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0484 | i3DSS 10 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0485 | i3DSS 10 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0486 | i3DSS 11 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0487 | i3DSS 11 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0488 | i3DSS 12 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0489 | i3DSS 12 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0490 | i3DSS 13 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0491 | i3DSS 13 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0492 | i3DSS 14 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0493 | i3DSS 14 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0494 | i3DSS 15 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0495 | i3DSS 15 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0496 | i3DSS 16 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0497 | i3DSS 16 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0498 | i3DSS 17 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0499 | i3DSS 17 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0500 | i3DSS 18 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0501 | i3DSS 18 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0502 | i3DSS 19 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0503 | i3DSS 19 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0504 | i3DSS 20 | Genesis8Female | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 控制器 | 是 |
| G81-0505 | i3DSS 20 | head | /Pose Controls/Head/Expressions/Godin i3D/Sweet &amp; Sassy Vol. 2/!Bonus Expressions | 别名 | 是 |
| G81-0506 | JCM Heavy LThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0507 | JCM Heavy RThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0508 | JCM Heavy UAbdoback | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0509 | JCM Heavy UAbdoback Labdo Forward | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0510 | JCM Liloo ArmBendFix L | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-0511 | JCM Liloo ArmBendFix L Null | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-0512 | JCM Liloo ArmBendFix R | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-0513 | JCM Liloo ArmBendFix R Null | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-0514 | JCM Pear LAbdo Bend | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0515 | JCM Pear LThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0516 | JCM Pear RThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0517 | JCM Pear UAbdoback Labdo Forward | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G81-0518 | JCMAbdoUChestLSide-sideFixLCombo | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-0519 | JCMAbdoUChestLSide-sideFixRCombo | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-0520 | JCMAbdoUpperChestLSide-sideFixL | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-0521 | JCMAbdoUpperChestLSide-sideFixR | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-0522 | JCMBuggaBooBrowOuterUp-DownL | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0523 | JCMBuggaBooBrowOuterUp-DownR | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0524 | JCMBuggaBooBrowSqueezeL | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0525 | JCMBuggaBooBrowSqueezeR | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0526 | JCMBuggaBooEyelidsLowerUpDown_L | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0527 | JCMBuggaBooEyelidsLowerUpDown_R | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0528 | JCMBuggaBooEyesRelaxL | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0529 | JCMBuggaBooEyesRelaxR | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0530 | JCMBuggaBrowInnerUp-DownL | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0531 | JCMBuggaBrowInnerUp-DownR | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0532 | JCMBuggaEyelidsUpperCloseL | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0533 | JCMBuggaEyelidsUpperCloseL_UP | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0534 | JCMBuggaEyelidsUpperCloseR | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0535 | JCMBuggaEyelidsUpperCloseR_UP | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0536 | JCMBuggaEyesSquint_L | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0537 | JCMBuggaEyesSquint_R | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0538 | JCMCheekBalloonL_BuggaBoo | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0539 | JCMCheekBalloonR_BuggaBoo | Genesis8Female | /Hidden/Correctives/BuggaBoo | 控制器 | 否 |
| G81-0540 | JCMEyesUp_BuggaBoo | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0541 | JCMShockHD_BuggaBoo | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0542 | JCMSmileOpenFullFaceHD_BuggaBoo | geometry | /Hidden/Correctives/BuggaBoo | 稀疏形态 | 否 |
| G81-0543 | Kanade Mixable 01 Talking | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0544 | Kanade Mixable 01 Talking | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0545 | Kanade Mixable 02 Laugh | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0546 | Kanade Mixable 02 Laugh | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0547 | Kanade Mixable 03 Serious | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0548 | Kanade Mixable 03 Serious | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0549 | Kanade Mixable 04 Distracted | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0550 | Kanade Mixable 04 Distracted | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0551 | Kanade Mixable 05 Boasting | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0552 | Kanade Mixable 05 Boasting | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0553 | Kanade Mixable 06 Sorry | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0554 | Kanade Mixable 06 Sorry | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0555 | Kanade Mixable 07 Disdain | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0556 | Kanade Mixable 07 Disdain | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0557 | Kanade Mixable 08 Satisfaction | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0558 | Kanade Mixable 08 Satisfaction | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0559 | Kanade Mixable 09 Upset | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0560 | Kanade Mixable 09 Upset | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0561 | Kanade Mixable 10 Explaining | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0562 | Kanade Mixable 10 Explaining | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0563 | Kanade Mixable 11 Smile | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0564 | Kanade Mixable 11 Smile | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0565 | Kanade Mixable 12 Meditation | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0566 | Kanade Mixable 12 Meditation | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0567 | Kanade Mixable 13 Excited | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0568 | Kanade Mixable 13 Excited | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0569 | Kanade Mixable 14 Amazed | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0570 | Kanade Mixable 14 Amazed | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0571 | Kanade Mixable 15 Sad | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0572 | Kanade Mixable 15 Sad | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0573 | Kanade Mixable 16 Fun | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0574 | Kanade Mixable 16 Fun | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0575 | Kanade Mixable 17 Bragging | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0576 | Kanade Mixable 17 Bragging | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0577 | Kanade Mixable 18 Love | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0578 | Kanade Mixable 18 Love | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0579 | Kanade Mixable 19 Surprise | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0580 | Kanade Mixable 19 Surprise | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0581 | Kanade Mixable 20 Wink | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0582 | Kanade Mixable 20 Wink | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0583 | Kanade Mixable 21 Happy | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0584 | Kanade Mixable 21 Happy | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0585 | Kanade Mixable 22 Wonder | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0586 | Kanade Mixable 22 Wonder | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0587 | Kanade Mixable 23 Chatting | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0588 | Kanade Mixable 23 Chatting | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0589 | Kanade Mixable 24 Soft Smile | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0590 | Kanade Mixable 24 Soft Smile | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0591 | Kanade Mixable 25 Worry | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0592 | Kanade Mixable 25 Worry | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0593 | Kanade Mixable 26 Kindness | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0594 | Kanade Mixable 26 Kindness | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0595 | Kanade Mixable 27 Sweet Smile | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0596 | Kanade Mixable 27 Sweet Smile | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0597 | Kanade Mixable 28 Alert | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0598 | Kanade Mixable 28 Alert | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0599 | Kanade Mixable 29 Desire | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0600 | Kanade Mixable 29 Desire | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0601 | Kanade Mixable 30 Open Smile | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 控制器 | 是 |
| G81-0602 | Kanade Mixable 30 Open Smile | head | /Pose Controls/Head/Expressions/EmmaAndJordi/Kanade Mixable | 别名 | 是 |
| G81-0603 | KikyouEyeFixLeft | geometry | /Eyes | 稀疏形态 | 否 |
| G81-0604 | KikyouEyeFixRight | geometry | /Eyes | 稀疏形态 | 否 |
| G81-0605 | Kirara Body Busty | geometry | /Kirara | 稀疏形态 | 是 |
| G81-0606 | Kirara Face | geometry | /Kirara | 稀疏形态 | 是 |
| G81-0607 | KrashWerks - KerriAnne Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-0608 | Ling Xiaoyu body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G81-0609 | Ling Xiaoyu head | geometry | /People/Stylized | 稀疏形态 | 是 |
| G81-0610 | Lip Bottom Bite | Genesis8Female | /Pose Controls/Head/Mouth/Lips | 控制器 | 是 |
| G81-0611 | Liya | Genesis8Female | /Full Body/People/Real World | 控制器 | 是 |
| G81-0612 | Liya | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0613 | MCM RS Pei lEyeClosed 125 | geometry | /Hidden/People/Pei | 稀疏形态 | 否 |
| G81-0614 | MCM RS Pei REyeClosed 125 | geometry | /Hidden/People/Pei | 稀疏形态 | 否 |
| G81-0615 | MCM3duG8FCadee_EyesClosedL | geometry | /Hidden/People/Cadee | 稀疏形态 | 否 |
| G81-0616 | MCM3duG8FCadee_EyesClosedR | geometry | /Hidden/People/Cadee | 稀疏形态 | 是 |
| G81-0617 | MCM3duG8FHeidi_EyeClosed_L | geometry | /Hidden/People/Heidi (3DU) | 稀疏形态 | 否 |
| G81-0618 | MCM3duG8FHeidi_EyeClosed_R | geometry | /Hidden/People/Heidi (3DU) | 稀疏形态 | 否 |
| G81-0619 | MCM_LY_Sterling_right | geometry | /Hidden/Correctives/Lyoness/Sterling | 稀疏形态 | 否 |
| G81-0620 | MCMAliceLiuEyesClosedL | geometry | /Hidden/People/Alice Liu/MCMs | 稀疏形态 | 否 |
| G81-0621 | MCMAliceLiuEyesClosedR | geometry | /Hidden/People/Alice Liu/MCMs | 稀疏形态 | 否 |
| G81-0622 | MCMAnakkuEyesClosedL | geometry | /Hidden/People/Anakku | 稀疏形态 | 否 |
| G81-0623 | MCMAnakkuEyesClosedR | geometry | /Hidden/People/Anakku | 稀疏形态 | 否 |
| G81-0624 | MCMAnakkuLashesDownL | Genesis8Female | /Hidden/People/Anakku | 未验证形态 | 否 |
| G81-0625 | MCMAnakkuLashesDownR | Genesis8Female | /Hidden/People/Anakku | 未验证形态 | 否 |
| G81-0626 | MCMArinaEyesClosedL | Genesis8Female | /Hidden/People/Arina/MCMs | 稀疏形态 | 否 |
| G81-0627 | MCMArinaEyesClosedR | Genesis8Female | /Hidden/People/Arina/MCMs | 稀疏形态 | 否 |
| G81-0628 | MCMBaeEyesClosedL | geometry | /Hidden/People/Bae/MCMs | 稀疏形态 | 否 |
| G81-0629 | MCMBaeEyesClosedR | geometry | /Hidden/People/Bae/MCMs | 稀疏形态 | 否 |
| G81-0630 | MCMBereftEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0631 | MCMBrowInnerDownEJMidori_L | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0632 | MCMBrowInnerDownEJMidori_R | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0633 | MCMBrownDownEJMidori_L | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0634 | MCMBrownDownEJMidori_R | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0635 | MCMCapricaFootDwn_75_L | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0636 | MCMCapricaFootDwn_75_R | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0637 | MCMCBEvelynLEyeClose | geometry | /Hidden/MCMs/EvelynFace | 稀疏形态 | 否 |
| G81-0638 | MCMCBEvelynREyeClose | geometry | /Hidden/MCMs/EvelynFace | 稀疏形态 | 否 |
| G81-0639 | MCMDDElsieEyesClosedL-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G81-0640 | MCMDDElsieEyesClosedR-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G81-0641 | MCMDDPaisleyEyesClosedL-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G81-0642 | MCMEyelidUpperDownEJMidori_L | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0643 | MCMEyelidUpperDownEJMidori_R | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0644 | MCMEyelidUpperUpEJMidori_L | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0645 | MCMEyelidUpperUpEJMidori_R | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0646 | MCMEyesClosedEJMidori_L | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0647 | MCMEyesClosedEJMidori_R | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0648 | MCMFierceEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0649 | MCMG8F3duGayll_EyesClosedL | geometry | /Hidden/People/Gayll (3DU) | 稀疏形态 | 否 |
| G81-0650 | MCMG8F3duGayll_EyesClosedR | geometry | /Hidden/People/Gayll (3DU) | 稀疏形态 | 否 |
| G81-0651 | MCMGDFHMNing12yoEyeClosedL | geometry | /Hidden/Ning/MCMs | 稀疏形态 | 否 |
| G81-0652 | MCMGDFHMNing12yoEyeClosedR | geometry | /Hidden/Ning/MCMs | 稀疏形态 | 否 |
| G81-0653 | MCMGDYueLEyeClose | geometry | /Hidden/MCMs/Yue | 稀疏形态 | 否 |
| G81-0654 | MCMGDYueREyeClose | geometry | /Hidden/MCMs/Yue | 稀疏形态 | 否 |
| G81-0655 | MCMGraceYongEyesClosed_Left | geometry | /Hidden/People/PCGraceYong/MCMs | 稀疏形态 | 否 |
| G81-0656 | MCMGraceYongEyesClosed_Right | geometry | /Hidden/People/PCGraceYong/MCMs | 稀疏形态 | 否 |
| G81-0657 | MCMHana_EyeCloseLeft | geometry | /Hidden/Correctives/Hana | 稀疏形态 | 否 |
| G81-0658 | MCMHana_EyeCloseRight | geometry | /Hidden/Correctives/Hana | 稀疏形态 | 否 |
| G81-0659 | MCMHashimotoEyesL | geometry | /Hidden/People/Hashimoto/MCMs | 稀疏形态 | 否 |
| G81-0660 | MCMHashimotoEyesR | geometry | /Hidden/People/Hashimoto/MCMs | 稀疏形态 | 否 |
| G81-0661 | MCMHSKeicyEyesClosedL | geometry | /Hidden/People/Keicy | 稀疏形态 | 否 |
| G81-0662 | MCMHSKeicyEyesClosedR | geometry | /Hidden/People/Keicy | 稀疏形态 | 否 |
| G81-0663 | MCMHSLurysEyesClosedL | geometry | /Hidden/People/Lurys | 稀疏形态 | 否 |
| G81-0664 | MCMHSLurysEyesClosedR | geometry | /Hidden/People/Lurys | 稀疏形态 | 否 |
| G81-0665 | MCMHSSannyEyesClosedL | geometry | /Hidden/People/Sanny | 稀疏形态 | 否 |
| G81-0666 | MCMHSSannyEyesClosedR | geometry | /Hidden/People/Sanny | 稀疏形态 | 否 |
| G81-0667 | MCMHSSannyMouthSmile | geometry | /Hidden/People/Sanny | 稀疏形态 | 否 |
| G81-0668 | MCMHuaEyesClosedL | geometry | /Hidden/People/Hua/MCMs | 稀疏形态 | 是 |
| G81-0669 | MCMHuaEyesClosedR | geometry | /Hidden/People/Hua/MCMs | 稀疏形态 | 是 |
| G81-0670 | MCMImoenFootDwn_75_L | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0671 | MCMImoenFootDwn_75_R | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0672 | MCMKannaEyesClosedL-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G81-0673 | MCMKannaEyesClosedR-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G81-0674 | MCMKayleeEyesClosedL | geometry | /Hidden/People/Teen Kaylee 8 | 稀疏形态 | 否 |
| G81-0675 | MCMKayleeEyesClosedR | geometry | /Hidden/People/Teen Kaylee 8 | 稀疏形态 | 否 |
| G81-0676 | MCMKeicyMouthSmile | geometry | /Hidden/People/Keicy | 稀疏形态 | 否 |
| G81-0677 | MCMKeicyMouthSmileOpen | geometry | /Hidden/People/Keicy | 稀疏形态 | 否 |
| G81-0678 | MCMKimSeohyunEyesClose_LashesL | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G81-0679 | MCMKimSeohyunEyesClose_LashesR | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G81-0680 | MCMKimSeohyunEyesCloseL | geometry | /Hidden/People/KimSeohyun | 稀疏形态 | 否 |
| G81-0681 | MCMKimSeohyunEyesCloseR | geometry | /Hidden/People/KimSeohyun | 稀疏形态 | 否 |
| G81-0682 | MCMKimSeohyunMouthSmileOpen | geometry | /Hidden/People/KimSeohyun | 稀疏形态 | 否 |
| G81-0683 | MCMLancyEyesClosedL | geometry | /Hidden/People/Lancy | 稀疏形态 | 否 |
| G81-0684 | MCMLancyEyesClosedR | geometry | /Hidden/People/Lancy | 稀疏形态 | 否 |
| G81-0685 | MCMLancyMouthSmile | geometry | /Hidden/People/Lancy | 稀疏形态 | 否 |
| G81-0686 | MCMLancySmileOpen | geometry | /Hidden/People/Lancy | 稀疏形态 | 否 |
| G81-0687 | MCMMeiAsakuraEyeBlinkL | Genesis8_1Female | /Hidden/People/Mei Asakura/MCMs | 稀疏形态 | 否 |
| G81-0688 | MCMMeiAsakuraEyeBlinkR | Genesis8_1Female | /Hidden/People/Mei Asakura/MCMs | 稀疏形态 | 否 |
| G81-0689 | MCMMeiAsakuraEyesClosedL | Genesis8Female | /Hidden/People/Mei Asakura/MCMs | 稀疏形态 | 否 |
| G81-0690 | MCMMeiAsakuraEyesClosedR | Genesis8Female | /Hidden/People/Mei Asakura/MCMs | 稀疏形态 | 否 |
| G81-0691 | MCMMinamiEyeClosed_100_L | geometry | /Hidden/People/Minami | 稀疏形态 | 否 |
| G81-0692 | MCMMinamiEyeClosed_100_R | geometry | /Hidden/People/Minami | 稀疏形态 | 否 |
| G81-0693 | MCMMintoCheeksBalloonPucker | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0694 | MCMMintoCheeksSuckInL | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0695 | MCMMintoCheeksSuckInR | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0696 | MCMMintoLipsPuckerWide | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0697 | MCMMintoMouthBareTeeth | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0698 | MCMMintoMouthNarrowL | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0699 | MCMMintoMouthNarrowR | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0700 | MCMMintoMouthSidetoSide_100_L | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0701 | MCMMintoMouthSidetoSide_100_R | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0702 | MCMMintoMouthSmile | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0703 | MCMMintoMouthSmileOpen_100 | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0704 | MCMMintoMouthSmileOpen_n50 | Genesis8Female | /Hidden/People/Minto | 控制器 | 否 |
| G81-0705 | MCMMintoMouthSmileSimpleL | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0706 | MCMMintoMouthSmileSimpleR | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0707 | MCMMintoMouthWideL | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0708 | MCMMintoMouthWideR | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0709 | MCMMintovF | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0710 | MCMMintovM | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0711 | MCMMintovOW | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0712 | MCMMintovUW | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0713 | MCMMintovW | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0714 | MCMMissShangEyeCloseL | geometry | /Hidden/People/MissShang/MCMs | 稀疏形态 | 否 |
| G81-0715 | MCMMissShangEyeCloseR | geometry | /Hidden/People/MissShang/MCMs | 稀疏形态 | 否 |
| G81-0716 | MCMMissShangMouthOpen | geometry | /Hidden/People/MissShang/MCMs | 稀疏形态 | 否 |
| G81-0717 | MCMMissShangMouthSmile | geometry | /Hidden/People/MissShang/MCMs | 稀疏形态 | 否 |
| G81-0718 | MCMMissShangMouthSmileOpen | geometry | /Hidden/People/MissShang/MCMs | 稀疏形态 | 否 |
| G81-0719 | MCMMiyaCheeksBalloonL | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0720 | MCMMiyaCheeksBalloonPucker | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0721 | MCMMiyaCheeksBalloonR | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0722 | MCMMiyaCheeksSuckInL | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0723 | MCMMiyaCheeksSuckInR | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0724 | MCMMiyaEyeClose_100_L | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0725 | MCMMiyaEyeClose_100_R | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0726 | MCMMiyaEyelidsUpperDownL | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0727 | MCMMiyaEyelidsUpperDownR | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0728 | MCMMiyaEyesSquintL | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0729 | MCMMiyaEyesSquintR | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0730 | MCMMiyaLipsClosedBareTeeth | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0731 | MCMMiyaLipsPuckerPressed | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0732 | MCMMiyaLipsPuckerWide | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0733 | MCMMiyaMouthFrown | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0734 | MCMMiyaMouthNarrowL | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0735 | MCMMiyaMouthNarrowR | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0736 | MCMMiyaMouthOpen | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0737 | MCMMiyaMouthSidetoSide_L | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0738 | MCMMiyaMouthSidetoSide_R | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0739 | MCMMiyaMouthSmile | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0740 | MCMMiyaMouthSmileOpen | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0741 | MCMMiyaMouthSmileSimple_L | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0742 | MCMMiyaMouthSmileSimple_R | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0743 | MCMMiyaMouthWideL | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0744 | MCMMiyaMouthWideR | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0745 | MCMMiyaNoseWrinkle | geometry | /Hidden/People/CSMiya | 稀疏形态 | 否 |
| G81-0746 | MCMNoseWrinkleEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0747 | MCMPainEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0748 | MCMRabaEyesCloseL100 | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G81-0749 | MCMRabaEyesCloseR100 | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G81-0750 | MCMRageEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0751 | MCMRNKaylaEyeClosed_100_L | geometry | /Hidden/People/RN Kayla/pJCMs | 稀疏形态 | 否 |
| G81-0752 | MCMRNKaylaEyeClosed_100_R | geometry | /Hidden/People/RN Kayla/pJCMs | 稀疏形态 | 否 |
| G81-0753 | MCMRumiEyeBlinkL | Genesis8_1Female | /Hidden/People/Rumi/MCMs | 稀疏形态 | 否 |
| G81-0754 | MCMRumiEyeBlinkR | Genesis8_1Female | /Hidden/People/Rumi/MCMs | 稀疏形态 | 否 |
| G81-0755 | MCMRumiEyesClosedL | Genesis8Female | /Hidden/People/Rumi/MCMs | 稀疏形态 | 否 |
| G81-0756 | MCMRumiEyesClosedR | Genesis8Female | /Hidden/People/Rumi/MCMs | 稀疏形态 | 否 |
| G81-0757 | MCMS_lEyecloseXiaoFang | geometry | /Hidden/Base | 稀疏形态 | 否 |
| G81-0758 | MCMS_rEyecloseXiaoFang | geometry | /Hidden/Base | 稀疏形态 | 否 |
| G81-0759 | MCMSaeEyeClosedL | geometry | /Hidden/People/Sae | 稀疏形态 | 是 |
| G81-0760 | MCMSaeEyeClosedR | geometry | /Hidden/People/Sae | 稀疏形态 | 是 |
| G81-0761 | MCMSannyMouthSmileOpen | geometry | /Hidden/People/Sanny | 稀疏形态 | 否 |
| G81-0762 | MCMSayaSmileLv1 | Genesis8Female | /Hidden/People/Saya/MCMs | 稀疏形态 | 否 |
| G81-0763 | MCMScreamEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0764 | MCMSeoHyunG8FEyesClosedL | geometry | /Hidden/People/Seo Hyun G8F/MCMs | 稀疏形态 | 否 |
| G81-0765 | MCMSeoHyunG8FEyesClosedR | geometry | /Hidden/People/Seo Hyun G8F/MCMs | 稀疏形态 | 否 |
| G81-0766 | MCMShirleyLeeEyesClosedL | geometry | /Hidden/People/Shirley Lee/MCMs | 稀疏形态 | 否 |
| G81-0767 | MCMShirleyLeeEyesClosedR | geometry | /Hidden/People/Shirley Lee/MCMs | 稀疏形态 | 否 |
| G81-0768 | MCMShizukaEyeClosedL | geometry | /Hidden/People/Shizuka/MCMs | 稀疏形态 | 否 |
| G81-0769 | MCMShizukaEyeClosedR | geometry | /Hidden/People/Shizuka/MCMs | 稀疏形态 | 否 |
| G81-0770 | MCMShizukaMouthSmile | geometry | /Hidden/People/Shizuka/MCMs | 稀疏形态 | 否 |
| G81-0771 | MCMSillyEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0772 | MCMSueEyeClosedL | geometry | /Hidden/People/Sue/MCMs | 稀疏形态 | 是 |
| G81-0773 | MCMSueEyeClosedR | geometry | /Hidden/People/Sue/MCMs | 稀疏形态 | 是 |
| G81-0774 | MCMTeenJosie8EyesClosedL | geometry | /Hidden/People/Teen Josie 8 | 稀疏形态 | 否 |
| G81-0775 | MCMTeenJosie8EyesClosedR | geometry | /Hidden/People/Teen Josie 8 | 稀疏形态 | 否 |
| G81-0776 | MCMVivyAngry | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0777 | MCMVivyCheeksBalloonPucker | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0778 | MCMVivyEyeClosedLeft | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0779 | MCMVivyEyeClosedRight | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0780 | MCMVivyFlirting | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0781 | MCMVivyFrown | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0782 | MCMVivyMouthSmileOpen | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0783 | MCMVivyShock | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0784 | MCMVivySmileFullFace | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0785 | MCMVivySmileOpenFullFace | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0786 | MCMVivySurprise | geometry | /Hidden/People/Vivy | 稀疏形态 | 否 |
| G81-0787 | MCMVWBabyEllaLEyeClosed | Genesis8Female | /Hidden/Correctives/Base Head | 控制器 | 否 |
| G81-0788 | MCMVWBabyEllaREyeClosed | Genesis8Female | /Hidden/Correctives/Base Head | 控制器 | 否 |
| G81-0789 | MCMVWNewbornEllaLEyeClosed | Genesis8Female | /Hidden/Correctives/Base Head | 控制器 | 否 |
| G81-0790 | MCMVWNewbornEllaREyeClosed | Genesis8Female | /Hidden/Correctives/Base Head | 控制器 | 否 |
| G81-0791 | MCMWinkEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G81-0792 | MCMXiaoYunEyemazing | geometry | /Hidden/People/Xiao Yun/MCMs | 稀疏形态 | 否 |
| G81-0793 | MCMXiaoYunEyesClosedL | geometry | /Hidden/People/Xiao Yun/MCMs | 稀疏形态 | 否 |
| G81-0794 | MCMXiaoYunEyesClosedR | geometry | /Hidden/People/Xiao Yun/MCMs | 稀疏形态 | 否 |
| G81-0795 | MCMXuEyesClosedL | geometry | /Hidden/People/Xu/MCMs | 稀疏形态 | 否 |
| G81-0796 | MCMXuEyesClosedR | geometry | /Hidden/People/Xu/MCMs | 稀疏形态 | 否 |
| G81-0797 | MCMYIYICheeksBalloonPucker | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0798 | MCMYIYIEyesClosedL | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0799 | MCMYIYIEyesClosedR | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0800 | MCMYIYILipPuckerPressed | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0801 | MCMYIYILipsPuckerWide | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0802 | MCMYIYIMouthSmile | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0803 | MCMYIYIMouthSmileOpen | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0804 | MCMYIYIMouthSmileSimpleL | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0805 | MCMYIYIMouthSmileSimpleR | geometry | /Hidden/People/YIYI | 稀疏形态 | 否 |
| G81-0806 | MCMYoungMintoEyeClosed_100_L | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0807 | MCMYoungMintoEyeClosed_100_R | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0808 | MCMYoungMintoMouthOpen_100 | geometry | /Hidden/People/Minto | 稀疏形态 | 否 |
| G81-0809 | MCMYOYOCheeksBalloonPucker | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0810 | MCMYOYOEyelidsUpperDown_L | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0811 | MCMYOYOEyelidsUpperDown_R | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0812 | MCMYOYOEyesClosedL | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0813 | MCMYOYOEyesClosedR | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0814 | MCMYOYOLipPuckerPress | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0815 | MCMYOYOLipPuckerwide | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0816 | MCMYOYOMouthcornerUp-Dwon_L | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0817 | MCMYOYOMouthcornerUp-Dwon_R | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0818 | MCMYOYOMouthFrown | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0819 | MCMYOYOMouthSmile | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0820 | MCMYOYOMouthSmileOpen | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0821 | MCMYOYOMouthSmileSimple_L | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0822 | MCMYOYOMouthSmileSimple_R | geometry | /Hidden/People/YOYO | 稀疏形态 | 否 |
| G81-0823 | MCMYuiEyesClosedL | geometry | /Hidden/People/Yui/MCMs | 稀疏形态 | 否 |
| G81-0824 | MCMYuiEyesClosedR | geometry | /Hidden/People/Yui/MCMs | 稀疏形态 | 否 |
| G81-0825 | Mio | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-0826 | Mio Head | Genesis81FemaleGeom | /People/Real World | 稀疏形态 | 是 |
| G81-0827 | Momo | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0828 | Mouth Chu | Genesis8Female | /Pose Controls/Head/Mouth | 控制器 | 是 |
| G81-0829 | Mouth Smile Open Wide | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-0830 | Mouth Smile Open Wide 2 | Genesis8Female | /Pose Controls/Head/Mouth | 控制器 | 是 |
| G81-0831 | My Friend - 01 Genesis 8 Female | Genesis8Female | /Pose Controls/Head/Expressions/My Friend | 控制器 | 是 |
| G81-0832 | My Friend - 01 Genesis 8 Female | head | /Pose Controls/Head/Expressions/My Friend | 别名 | 是 |
| G81-0833 | My Friend - 02 Genesis 8 Female | Genesis8Female | /Pose Controls/Head/Expressions/My Friend | 控制器 | 是 |
| G81-0834 | My Friend - 02 Genesis 8 Female | head | /Pose Controls/Head/Expressions/My Friend | 别名 | 是 |
| G81-0835 | My Friend - 03 Genesis 8 Female | Genesis8Female | /Pose Controls/Head/Expressions/My Friend | 控制器 | 是 |
| G81-0836 | My Friend - 03 Genesis 8 Female | head | /Pose Controls/Head/Expressions/My Friend | 别名 | 是 |
| G81-0837 | My Friend - 04 Genesis 8 Female | Genesis8Female | /Pose Controls/Head/Expressions/My Friend | 控制器 | 是 |
| G81-0838 | My Friend - 04 Genesis 8 Female | head | /Pose Controls/Head/Expressions/My Friend | 别名 | 是 |
| G81-0839 | My Friend - 05 Genesis 8 Female | Genesis8Female | /Pose Controls/Head/Expressions/My Friend | 控制器 | 是 |
| G81-0840 | My Friend - 05 Genesis 8 Female | head | /Pose Controls/Head/Expressions/My Friend | 别名 | 是 |
| G81-0841 | Nam Face Cute | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0842 | Nam Face Delight | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0843 | Nam Face Idle 01 | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0844 | Nam Face Idle 02 | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0845 | Nam Face Idle 03 | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0846 | Nam Face Naughty | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0847 | Nam Face Scare | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0848 | Nam Face Scream | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0849 | Nam Face Serious | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0850 | Nam Face Shocked | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0851 | Nam Face Smile | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0852 | Nam Face Smile Eyesclose | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0853 | Nam Face Surpirse MouthOpen | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0854 | Nam Face Surprise | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0855 | Nam Face Trouble | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0856 | Nam Terror Eyes Down - Up | Genesis8Female | /Pose Controls/Head/Expressions/Nam Expressions | 控制器 | 是 |
| G81-0857 | Natalie Head | Genesis81FemaleGeom | /People/Real World | 稀疏形态 | 是 |
| G81-0858 | Nyao Expression 01 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0859 | Nyao Expression 02 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0860 | Nyao Expression 03 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0861 | Nyao Expression 04 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0862 | Nyao Expression 05 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0863 | Nyao Expression 06 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G81-0864 | Nyao Smile | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-0865 | Nyao Smile Open | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-0866 | Nyao Smile Simple | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-0867 | PBMCSNamCheekCreaseL | geometry | /Hidden/People/CSNam/PBMs | 稀疏形态 | 否 |
| G81-0868 | PBMCSNamCheekCreaseR | geometry | /Hidden/People/CSNam/PBMs | 稀疏形态 | 否 |
| G81-0869 | PBMsRNFreddaEyeClose_100_L | geometry | /Hidden/People/RNFredda/PBMs | 稀疏形态 | 否 |
| G81-0870 | PBMsRNFreddaEyeClose_100_R | geometry | /Hidden/People/RNFredda/PBMs | 稀疏形态 | 否 |
| G81-0871 | Pear &amp; Heavy LAbdo Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G81-0872 | Pear Lower Details Bendback UAbdo&amp;Lower | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G81-0873 | pJCMCapricaFootBndBckL | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0874 | pJCMCapricaFootBndBckR | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0875 | pJCMCapricaFootBndUpL | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0876 | pJCMCapricaFootBndUpR | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0877 | pJCMCapricaToesBndDwnL | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0878 | pJCMCapricaToesBndDwnR | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0879 | pJCMCapricaToesBndUpL | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0880 | pJCMCapricaToesBndUpR | geometry | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-0881 | pJCMImoenFootBndBckL | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0882 | pJCMImoenFootBndBckR | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0883 | pJCMImoenFootBndUpL | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0884 | pJCMImoenFootBndUpR | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0885 | pJCMImoenToesBndDwnL | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0886 | pJCMImoenToesBndDwnR | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0887 | pJCMImoenToesBndUpL | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0888 | pJCMImoenToesBndUpR | geometry | /Hidden/People/Imoen | 稀疏形态 | 否 |
| G81-0889 | pMCM3duG8FAmberEyeCloseR | geometry | /Hidden/People/Amber (3DU) | 稀疏形态 | 否 |
| G81-0890 | QX Ferin Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-0891 | QX Ferin Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-0892 | QX Ferin Teeth | Genesis8Female | /Fantasy SciFi | 稀疏形态 | 是 |
| G81-0893 | Ravyn (3DU) | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0894 | RW Serenity Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0895 | S3D Seema Head | geometry |  | 稀疏形态 | 是 |
| G81-0896 | SF Addy Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0897 | SF Carly Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0898 | SF Emily Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0899 | SF Grace Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0900 | SF Hailey Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0901 | SF Jasmine Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0902 | SF Kayo Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0903 | SF Mackenzie Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0904 | SF Madeline Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0905 | SF Shannon Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0906 | SF Sophie Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0907 | SF Zoe Head | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0908 | SFHD Arm Bend Down Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-0909 | SFHD Arm Bend Down Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-0910 | SFHD Collar Bend Up Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-0911 | SFHD Collar Bend Up Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-0912 | Shuang Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-0913 | Thigh bend Both+ Stephanie | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-0914 | Thigh side Both Olympia | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-0915 | Thight side Both Victoria | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-0916 | Tifa Body | geometry | /Tifa G8F | 稀疏形态 | 是 |
| G81-0917 | Tifa Face | geometry | /Tifa G8F | 稀疏形态 | 是 |
| G81-0918 | Tongue Out | Genesis8Female | /Pose Controls/Head/Mouth/Tongue | 控制器 | 是 |
| G81-0919 | Vo Xiao Hong  | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0920 | Vo Xiao Mei Body | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-0921 | VO Xiao Xin | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-0922 | VO Xiao Xong Closed L | geometry | /Hidden/People/Vo Xoap Hong/pjCMs | 稀疏形态 | 否 |
| G81-0923 | VO Xiao Xong Closed R | geometry | /Hidden/People/Vo Xoap Hong/pjCMs | 稀疏形态 | 否 |
| G81-0924 | VOXiaoXin Closedl | geometry | /Hidden/People/VO Xiaoxin/pjCMs | 稀疏形态 | 否 |
| G81-0925 | VOXiaoXin Closedr | geometry | /Hidden/People/VO Xiaoxin/pjCMs | 稀疏形态 | 否 |
| G81-0926 | VW Baby Ella | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-0927 | VW Newborn Ella | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-0928 | VYK Kiyomiko Head | geometry | /People/Stylized | 稀疏形态 | 是 |
| G81-0929 | Xiao Bei EYE Closed L | geometry | /Hidden/People/Vo Xiao Bei/pjCMS | 稀疏形态 | 否 |
| G81-0930 | Xiao Bei EYE Closed L | geometry | /Hidden/People/Vo Xiao Bei/pjCMS | 稀疏形态 | 否 |
| G81-0931 | Xiao Bei EYE Closed R | geometry | /Hidden/People/Vo Xiao Bei/pjCMS | 稀疏形态 | 否 |
| G81-0932 | Xiao Bei EYE Closed R | geometry | /Hidden/People/Vo Xiao Bei/pjCMS | 稀疏形态 | 否 |
| G81-0933 | Xiao Hong Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-0934 | Yujin Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-0935 | Z AB Eye Close Left Fix | geometry | /Hidden/Correctives/Zeddicuss/Z Anime Beauty | 稀疏形态 | 是 |
| G81-0936 | Z AB Eye Close Right Fix | geometry | /Hidden/Correctives/Zeddicuss/Z Anime Beauty | 稀疏形态 | 是 |
| G81-0937 | Z Anime Girl Head | Genesis8Female | /People/Stylized | 控制器 | 是 |
| G81-0938 | Z AOV 01 Sad | Genesis8Female | /Pose Controls/Head/Expressions/Z Alone on Valentines | 控制器 | 是 |
| G81-0939 | Z AOV 01 Sad | head | /Pose Controls/Head/Expressions/Z Alone on Valentines | 别名 | 是 |
| G81-0940 | Z AOV Crying | Genesis8Female | /Pose Controls/Head/Expressions/Z Alone on Valentines | 控制器 | 是 |
| G81-0941 | Z AOV Crying | head | /Pose Controls/Head/Expressions/Z Alone on Valentines | 别名 | 是 |
| G81-0942 | Z AOV Crying Out Loud | Genesis8Female | /Pose Controls/Head/Expressions/Z Alone on Valentines | 控制器 | 是 |
| G81-0943 | Z AOV Crying Out Loud | head | /Pose Controls/Head/Expressions/Z Alone on Valentines | 别名 | 是 |
| G81-0944 | Z AOV Sobbing | Genesis8Female | /Pose Controls/Head/Expressions/Z Alone on Valentines | 控制器 | 是 |
| G81-0945 | Z AOV Sobbing | head | /Pose Controls/Head/Expressions/Z Alone on Valentines | 别名 | 是 |
| G81-0946 | Z AOV Upset | Genesis8Female | /Pose Controls/Head/Expressions/Z Alone on Valentines | 控制器 | 是 |
| G81-0947 | Z AOV Upset | head | /Pose Controls/Head/Expressions/Z Alone on Valentines | 别名 | 是 |
| G81-0948 | Z BGE Kind Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Best Girl Ever | 控制器 | 是 |
| G81-0949 | Z BGE Kind Smile | head | /Pose Controls/Head/Expressions/Z Best Girl Ever | 别名 | 是 |
| G81-0950 | Z BGE Side Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Best Girl Ever | 控制器 | 是 |
| G81-0951 | Z BGE Side Smile | head | /Pose Controls/Head/Expressions/Z Best Girl Ever | 别名 | 是 |
| G81-0952 | Z BGE Stuck Up | Genesis8Female | /Pose Controls/Head/Expressions/Z Best Girl Ever | 控制器 | 是 |
| G81-0953 | Z BGE Stuck Up | head | /Pose Controls/Head/Expressions/Z Best Girl Ever | 别名 | 是 |
| G81-0954 | Z BGE Surprise | Genesis8Female | /Pose Controls/Head/Expressions/Z Best Girl Ever | 控制器 | 是 |
| G81-0955 | Z BGE Surprise | head | /Pose Controls/Head/Expressions/Z Best Girl Ever | 别名 | 是 |
| G81-0956 | Z BGE Upset | Genesis8Female | /Pose Controls/Head/Expressions/Z Best Girl Ever | 控制器 | 是 |
| G81-0957 | Z BGE Upset | head | /Pose Controls/Head/Expressions/Z Best Girl Ever | 别名 | 是 |
| G81-0958 | Z BTR Cheeky Smirk | Genesis8Female | /Pose Controls/Head/Expressions/Z Break The Rules | 控制器 | 是 |
| G81-0959 | Z BTR Cheeky Smirk | head | /Pose Controls/Head/Expressions/Z Break The Rules | 别名 | 是 |
| G81-0960 | Z BTR Cheerful | Genesis8Female | /Pose Controls/Head/Expressions/Z Break The Rules | 控制器 | 是 |
| G81-0961 | Z BTR Cheerful | head | /Pose Controls/Head/Expressions/Z Break The Rules | 别名 | 是 |
| G81-0962 | Z BTR Pouting | Genesis8Female | /Pose Controls/Head/Expressions/Z Break The Rules | 控制器 | 是 |
| G81-0963 | Z BTR Pouting | head | /Pose Controls/Head/Expressions/Z Break The Rules | 别名 | 是 |
| G81-0964 | Z BTR Rebel | Genesis8Female | /Pose Controls/Head/Expressions/Z Break The Rules | 控制器 | 是 |
| G81-0965 | Z BTR Rebel | head | /Pose Controls/Head/Expressions/Z Break The Rules | 别名 | 是 |
| G81-0966 | Z BTR Smile Subtle | Genesis8Female | /Pose Controls/Head/Expressions/Z Break The Rules | 控制器 | 是 |
| G81-0967 | Z BTR Smile Subtle | head | /Pose Controls/Head/Expressions/Z Break The Rules | 别名 | 是 |
| G81-0968 | Z CO A Bit Sad | Genesis8Female | /Pose Controls/Head/Expressions/Z Cuteness Overload | 控制器 | 是 |
| G81-0969 | Z CO A Bit Sad | head | /Pose Controls/Head/Expressions/Z Cuteness Overload | 别名 | 是 |
| G81-0970 | Z CO Cutie Pie | Genesis8Female | /Pose Controls/Head/Expressions/Z Cuteness Overload | 控制器 | 是 |
| G81-0971 | Z CO Cutie Pie | head | /Pose Controls/Head/Expressions/Z Cuteness Overload | 别名 | 是 |
| G81-0972 | Z CO Excited | Genesis8Female | /Pose Controls/Head/Expressions/Z Cuteness Overload | 控制器 | 是 |
| G81-0973 | Z CO Excited | head | /Pose Controls/Head/Expressions/Z Cuteness Overload | 别名 | 是 |
| G81-0974 | Z CO Happy Laugh | Genesis8Female | /Pose Controls/Head/Expressions/Z Cuteness Overload | 控制器 | 是 |
| G81-0975 | Z CO Happy Laugh | head | /Pose Controls/Head/Expressions/Z Cuteness Overload | 别名 | 是 |
| G81-0976 | Z CO Side Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Cuteness Overload | 控制器 | 是 |
| G81-0977 | Z CO Side Smile | head | /Pose Controls/Head/Expressions/Z Cuteness Overload | 别名 | 是 |
| G81-0978 | Z DAH G8F Drunk Laugh | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-0979 | Z DAH G8F Drunk Laugh | head | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 别名 | 是 |
| G81-0980 | Z DAH G8F Drunk Silly | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-0981 | Z DAH G8F Drunk Silly | head | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 别名 | 是 |
| G81-0982 | Z DAH G8F Happy Drunk | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-0983 | Z DAH G8F Happy Drunk | head | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 别名 | 是 |
| G81-0984 | Z DAH G8F Headache | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-0985 | Z DAH G8F Headache | head | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 别名 | 是 |
| G81-0986 | Z DAH G8F Hungover | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-0987 | Z DAH G8F Hungover | head | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 别名 | 是 |
| G81-0988 | Z DAH G8F Hungover 2 | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-0989 | Z DAH G8F Hungover 2 | head | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 别名 | 是 |
| G81-0990 | Z EA Disappointed | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-0991 | Z EA Disappointed | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-0992 | Z EA Distant Gaze | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-0993 | Z EA Distant Gaze | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-0994 | Z EA Fear | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-0995 | Z EA Fear | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-0996 | Z EA Happy | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-0997 | Z EA Happy | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-0998 | Z EA Relaxed | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-0999 | Z EA Relaxed | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-1000 | Z EA Shock | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-1001 | Z EA Shock | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-1002 | Z EA Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-1003 | Z EA Smile | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-1004 | Z EA Smile Simple | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-1005 | Z EA Smile Simple | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-1006 | Z EA Talking | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-1007 | Z EA Talking | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-1008 | Z EA Tired | Genesis8Female | /Pose Controls/Head/Expressions/Z Everyday Activities | 控制器 | 是 |
| G81-1009 | Z EA Tired | head | /Pose Controls/Head/Expressions/Z Everyday Activities | 别名 | 是 |
| G81-1010 | Z FE Disappointed | Genesis8Female | /Pose Controls/Head/Expressions/Z Feminine Essence | 控制器 | 是 |
| G81-1011 | Z FE Disappointed | head | /Pose Controls/Head/Expressions/Z Feminine Essence | 别名 | 是 |
| G81-1012 | Z FE Gorgeous Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Feminine Essence | 控制器 | 是 |
| G81-1013 | Z FE Gorgeous Smile | head | /Pose Controls/Head/Expressions/Z Feminine Essence | 别名 | 是 |
| G81-1014 | Z FE Happy Glorious | Genesis8Female | /Pose Controls/Head/Expressions/Z Feminine Essence | 控制器 | 是 |
| G81-1015 | Z FE Happy Glorious | head | /Pose Controls/Head/Expressions/Z Feminine Essence | 别名 | 是 |
| G81-1016 | Z FE Serious Beautiful | Genesis8Female | /Pose Controls/Head/Expressions/Z Feminine Essence | 控制器 | 是 |
| G81-1017 | Z FE Serious Beautiful | head | /Pose Controls/Head/Expressions/Z Feminine Essence | 别名 | 是 |
| G81-1018 | Z FE Unsure | Genesis8Female | /Pose Controls/Head/Expressions/Z Feminine Essence | 控制器 | 是 |
| G81-1019 | Z FE Unsure | head | /Pose Controls/Head/Expressions/Z Feminine Essence | 别名 | 是 |
| G81-1020 | Z FI 01 Terror G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Fear Itself | 控制器 | 是 |
| G81-1021 | Z FI 02 Fright G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Fear Itself | 控制器 | 是 |
| G81-1022 | Z FI 03 Distress G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Fear Itself | 控制器 | 是 |
| G81-1023 | Z FI 04 Panic G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Fear Itself | 控制器 | 是 |
| G81-1024 | Z FI 05 Dread G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Fear Itself | 控制器 | 是 |
| G81-1025 | Z HAF Anger | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1026 | Z HAF Anger | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1027 | Z HAF Angry Snarl  | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1028 | Z HAF Angry Snarl  | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1029 | Z HAF Angst | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1030 | Z HAF Angst | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1031 | Z HAF Angst | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1032 | Z HAF Crying | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1033 | Z HAF Crying | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1034 | Z HAF Crying | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1035 | Z HAF Diabolical Laugh | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1036 | Z HAF Diabolical Laugh | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1037 | Z HAF Dismay | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1038 | Z HAF Dismay | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1039 | Z HAF Dismay | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1040 | Z HAF Dispair | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1041 | Z HAF Dispair | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1042 | Z HAF Dispair | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1043 | Z HAF Distress | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1044 | Z HAF Distress | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1045 | Z HAF Distress | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1046 | Z HAF Dread | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1047 | Z HAF Dread | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1048 | Z HAF Dread | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1049 | Z HAF Evil Growl | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1050 | Z HAF Evil Growl | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1051 | Z HAF Evil Look | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1052 | Z HAF Evil Look | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1053 | Z HAF Evil Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1054 | Z HAF Evil Smile | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1055 | Z HAF Fear | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1056 | Z HAF Fear | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1057 | Z HAF Fear | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1058 | Z HAF Fear In Horror | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1059 | Z HAF Fear In Horror | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1060 | Z HAF Fear In Horror | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1061 | Z HAF Harrowing Scream | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1062 | Z HAF Harrowing Scream | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1063 | Z HAF Misery | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1064 | Z HAF Misery | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1065 | Z HAF Misery | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1066 | Z HAF Pain | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1067 | Z HAF Pain | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1068 | Z HAF Pain | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1069 | Z HAF Panic | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1070 | Z HAF Panic | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1071 | Z HAF Panic | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1072 | Z HAF Scream | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1073 | Z HAF Scream | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1074 | Z HAF Shriek | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1075 | Z HAF Shriek | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1076 | Z HAF Sinister Laugh | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1077 | Z HAF Sinister Laugh | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1078 | Z HAF Sorrow | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1079 | Z HAF Sorrow | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1080 | Z HAF Sorrow | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1081 | Z HAF Terror | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 控制器 | 是 |
| G81-1082 | Z HAF Terror | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1083 | Z HAF Terror | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Fear | 别名 | 是 |
| G81-1084 | Z HAF Wicked | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1085 | Z HAF Wicked | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1086 | Z HAF Wrath | Genesis8Female | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 控制器 | 是 |
| G81-1087 | Z HAF Wrath | head | /Pose Controls/Head/Expressions/Z Horror and Fear/Anger | 别名 | 是 |
| G81-1088 | Z HAW Agonizing Pain G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Hurt and Wounded | 控制器 | 是 |
| G81-1089 | Z HAW Agonizing Pain G8F | head | /Pose Controls/Head/Expressions/Z Hurt and Wounded | 别名 | 是 |
| G81-1090 | Z HAW Distress G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Hurt and Wounded | 控制器 | 是 |
| G81-1091 | Z HAW Distress G8F | head | /Pose Controls/Head/Expressions/Z Hurt and Wounded | 别名 | 是 |
| G81-1092 | Z HAW In Pain G8F | Genesis8Female | /Pose Controls/Head/Expressions/Z Hurt and Wounded | 控制器 | 是 |
| G81-1093 | Z HAW In Pain G8F | head | /Pose Controls/Head/Expressions/Z Hurt and Wounded | 别名 | 是 |
| G81-1094 | Z MIR Pregnant Shape | Genesis8Female | /Full Body/People/Real World | 控制器 | 是 |
| G81-1095 | Z OM Cross | Genesis8Female | /Pose Controls/Head/Expressions/Z On a Mission | 控制器 | 是 |
| G81-1096 | Z OM Cross | head | /Pose Controls/Head/Expressions/Z On a Mission | 别名 | 是 |
| G81-1097 | Z OM Delighted | Genesis8Female | /Pose Controls/Head/Expressions/Z On a Mission | 控制器 | 是 |
| G81-1098 | Z OM Delighted | head | /Pose Controls/Head/Expressions/Z On a Mission | 别名 | 是 |
| G81-1099 | Z OM Determined | Genesis8Female | /Pose Controls/Head/Expressions/Z On a Mission | 控制器 | 是 |
| G81-1100 | Z OM Determined | head | /Pose Controls/Head/Expressions/Z On a Mission | 别名 | 是 |
| G81-1101 | Z OM Enraged | Genesis8Female | /Pose Controls/Head/Expressions/Z On a Mission | 控制器 | 是 |
| G81-1102 | Z OM Enraged | head | /Pose Controls/Head/Expressions/Z On a Mission | 别名 | 是 |
| G81-1103 | Z OM Resilient | Genesis8Female | /Pose Controls/Head/Expressions/Z On a Mission | 控制器 | 是 |
| G81-1104 | Z OM Resilient | head | /Pose Controls/Head/Expressions/Z On a Mission | 别名 | 是 |
| G81-1105 | Z PAP G8F Agony | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1106 | Z PAP G8F Agony | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1107 | Z PAP G8F Contentment | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1108 | Z PAP G8F Contentment | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1109 | Z PAP G8F Delight | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1110 | Z PAP G8F Delight | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1111 | Z PAP G8F Elation | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1112 | Z PAP G8F Elation | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1113 | Z PAP G8F Fullfillment | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1114 | Z PAP G8F Fullfillment | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1115 | Z PAP G8F Grief | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1116 | Z PAP G8F Grief | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1117 | Z PAP G8F Happiness | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1118 | Z PAP G8F Happiness | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1119 | Z PAP G8F Hedonism | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1120 | Z PAP G8F Hedonism | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1121 | Z PAP G8F Hurt | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1122 | Z PAP G8F Hurt | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1123 | Z PAP G8F Indulgence | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1124 | Z PAP G8F Indulgence | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1125 | Z PAP G8F Irritation | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1126 | Z PAP G8F Irritation | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1127 | Z PAP G8F Pain | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1128 | Z PAP G8F Pain | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1129 | Z PAP G8F Pure Joy | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1130 | Z PAP G8F Pure Joy | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1131 | Z PAP G8F Rapture | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1132 | Z PAP G8F Rapture | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1133 | Z PAP G8F Sadness | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1134 | Z PAP G8F Sadness | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1135 | Z PAP G8F Satisfaction | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1136 | Z PAP G8F Satisfaction | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1137 | Z PAP G8F Struggle | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1138 | Z PAP G8F Struggle | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1139 | Z PAP G8F Suffering | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1140 | Z PAP G8F Suffering | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1141 | Z PAP G8F Torment | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1142 | Z PAP G8F Torment | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1143 | Z PAP G8F Wounded | Genesis8Female | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 控制器 | 是 |
| G81-1144 | Z PAP G8F Wounded | head | /Pose Controls/Head/Expressions/Z Pleasure and Pain | 别名 | 是 |
| G81-1145 | Z RC Chatting Serious | Genesis8Female | /Pose Controls/Head/Expressions/Z Real Conversation | 控制器 | 是 |
| G81-1146 | Z RC Chatting Serious | head | /Pose Controls/Head/Expressions/Z Real Conversation | 别名 | 是 |
| G81-1147 | Z RC Chatty | Genesis8Female | /Pose Controls/Head/Expressions/Z Real Conversation | 控制器 | 是 |
| G81-1148 | Z RC Chatty | head | /Pose Controls/Head/Expressions/Z Real Conversation | 别名 | 是 |
| G81-1149 | Z RC Conversation Happy | Genesis8Female | /Pose Controls/Head/Expressions/Z Real Conversation | 控制器 | 是 |
| G81-1150 | Z RC Conversation Happy | head | /Pose Controls/Head/Expressions/Z Real Conversation | 别名 | 是 |
| G81-1151 | Z RC Conversation Upset | Genesis8Female | /Pose Controls/Head/Expressions/Z Real Conversation | 控制器 | 是 |
| G81-1152 | Z RC Conversation Upset | head | /Pose Controls/Head/Expressions/Z Real Conversation | 别名 | 是 |
| G81-1153 | Z RC Talking Cheeky | Genesis8Female | /Pose Controls/Head/Expressions/Z Real Conversation | 控制器 | 是 |
| G81-1154 | Z RC Talking Cheeky | head | /Pose Controls/Head/Expressions/Z Real Conversation | 别名 | 是 |
| G81-1155 | Z RS Determined | Genesis8Female | /Pose Controls/Head/Expressions/Z Rebel Spirit | 控制器 | 是 |
| G81-1156 | Z RS Determined | head | /Pose Controls/Head/Expressions/Z Rebel Spirit | 别名 | 是 |
| G81-1157 | Z RS Fierce | Genesis8Female | /Pose Controls/Head/Expressions/Z Rebel Spirit | 控制器 | 是 |
| G81-1158 | Z RS Fierce | head | /Pose Controls/Head/Expressions/Z Rebel Spirit | 别名 | 是 |
| G81-1159 | Z RS Innocent | Genesis8Female | /Pose Controls/Head/Expressions/Z Rebel Spirit | 控制器 | 是 |
| G81-1160 | Z RS Innocent | head | /Pose Controls/Head/Expressions/Z Rebel Spirit | 别名 | 是 |
| G81-1161 | Z RS Serious | Genesis8Female | /Pose Controls/Head/Expressions/Z Rebel Spirit | 控制器 | 是 |
| G81-1162 | Z RS Serious | head | /Pose Controls/Head/Expressions/Z Rebel Spirit | 别名 | 是 |
| G81-1163 | Z RS Smiley | Genesis8Female | /Pose Controls/Head/Expressions/Z Rebel Spirit | 控制器 | 是 |
| G81-1164 | Z RS Smiley | head | /Pose Controls/Head/Expressions/Z Rebel Spirit | 别名 | 是 |
| G81-1165 | Z SB Boredom | Genesis8Female | /Pose Controls/Head/Expressions/Z Simply Bored | 控制器 | 是 |
| G81-1166 | Z SB Boredom | head | /Pose Controls/Head/Expressions/Z Simply Bored | 别名 | 是 |
| G81-1167 | Z SB Disinterested | Genesis8Female | /Pose Controls/Head/Expressions/Z Simply Bored | 控制器 | 是 |
| G81-1168 | Z SB Disinterested | head | /Pose Controls/Head/Expressions/Z Simply Bored | 别名 | 是 |
| G81-1169 | Z SB Fed Up | Genesis8Female | /Pose Controls/Head/Expressions/Z Simply Bored | 控制器 | 是 |
| G81-1170 | Z SB Fed Up | head | /Pose Controls/Head/Expressions/Z Simply Bored | 别名 | 是 |
| G81-1171 | Z SB Tired | Genesis8Female | /Pose Controls/Head/Expressions/Z Simply Bored | 控制器 | 是 |
| G81-1172 | Z SB Tired | head | /Pose Controls/Head/Expressions/Z Simply Bored | 别名 | 是 |
| G81-1173 | Z SB Yawn | Genesis8Female | /Pose Controls/Head/Expressions/Z Simply Bored | 控制器 | 是 |
| G81-1174 | Z SB Yawn | head | /Pose Controls/Head/Expressions/Z Simply Bored | 别名 | 是 |
| G81-1175 | Z SC 01 Exhale | Genesis8Female | /Pose Controls/Head/Expressions/Z Smokers Club | 控制器 | 是 |
| G81-1176 | Z SC 01 Exhale | head | /Pose Controls/Head/Expressions/Z Smokers Club | 别名 | 是 |
| G81-1177 | Z SC 02 Blow Smoke | Genesis8Female | /Pose Controls/Head/Expressions/Z Smokers Club | 控制器 | 是 |
| G81-1178 | Z SC 02 Blow Smoke | head | /Pose Controls/Head/Expressions/Z Smokers Club | 别名 | 是 |
| G81-1179 | Z SC 03 Inhale | Genesis8Female | /Pose Controls/Head/Expressions/Z Smokers Club | 控制器 | 是 |
| G81-1180 | Z SC 03 Inhale | head | /Pose Controls/Head/Expressions/Z Smokers Club | 别名 | 是 |
| G81-1181 | Z SF !!Strong Female | Genesis8Female | /People/Real World/Z Strong Female | 控制器 | 是 |
| G81-1182 | Z SP Happy | Genesis8Female | /Pose Controls/Head/Expressions/Z Sweetie Pie | 控制器 | 是 |
| G81-1183 | Z SP Happy | head | /Pose Controls/Head/Expressions/Z Sweetie Pie | 别名 | 是 |
| G81-1184 | Z SP Happy Wink | Genesis8Female | /Pose Controls/Head/Expressions/Z Sweetie Pie | 控制器 | 是 |
| G81-1185 | Z SP Happy Wink | head | /Pose Controls/Head/Expressions/Z Sweetie Pie | 别名 | 是 |
| G81-1186 | Z SP In Love | Genesis8Female | /Pose Controls/Head/Expressions/Z Sweetie Pie | 控制器 | 是 |
| G81-1187 | Z SP In Love | head | /Pose Controls/Head/Expressions/Z Sweetie Pie | 别名 | 是 |
| G81-1188 | Z SP Surprise | Genesis8Female | /Pose Controls/Head/Expressions/Z Sweetie Pie | 控制器 | 是 |
| G81-1189 | Z SP Surprise | head | /Pose Controls/Head/Expressions/Z Sweetie Pie | 别名 | 是 |
| G81-1190 | Z SP Sweet Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Sweetie Pie | 控制器 | 是 |
| G81-1191 | Z SP Sweet Smile | head | /Pose Controls/Head/Expressions/Z Sweetie Pie | 别名 | 是 |
| G81-1192 | Z USD 02 Crying | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Sad and Depressed | 控制器 | 是 |
| G81-1193 | Z USD 02 Crying | head | /Pose Controls/Head/Expressions/Z Utility Sad and Depressed | 别名 | 是 |
| G81-1194 | Z USD Agonizing | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Sad and Depressed | 控制器 | 是 |
| G81-1195 | Z USD Agonizing | head | /Pose Controls/Head/Expressions/Z Utility Sad and Depressed | 别名 | 是 |
| G81-1196 | Z USD Sad | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Sad and Depressed | 控制器 | 是 |
| G81-1197 | Z USD Sad | head | /Pose Controls/Head/Expressions/Z Utility Sad and Depressed | 别名 | 是 |
| G81-1198 | Z USRL Appalled | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1199 | Z USRL Appalled | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1200 | Z USRL Arrogant Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1201 | Z USRL Arrogant Smile | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1202 | Z USRL Condescending Smile | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1203 | Z USRL Condescending Smile | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1204 | Z USRL Conversation | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1205 | Z USRL Conversation | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1206 | Z USRL Curious | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1207 | Z USRL Curious | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1208 | Z USRL Daydream | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1209 | Z USRL Daydream | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1210 | Z USRL Disappointed | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1211 | Z USRL Disappointed | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1212 | Z USRL Disgusted | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1213 | Z USRL Disgusted | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1214 | Z USRL Doubtful | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1215 | Z USRL Doubtful | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1216 | Z USRL Focused | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1217 | Z USRL Focused | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1218 | Z USRL Grimace | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1219 | Z USRL Grimace | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1220 | Z USRL Guilty Grin | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1221 | Z USRL Guilty Grin | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1222 | Z USRL Happy Scream | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1223 | Z USRL Happy Scream | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1224 | Z USRL I  Am On To You | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1225 | Z USRL I  Am On To You | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1226 | Z USRL In Deep Thought | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1227 | Z USRL In Deep Thought | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1228 | Z USRL In Disbelief | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1229 | Z USRL In Disbelief | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1230 | Z USRL Shocked | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1231 | Z USRL Shocked | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1232 | Z USRL Smile Silly | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1233 | Z USRL Smile Silly | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1234 | Z USRL Smile Subtle | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1235 | Z USRL Smile Subtle | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1236 | Z USRL Smile Sweet | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1237 | Z USRL Smile Sweet | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1238 | Z USRL Surprised Pout | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1239 | Z USRL Surprised Pout | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1240 | Z USRL Suspicious | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1241 | Z USRL Suspicious | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1242 | Z USRL Talking Loudly | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1243 | Z USRL Talking Loudly | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1244 | Z USRL Unsure | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1245 | Z USRL Unsure | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |
| G81-1246 | Z USRL Up To Something | Genesis8Female | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 控制器 | 是 |
| G81-1247 | Z USRL Up To Something | head | /Pose Controls/Head/Expressions/Z Utility Series Real Life | 别名 | 是 |

## 别名目标未解析（6 条）

该面板条目没有找到唯一可编辑目标。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1248 | HS Lurys Unhappy | head | /Pose Controls/Head/Expressions/Lurys Expression | 别名 | 是 |
| G81-1249 | Z FI 01 Terror G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G81-1250 | Z FI 02 Fright G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G81-1251 | Z FI 03 Distress G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G81-1252 | Z FI 04 Panic G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G81-1253 | Z FI 05 Dread G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |

## 下游参数受阻（12 条）

控制器自身存在，但实际驱动的下游仍受 HD 或引用问题影响。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1254 | Cheek Puff | Genesis8_1Female | /Pose Controls/Head/Cheek and Jaw | 控制器 | 是 |
| G81-1255 | GamerGirl Expression 07 | Genesis8Female | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 控制器 | 是 |
| G81-1256 | Liloo HD Details | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1257 | Liya | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1258 | LY Sterling | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1259 | Neko HD Details | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1260 | Paisley | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1261 | QX Ferin | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1262 | Satomi HD Details | Genesis8_1Female | /People/Stylized | 控制器 | 是 |
| G81-1263 | Shuang | Genesis8Female | /People/Real World | 控制器 | 是 |
| G81-1264 | Skin Crease Controller | Genesis8Female | /Full Body/Real World/Skin Folds &amp; Creases HD/Controllers | 控制器 | 是 |
| G81-1265 | Vo Xiao Hua HD Details LV4 | Genesis81FemaleGeom | /People/Real World | 控制器 | 是 |

## 没有有效输出（69 条）

当前角色内无有效输出；部分服装 / 睫毛项需跨对象驱动进一步验证。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1266 | AG Jenna Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1267 | Alice Eyelashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1268 | BGM Heavy Clothing Smoother | Genesis8Female | /Full Body/Real World | 控制器 | 是 |
| G81-1269 | BGM Heavy Clothing Smoother Skirts | Genesis8Female | /Full Body/Real World | 控制器 | 是 |
| G81-1270 | Body Shapes - Children Clothing Helper | Genesis8Female | /Full Body/Real World/Body Shapes Children | 控制器 | 是 |
| G81-1271 | Breast Helper Default Lower | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1272 | Breast Helper Default Upper | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1273 | Breast Helper Larger Lower | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1274 | Breast Helper Larger Upper | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1275 | Breast Helper Midzone Delumpifier 01 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1276 | Breast Helper Midzone Delumpifier 02 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1277 | Breast Helper Midzone Delumpifier 03 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1278 | Breast Helper Strap Lifter 01 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1279 | Breast Helper Strap Width | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G81-1280 | bs_EyelashesCollapsed | Genesis8_1Female | /Hidden/Utility | 控制器 | 否 |
| G81-1281 | Clothing Fit Helper Back Helper Lower | Genesis8Female | /Real World/Clothing Fit Helper | 控制器 | 是 |
| G81-1282 | DD Long Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1283 | DD Short Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1284 | Difa Eyelashes | Genesis8Female |  | 控制器 | 是 |
| G81-1285 | Eyelashes Long P3Design | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1286 | Eyelashes Natural | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1287 | Eyelashes Root Spread Irregular Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1288 | Eyelashes Root Spread Irregular Upper | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1289 | Fredda Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1290 | JA Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1291 | Kaoruko Eyelashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1292 | KrashWerks - Huan Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1293 | KrashWerks - KerriAnne Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1294 | KrashWerks - Sarah Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1295 | Long Lashes by FWArt | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1296 | Long Lashes by FWSA | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1297 | Long Lashes by Hinky Punk | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1298 | Long Lashes by Hinky Punk | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1299 | LY Long Lash | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1300 | MCMYujinLashesFit | Genesis8_1Female | /Hidden/People/Yujin | 控制器 | 否 |
| G81-1301 | MCMYujinLashesFitSmall | Genesis8Female | /Hidden/People/Yujin | 控制器 | 否 |
| G81-1302 | Mio Eyelashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1303 | NIPPLE INVERTED Do not require Nipple Base | Genesis8Female | /Sonsy 2025 Nipples | 控制器 | 是 |
| G81-1304 | Paisley Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1305 | RN Kayla Lashes 01 | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1306 | SASE Long Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1307 | SI Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1308 | TMLI Lashes 01 | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1309 | Vo Carin Lashes | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1310 | Vo Carin Natural Brows | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1311 | Vo Carin Young Natural Brows | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1312 | Vo Hai Yan Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1313 | Vo Hai Yan Natural Brows | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1314 | Vo Hataru Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1315 | Vo Hataru Natural Brows | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1316 | Vo Mieko Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1317 | Vo Ni Ni Lashes | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1318 | Vo Ni Ni Natural Brows | Genesis8_1Female | /Follower/Attachment/Head/Forehead/Eyebrows | 控制器 | 是 |
| G81-1319 | Vo Ni Ni Young Natural Brows | Genesis8_1Female | /Follower/Attachment/Head/Forehead/Eyebrows | 控制器 | 是 |
| G81-1320 | Vo Xiao Bei Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1321 | Vo Xiao Bei Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1322 | Vo Xiao Hua Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1323 | Vo Xiao Mei Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1324 | Vo Xiao Nan Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1325 | VO Xiao Xin Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1326 | VO Xiao Xin Lashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1327 | VO Xiao Xong Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1328 | Xia Lashes | Genesis8Female | /Real World | 控制器 | 否 |
| G81-1329 | Yue Eyelashes Down | Genesis8Female | /People/Stylized | 控制器 | 是 |
| G81-1330 | Z DAH G8F Tipsy | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G81-1331 | Z HCC Hoover Bend Morph | Genesis8Female | /Hidden/Correctives/Zeddicuss | 控制器 | 否 |
| G81-1332 | Z HCC Rubbish Under Rug | Genesis8Female | /Hidden/Correctives/Zeddicuss | 控制器 | 否 |
| G81-1333 | Z HCC Rug Hold Up | Genesis8Female | /Hidden/Correctives/Zeddicuss | 控制器 | 否 |
| G81-1334 | Zhi Lashes | Genesis8Female | /Real World | 控制器 | 否 |

## 骨骼属性别名尚未支持编辑（16 条）

Twist、Bend 等别名需要接入姿势输入与双向同步。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1335 | Bend | lShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1336 | Bend | lThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1337 | Bend | lWrist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1338 | Bend | rShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1339 | Bend | rThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1340 | Bend | rWrist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1341 | Front-Back | lShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1342 | Front-Back | rShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1343 | Side-Side | lThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1344 | Side-Side | rThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G81-1345 | Twist | lForeArm | /General/Transforms/Rotation | 别名 | 是 |
| G81-1346 | Twist | lShldr | /General/Transforms/Rotation | 别名 | 是 |
| G81-1347 | Twist | lThigh | /General/Transforms/Rotation | 别名 | 是 |
| G81-1348 | Twist | rForeArm | /General/Transforms/Rotation | 别名 | 是 |
| G81-1349 | Twist | rShldr | /General/Transforms/Rotation | 别名 | 是 |
| G81-1350 | Twist | rThigh | /General/Transforms/Rotation | 别名 | 是 |

## 目标顶点数不匹配（8 条）

明确的计数不匹配仍被拒绝，不能直接套用到当前角色。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1351 | Anakku Lashes | Genesis8Female | /Face/Eyes/Real World | 未验证形态 | 是 |
| G81-1352 | BendfixUpLowAbdoSKinFoldSH2LS | Genesis8Female | /Hidden/Correctives/Aging Morphs | 未验证形态 | 否 |
| G81-1353 | Legacy Eyelashes Curl | Genesis8_1Female | /Real World | 未验证形态 | 是 |
| G81-1354 | Legacy Eyelashes Hide Layer 1 | Genesis8_1Female | /Real World | 未验证形态 | 是 |
| G81-1355 | Legacy Eyelashes Hide Layer 2 | Genesis8_1Female | /Real World | 未验证形态 | 是 |
| G81-1356 | Legacy Eyelashes Top Point | Genesis8_1Female | /Real World | 未验证形态 | 是 |
| G81-1357 | Neck Wattle Single HD | Genesis8Female | /Full Body/Real World/Aging Morphs/Neck | 未验证形态 | 是 |
| G81-1358 | SFHD Chest Lower Bend Back Folds | Genesis8Female | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 未验证形态 | 是 |

## HD 尚未支持（414 条）

需要完整 HD / SubD 支持，包括部分名称中没有 HD 的形态。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1359 | !! NIPPLE BASE - Not needed if using Sonsy 2024 Nipples !! | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1360 | !! NIPPLE BASE Alternate Version BETTER | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1361 | Aged Body 1 Shape HD | geometry | /Full Body/Real World/Aging Morphs/Body | 稀疏形态 | 是 |
| G81-1362 | Aged Body 2 Assist HD | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-1363 | Aged Body 2 Shape HD | geometry | /Full Body/Real World/Aging Morphs/Body | 稀疏形态 | 是 |
| G81-1364 | Aging Arms Detail HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1365 | Aging Body Detail HD 1 | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1366 | Aging Body Detail HD 2 | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1367 | Aging Body Skin Base HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1368 | Aging Feet Detail HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1369 | Aging Hands Detail HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1370 | Alice Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1371 | Alice Liu Happy | geometry | /Pose Controls/Head/Expressions/Alice Liu Expressions | 稀疏形态 | 是 |
| G81-1372 | Alice Liu Sad | geometry | /Pose Controls/Head/Expressions/Alice Liu Expressions | 稀疏形态 | 是 |
| G81-1373 | Alice Liu Sweet | geometry | /Pose Controls/Head/Expressions/Alice Liu Expressions | 稀疏形态 | 是 |
| G81-1374 | Areola Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1375 | Areola Central Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1376 | Areola Edge Big Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1377 | Areola Edge Lines B | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1378 | Areola Edge Small Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1379 | Areola Gator Skin | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1380 | Areola Mounds | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1381 | Areola Puffy | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1382 | Areola Ridge Lines | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1383 | Athletic HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G81-1384 | Audrey Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1385 | Audrey Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1386 | Aurore Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1387 | Aurore Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1388 | Aurore Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1389 | Auto Life 100 Added Detail Hd | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1390 | AutoLife 100 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1391 | AutoLife 15 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1392 | AutoLife 30 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1393 | AutoLife 50 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1394 | AutoLife 75 Gen smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1395 | AutoLife_Age100 Skin HD | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1396 | AutoLife_Age30 Skin HD | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G81-1397 | Ayako Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1398 | Ayako Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1399 | Belly Cellulite HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1400 | Belly Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1401 | BGM Cellulite Belly HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1402 | BGM Cellulite Buttocks HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1403 | BGM Cellulite Thighs HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1404 | BGM Chin 01 | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1405 | BGM Chin 02 | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1406 | BGM Laura Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1407 | BGM Sarah Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1408 | BGM Stretch Marks Belly HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1409 | BGM Stretch Marks Hips HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1410 | BJ Caprica Body HD Details | Genesis81FemaleGeom | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1411 | BJ Caprica Head HD Details | Genesis81FemaleGeom | /People/Real World | 稀疏形态 | 是 |
| G81-1412 | BJ Imoen Body HD Details | Genesis81FemaleGeom | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1413 | BJ Imoen Head HD Details | Genesis81FemaleGeom | /People/Real World | 稀疏形态 | 是 |
| G81-1414 | Bold HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G81-1415 | Breast Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1416 | Brow Down Left | Genesis81FemaleGeom | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G81-1417 | Brow Down Left | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G81-1418 | Brow Down Right | Genesis81FemaleGeom | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G81-1419 | Brow Down Right | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G81-1420 | Brow Inner Up Left | Genesis81FemaleGeom | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G81-1421 | Brow Inner Up Left | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G81-1422 | Brow Inner Up Right | Genesis81FemaleGeom | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G81-1423 | Brow Inner Up Right | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G81-1424 | Brow Lateral Prominence HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G81-1425 | Brow Outer Up Left | Genesis81FemaleGeom | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G81-1426 | Brow Outer Up Left | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G81-1427 | Brow Outer Up Right | Genesis81FemaleGeom | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G81-1428 | Brow Outer Up Right | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G81-1429 | Brow Slack HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G81-1430 | Brow Superciliary Arch HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G81-1431 | Brow Thin HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G81-1432 | CapricaSmileHDLv2 | Genesis81FemaleGeom | /Hidden/People/Caprica | 仅HD | 否 |
| G81-1433 | CapricaSmileOpenHDLv2 | Genesis81FemaleGeom | /Hidden/People/Caprica | 稀疏形态 | 否 |
| G81-1434 | Carmelita Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1435 | Carmelita Elf Ears | geometry | /Fantasy SciFi | 稀疏形态 | 是 |
| G81-1436 | Casual HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G81-1437 | Cheek Acne Scars HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1438 | Cheek Puff Left | Genesis81FemaleGeom | /Pose Controls/Head/Cheek and Jaw | 稀疏形态 | 是 |
| G81-1439 | Cheek Puff Left | head | /Pose Controls/Head/Cheek and Jaw | 别名 | 是 |
| G81-1440 | Cheek Puff Right | Genesis81FemaleGeom | /Pose Controls/Head/Cheek and Jaw | 稀疏形态 | 是 |
| G81-1441 | Cheek Puff Right | head | /Pose Controls/Head/Cheek and Jaw | 别名 | 是 |
| G81-1442 | Cheek Squint Left | Genesis81FemaleGeom | /Pose Controls/Head/Cheek and Jaw | 稀疏形态 | 是 |
| G81-1443 | Cheek Squint Left | head | /Pose Controls/Head/Cheek and Jaw | 别名 | 是 |
| G81-1444 | Cheek Squint Right | Genesis81FemaleGeom | /Pose Controls/Head/Cheek and Jaw | 稀疏形态 | 是 |
| G81-1445 | Cheek Squint Right | head | /Pose Controls/Head/Cheek and Jaw | 别名 | 是 |
| G81-1446 | Cheekbones Arch Size HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1447 | Cheekbones Depression HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1448 | Cheeks Slack HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1449 | Chin Acne Scars HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1450 | Chin Cleft HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1451 | Chin Crease HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1452 | Chin Wrinkles HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1453 | CNB Angelalova Body HD Details | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1454 | CNB Angelalova Head HD Details | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |
| G81-1455 | CNB DI Yangon Win Body HD Details | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1456 | CNB DI Yangon Win Head HD Details | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |
| G81-1457 | CNB Rainy Koo Body HD Details | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1458 | CNB Rainy Koo Head HD Details | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |
| G81-1459 | Crows Feet HD Left | geometry | /Real World | 稀疏形态 | 是 |
| G81-1460 | Crows Feet HD Right | geometry | /Real World | 稀疏形态 | 是 |
| G81-1461 | Curvy HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G81-1462 | DD Isabelle Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1463 | DD Isabelle Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-1464 | DD Nails | geometry | /Real World | 稀疏形态 | 是 |
| G81-1465 | DD Nails - Square | geometry | /Real World | 稀疏形态 | 是 |
| G81-1466 | DD Nails - Squoval | geometry | /Real World | 稀疏形态 | 是 |
| G81-1467 | DD Nails - Stiletto | geometry | /Real World | 稀疏形态 | 是 |
| G81-1468 | eJCMGabriela8_ForeheadWrinkleL_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-1469 | eJCMGabriela8_ForeheadWrinkleR_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G81-1470 | eJCMMrsChow8_ForeheadWrinkleL_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-1471 | eJCMMrsChow8_ForeheadWrinkleR_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G81-1472 | Elsie Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1473 | Eye Bags HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G81-1474 | Eye Look In Left | Genesis81FemaleGeom | /Pose Controls/Head/Eyes/Lids | 稀疏形态 | 是 |
| G81-1475 | Eye Look In Left | head | /Pose Controls/Head/Eyes/Lids | 别名 | 是 |
| G81-1476 | Eye Look In Right | Genesis81FemaleGeom | /Pose Controls/Head/Eyes/Lids | 稀疏形态 | 是 |
| G81-1477 | Eye Look In Right | head | /Pose Controls/Head/Eyes/Lids | 别名 | 是 |
| G81-1478 | Eye Look Out Left | Genesis81FemaleGeom | /Pose Controls/Head/Eyes/Lids | 稀疏形态 | 是 |
| G81-1479 | Eye Look Out Left | head | /Pose Controls/Head/Eyes/Lids | 别名 | 是 |
| G81-1480 | Eye Look Out Right | Genesis81FemaleGeom | /Pose Controls/Head/Eyes/Lids | 稀疏形态 | 是 |
| G81-1481 | Eye Look Out Right | head | /Pose Controls/Head/Eyes/Lids | 别名 | 是 |
| G81-1482 | Eye Rim Refine HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1483 | Eye Side Wrinkles HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G81-1484 | Eye Squint Left | Genesis81FemaleGeom | /Pose Controls/Head/Eyes | 稀疏形态 | 是 |
| G81-1485 | Eye Squint Left | head | /Pose Controls/Head/Eyes | 别名 | 是 |
| G81-1486 | Eye Squint Right | Genesis81FemaleGeom | /Pose Controls/Head/Eyes | 稀疏形态 | 是 |
| G81-1487 | Eye Squint Right | head | /Pose Controls/Head/Eyes | 别名 | 是 |
| G81-1488 | Eyebrow Centre Folds HD | geometry | /Real World/Aging Morphs/Brow &amp; Forehead | 稀疏形态 | 是 |
| G81-1489 | Eyelids Lower Crease HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1490 | Face Age Shape HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1491 | Face Base Detail HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1492 | Face Base Skin HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1493 | Face Fine Wrinkles HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G81-1494 | facs_cbs_BDL_BIUL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1495 | facs_cbs_BDR_BIUR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1496 | facs_cbs_BIUL_BOUL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1497 | facs_cbs_BIUR_BOUR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1498 | facs_cbs_EBL_BIUL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1499 | facs_cbs_EBL_BOUL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1500 | facs_cbs_EBL_CSL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1501 | facs_cbs_EBL_ELDL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1502 | facs_cbs_EBL_ELIL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1503 | facs_cbs_EBL_ELOL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1504 | facs_cbs_EBL_ELUL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1505 | facs_cbs_EBL_ESL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1506 | facs_cbs_EBL_NSL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1507 | facs_cbs_EBR_BIUR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1508 | facs_cbs_EBR_BOUR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1509 | facs_cbs_EBR_CSR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1510 | facs_cbs_EBR_ELDR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1511 | facs_cbs_EBR_ELIR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1512 | facs_cbs_EBR_ELOR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1513 | facs_cbs_EBR_ELUR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1514 | facs_cbs_EBR_ESR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1515 | facs_cbs_EBR_NSR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1516 | facs_cbs_EyeBlinkLeft_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1517 | facs_cbs_EyeBlinkRight_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1518 | facs_cbs_EyeLookDownLeft_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1519 | facs_cbs_EyeLookDownRight_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1520 | facs_cbs_EyeLookUpLeft_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1521 | facs_cbs_EyeLookUpRight_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1522 | facs_cbs_EyeWideLeft_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1523 | facs_cbs_EyeWideRight_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1524 | facs_cbs_JawForward_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1525 | facs_cbs_JawLeft_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1526 | facs_cbs_JawOpen_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1527 | facs_cbs_JawRecess_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1528 | facs_cbs_JawRight_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1529 | facs_cbs_JO_JL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1530 | facs_cbs_JO_JR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1531 | facs_cbs_JO_MF_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1532 | facs_cbs_JO_ML_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1533 | facs_cbs_JO_MR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1534 | facs_cbs_MouthPuckerDetails_div2 | Genesis81FemaleGeom | /Hidden/FACS/Additives | 稀疏形态 | 否 |
| G81-1535 | facs_cbs_MP_CPL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1536 | facs_cbs_MP_CPR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1537 | facs_cbs_MP_MC_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1538 | facs_cbs_MP_MF_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1539 | facs_cbs_MSL_MDL_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1540 | facs_cbs_MSR_MDR_div2 | Genesis81FemaleGeom | /Hidden/FACS/Fixes | 稀疏形态 | 否 |
| G81-1541 | Forehead Wrinkle HD Left | geometry | /Real World | 稀疏形态 | 是 |
| G81-1542 | Forehead Wrinkle HD Right | geometry | /Real World | 稀疏形态 | 是 |
| G81-1543 | Forehead Wrinkles HD | geometry | /Real World/Aging Morphs/Brow &amp; Forehead | 稀疏形态 | 是 |
| G81-1544 | GamerGirl Expression 01 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G81-1545 | Giada Body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G81-1546 | Giada Head | geometry | /People/Stylized | 稀疏形态 | 是 |
| G81-1547 | Glute Cellulite Left HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1548 | Glute Cellulite Left HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1549 | Glute Cellulite Right HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1550 | Glute Cellulite Right HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1551 | Glute Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1552 | GraceYong Chest Upper Shape HDlv4 | geometry | /Hidden/People/PCGraceYong/PBMs | 稀疏形态 | 否 |
| G81-1553 | Hailey Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1554 | Hailey Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1555 | Hip Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1556 | ImoenSmileHDLv2 | Genesis81FemaleGeom | /Hidden/People/Imoen | 仅HD | 否 |
| G81-1557 | JA Clary Body | Genesis8Female | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G81-1558 | JA Clary Head | Genesis8Female | /People/Stylized | 稀疏形态 | 是 |
| G81-1559 | Jaw Slack HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1560 | JCMDDGamerGirlPose01 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G81-1561 | JCMDDGamerGirlPose02 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G81-1562 | JCMDDGamerGirlPose03 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G81-1563 | JCMDDGamerGirlPose08 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G81-1564 | Jowls 1 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1565 | Jowls 2 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1566 | Laugh Lines 1 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1567 | Laugh Lines 2 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1568 | Laugh Lines 3 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1569 | Laugh Lines 4 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1570 | Leony Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1571 | Leony Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1572 | Leony Navel HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1573 | Liloo Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1574 | Liloo Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1575 | Lips Contour HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1576 | Long Nails by FWArt | geometry | /Real World | 稀疏形态 | 是 |
| G81-1577 | Longer Nails | geometry | /Real World | 稀疏形态 | 是 |
| G81-1578 | LY Sterling Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1579 | LY Sterling Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-1580 | LY Sterling Head Asymmetry | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-1581 | Marionette Lines HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1582 | MCM_LY_Sterling_Left | geometry | /Hidden/Correctives/Lyoness/Sterling | 稀疏形态 | 否 |
| G81-1583 | MCMCarynHD_Chubby | geometry | /Hidden/People/Caryn/MCMs | 稀疏形态 | 否 |
| G81-1584 | MCMDDPaisleyEyesClosedR-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G81-1585 | MCMGabriela8_MouthRealism_HDLv3 | geometry | /Hidden/People/Gabriela 8/MCMs | 稀疏形态 | 否 |
| G81-1586 | MCMGabriela8_Navel_HDLv4 | geometry | /Hidden/People/Gabriela 8/MCMs | 稀疏形态 | 否 |
| G81-1587 | MCMGabriela8_Nipples_HDLv4 | geometry | /Hidden/People/Gabriela 8/MCMs | 稀疏形态 | 否 |
| G81-1588 | MCMGGExpression09FIX | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G81-1589 | MCMMrsChow8_MouthRealism_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/MCMs | 稀疏形态 | 否 |
| G81-1590 | MCMMrsChow8_Navel_HDLv4 | geometry | /Hidden/People/Mrs Chow 8/MCMs | 稀疏形态 | 否 |
| G81-1591 | MCMMrsChow8_Nipples_HDLv4 | geometry | /Hidden/People/Mrs Chow 8/MCMs | 稀疏形态 | 否 |
| G81-1592 | MCMNyaoSquareNailsFIX | Genesis8Female | /Hidden/Correctives | 稀疏形态 | 否 |
| G81-1593 | MCMNyaoSquovalNailsFIX | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G81-1594 | MCMSayaNavelLv4 | geometry | /Hidden/People/Saya/MCMs | 稀疏形态 | 否 |
| G81-1595 | MCMSayaNipplesLv4 | geometry | /Hidden/People/Saya/MCMs | 稀疏形态 | 否 |
| G81-1596 | Minami HD Body Details | Genesis8Female | /Full Body/People/Real World | 仅HD | 是 |
| G81-1597 | Mouth Close | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1598 | Mouth Close | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1599 | Mouth Dimple Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1600 | Mouth Dimple Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1601 | Mouth Dimple Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1602 | Mouth Dimple Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1603 | Mouth Frown Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1604 | Mouth Frown Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1605 | Mouth Frown Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1606 | Mouth Frown Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1607 | Mouth Funnel | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1608 | Mouth Funnel | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1609 | Mouth Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1610 | Mouth Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1611 | Mouth Lower Down Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1612 | Mouth Lower Down Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1613 | Mouth Lower Down Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1614 | Mouth Lower Down Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1615 | Mouth Press Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1616 | Mouth Press Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1617 | Mouth Press Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1618 | Mouth Press Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1619 | Mouth Pucker | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1620 | Mouth Pucker | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1621 | Mouth Realism 8.1 HD | Genesis81FemaleGeom | /Real World | 稀疏形态 | 是 |
| G81-1622 | Mouth Realism HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1623 | Mouth Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1624 | Mouth Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1625 | Mouth Roll Lower | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1626 | Mouth Roll Lower | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1627 | Mouth Roll Upper | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1628 | Mouth Roll Upper | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1629 | Mouth Shrug Lower | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1630 | Mouth Shrug Lower | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1631 | Mouth Shrug Upper | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1632 | Mouth Shrug Upper | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1633 | Mouth Smile Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1634 | Mouth Smile Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1635 | Mouth Smile Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1636 | Mouth Smile Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1637 | Mouth Stretch Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1638 | Mouth Stretch Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1639 | Mouth Stretch Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1640 | Mouth Stretch Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1641 | Mouth Upper Up Left | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1642 | Mouth Upper Up Left | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1643 | Mouth Upper Up Right | Genesis81FemaleGeom | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G81-1644 | Mouth Upper Up Right | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-1645 | Mouth Wrinkles HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G81-1646 | MPLeonyOutfitPants | geometry | /Clothing/OOT Micro Pressure | 稀疏形态 | 是 |
| G81-1647 | MPLeonyOutfitTop | geometry | /Clothing/OOT Micro Pressure | 稀疏形态 | 是 |
| G81-1648 | MPVelocityOutfitBra | geometry | /Full Body/OOT Micro Pressure | 稀疏形态 | 是 |
| G81-1649 | MPVelocityOutfitPants | geometry | /Full Body/OOT Micro Pressure | 稀疏形态 | 是 |
| G81-1650 | Naomi HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1651 | Naomi Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1652 | Nasal Flare | Genesis81FemaleGeom | /Pose Controls/Head/Nose | 稀疏形态 | 是 |
| G81-1653 | Nasal Flare | head | /Pose Controls/Head/Nose | 别名 | 是 |
| G81-1654 | Neck Detail 1 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1655 | Neck Detail 2 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1656 | Neck Detail 3 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1657 | Neck Detail 4 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1658 | Neck Fine Wrinkles HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1659 | Neck Wattle Double HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1660 | Neck Wrinkles HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G81-1661 | Neko Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1662 | Neko Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1663 | Neko Navel HD | geometry | /Real World | 稀疏形态 | 是 |
| G81-1664 | Nipple Base Bend Adjustment Fix | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1665 | Nipple Bigger HD | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1666 | Nipple Fatter | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1667 | Nipple Inverted 1 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1668 | Nipple Inverted 2 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1669 | Nipple Longer | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1670 | Nipple Realism | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1671 | Nipple Realism OPTION 2 Pointy | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1672 | Nose Pores HD | geometry | /Real World/Aging Morphs/Nose | 稀疏形态 | 是 |
| G81-1673 | Nose Sneer Left | Genesis81FemaleGeom | /Pose Controls/Head/Nose | 稀疏形态 | 是 |
| G81-1674 | Nose Sneer Left | head | /Pose Controls/Head/Nose | 别名 | 是 |
| G81-1675 | Nose Sneer Right | Genesis81FemaleGeom | /Pose Controls/Head/Nose | 稀疏形态 | 是 |
| G81-1676 | Nose Sneer Right | head | /Pose Controls/Head/Nose | 别名 | 是 |
| G81-1677 | Nose Wrinkles HD | geometry | /Real World/Aging Morphs/Nose | 稀疏形态 | 是 |
| G81-1678 | Nyao Body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G81-1679 | Nyao Body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G81-1680 | Nyao Head | geometry | /People/Stylized | 稀疏形态 | 是 |
| G81-1681 | Nylon Pantyhose-Waist-Squeezing1 | geometry | /Clothing/Nylon Pantyhose | 稀疏形态 | 是 |
| G81-1682 | Nylon Pantyhose-Waist-Squeezing2 | geometry | /Clothing/Nylon Pantyhose | 稀疏形态 | 是 |
| G81-1683 | Nylon Pantyhose-Waist-Squeezing3 | geometry | /Clothing/Nylon Pantyhose | 稀疏形态 | 是 |
| G81-1684 | P3D Mirai Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1685 | Paisley Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1686 | Paisley Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-1687 | PBMAuroreNavelHDlv4 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-1688 | PBMAuroreNipplesHDlv4 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G81-1689 | pJCMGabriela8_HeadTwistL_HDLv3 | geometry | /Hidden/People/Gabriela 8/pJCMs | 稀疏形态 | 否 |
| G81-1690 | pJCMGabriela8_HeadTwistR_HDLv3 | geometry | /Hidden/People/Gabriela 8/pJCMs | 稀疏形态 | 否 |
| G81-1691 | pJCMMrsChow8_Head_TwistL_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/pJCMs | 稀疏形态 | 否 |
| G81-1692 | pJCMMrsChow8_Head_TwistR_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/pJCMs | 稀疏形态 | 否 |
| G81-1693 | Raised Cornea | geometry | /Real World | 稀疏形态 | 是 |
| G81-1694 | RN Fredda Body HD Details | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1695 | RN Fredda Head HD Details | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-1696 | RN Kayla Body HD Details | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1697 | RN Kayla Head HD Details | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G81-1698 | RN Kayla Mouth HD Details | Genesis8Female | /Real World | 稀疏形态 | 是 |
| G81-1699 | RN Minto 8.1 Body HD Details Lv4 | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1700 | RN Minto 8.1 Head HD Details Lv4 | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |
| G81-1701 | Rosabell HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1702 | RS Minto 8.1 Simle Expression HD | Genesis8_1Female | /Pose Controls/Head | 稀疏形态 | 是 |
| G81-1703 | Satomi Body HD Details | Genesis81FemaleGeom | /Full Body/People/Stylized | 仅HD | 是 |
| G81-1704 | Satomi Dragon Maiden Ears HD | Genesis8_1Female | /Stylized | 稀疏形态 | 是 |
| G81-1705 | Satomi Dragon Maiden Horn HD | Genesis81FemaleGeom | /People/Stylized | 稀疏形态 | 是 |
| G81-1706 | Satomi Head HD Details | Genesis81FemaleGeom | /People/Stylized | 仅HD | 是 |
| G81-1707 | Saya HD Details | geometry | /People/Real World | 仅HD | 是 |
| G81-1708 | SFHD Chest Lower Bend Front Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G81-1709 | SFHD Chest Lower Bend Front Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 稀疏形态 | 是 |
| G81-1710 | SFHD Chest Lower Left Side Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Left Side | 稀疏形态 | 是 |
| G81-1711 | SFHD Chest Lower Right Side Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Right Side | 稀疏形态 | 是 |
| G81-1712 | SFHD Collar Back Both Crease | geometry | /Hidden/Skin Folds &amp; Creases HD/Crease | 稀疏形态 | 否 |
| G81-1713 | SFHD Collar Left Back Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-1714 | SFHD Collar Right Back Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-1715 | SFHD Neck Bend Front Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G81-1716 | SFHD Neck Bend Side Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-1717 | SFHD Neck Bend Side Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-1718 | SFHD Shoulder Bend Back Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-1719 | SFHD Shoulder Bend Back Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-1720 | SFHD Torso Bend Back Lower Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G81-1721 | SFHD Torso Bend Back Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G81-1722 | SFHD Torso Bend Back Upper Crease Assist 1 | geometry | /Hidden/Skin Folds &amp; Creases HD/Crease | 稀疏形态 | 否 |
| G81-1723 | SFHD Torso Bend Back Upper Crease Assist 2 | geometry | /Hidden/Skin Folds &amp; Creases HD/Crease | 稀疏形态 | 否 |
| G81-1724 | SFHD Torso Bend Back Upper Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 稀疏形态 | 是 |
| G81-1725 | SFHD Torso Bend Front Lower Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G81-1726 | SFHD Torso Bend Front Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G81-1727 | SFHD Torso Bend Front Upper Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 稀疏形态 | 是 |
| G81-1728 | SFHD Torso Bend Left Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-1729 | SFHD Torso Bend Right Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-1730 | SFHD Torso Bend Side Upper Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G81-1731 | SFHD Torso Bend Side Upper Left Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Left Side | 稀疏形态 | 是 |
| G81-1732 | SFHD Torso Bend Side Upper Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G81-1733 | SFHD Torso Bend Side Upper Right Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Right Side | 稀疏形态 | 是 |
| G81-1734 | Shizuka HD Details | geometry | /People/Stylized | 稀疏形态 | 是 |
| G81-1735 | Shuang Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1736 | Slim HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G81-1737 | Spine HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1738 | Taryn Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1739 | Taryn Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1740 | Thigh Cellulite Left HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1741 | Thigh Cellulite Left HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1742 | Thigh Cellulite Right HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1743 | Thigh Cellulite Right HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1744 | Thigh Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1745 | Tip Bumps Texture | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1746 | Tip Hole 1 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1747 | Tip Hole 2 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1748 | Tip Hole Deep | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1749 | Tip Hole Horizontal | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1750 | Tip Hole Lip Shape | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1751 | Tip Hole Star Shape | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1752 | Tip Hole Vertical | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1753 | Tip Hole Y Shape | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G81-1754 | Under Eye 1 HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G81-1755 | Under Eye 2 HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G81-1756 | Under Eye 3 HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G81-1757 | Under Eye Wrinkles HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G81-1758 | Under Mouth Slack HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G81-1759 | Varicose Veins Left Leg Less HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1760 | Varicose Veins Left Leg More HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1761 | Varicose Veins Right Leg Less HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1762 | Varicose Veins Right Leg More HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G81-1763 | Vo Hai Yan HD Body Details LV4 | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1764 | Vo Hai Yan HD Head Details LV4 | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |
| G81-1765 | Vo Xiao Hua HD Body Details LV4 | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1766 | Vo Xiao Hua HD Head Details LV4 | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |
| G81-1767 | Willa Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1768 | Willa Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1769 | Xia Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1770 | Xia Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G81-1771 | Yamazaki Body HD Details | Genesis8_1Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G81-1772 | Yamazaki Head HD Details | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |

## 资产明确锁定（193 条）

不允许手动编辑；它可能仍由公式驱动，不等于求值失效，也不计作待修复故障。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1773 | Aging Shape Blender 1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-1774 | CTRLMD_N_CTRLMD_N_YRotate_110_1 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1775 | CTRLMD_N_Value_0_8429 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1776 | CTRLMD_N_Value_0_9082 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1777 | CTRLMD_N_Value_0_942 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1778 | CTRLMD_N_Value_0_9517 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1779 | CTRLMD_N_Value_1 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1780 | CTRLMD_N_Value_n0_5 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1781 | CTRLMD_N_Value_n1 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1782 | CTRLMD_N_XRotate_110 | lShin | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1783 | CTRLMD_N_XRotate_110 | rShin | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1784 | CTRLMD_N_XRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1785 | CTRLMD_N_XRotate_24 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1786 | CTRLMD_N_XRotate_24 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1787 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1788 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1789 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1790 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1791 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1792 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1793 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1794 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1795 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1796 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1797 | CTRLMD_N_XRotate_35 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1798 | CTRLMD_N_XRotate_35 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1799 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1800 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1801 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1802 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1803 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1804 | CTRLMD_N_XRotate_n115 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1805 | CTRLMD_N_XRotate_n115 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1806 | CTRLMD_N_XRotate_n20 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1807 | CTRLMD_N_XRotate_n25 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1808 | CTRLMD_N_XRotate_n25 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1809 | CTRLMD_N_XRotate_n25 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1810 | CTRLMD_N_XRotate_n25 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1811 | CTRLMD_N_XRotate_n25 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1812 | CTRLMD_N_XRotate_n25 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1813 | CTRLMD_N_XRotate_n27 | head | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1814 | CTRLMD_N_XRotate_n27 | head | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1815 | CTRLMD_N_XRotate_n27 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1816 | CTRLMD_N_XRotate_n27 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1817 | CTRLMD_N_XRotate_n70 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1818 | CTRLMD_N_XRotate_n70 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1819 | CTRLMD_N_XRotate_n90 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1820 | CTRLMD_N_XRotate_n90 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1821 | CTRLMD_N_XRotate_n90 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1822 | CTRLMD_N_XRotate_n90 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1823 | CTRLMD_N_YRotate_110 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1824 | CTRLMD_N_YRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1825 | CTRLMD_N_YRotate_15 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1826 | CTRLMD_N_YRotate_17 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1827 | CTRLMD_N_YRotate_20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1828 | CTRLMD_N_YRotate_20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1829 | CTRLMD_N_YRotate_22 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1830 | CTRLMD_N_YRotate_40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1831 | CTRLMD_N_YRotate_40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1832 | CTRLMD_N_YRotate_40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1833 | CTRLMD_N_YRotate_90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1834 | CTRLMD_N_YRotate_90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1835 | CTRLMD_N_YRotate_n110 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1836 | CTRLMD_N_YRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1837 | CTRLMD_N_YRotate_n15 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1838 | CTRLMD_N_YRotate_n17 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1839 | CTRLMD_N_YRotate_n20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1840 | CTRLMD_N_YRotate_n20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1841 | CTRLMD_N_YRotate_n22 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1842 | CTRLMD_N_YRotate_n40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1843 | CTRLMD_N_YRotate_n40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1844 | CTRLMD_N_YRotate_n40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1845 | CTRLMD_N_YRotate_n90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1846 | CTRLMD_N_YRotate_n90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1847 | CTRLMD_N_ZRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1848 | CTRLMD_N_ZRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1849 | CTRLMD_N_ZRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1850 | CTRLMD_N_ZRotate_20 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1851 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1852 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1853 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1854 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1855 | CTRLMD_N_ZRotate_35 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1856 | CTRLMD_N_ZRotate_40 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1857 | CTRLMD_N_ZRotate_40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1858 | CTRLMD_N_ZRotate_45 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1859 | CTRLMD_N_ZRotate_50 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1860 | CTRLMD_N_ZRotate_50 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1861 | CTRLMD_N_ZRotate_55 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1862 | CTRLMD_N_ZRotate_60 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1863 | CTRLMD_N_ZRotate_70 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1864 | CTRLMD_N_ZRotate_80 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1865 | CTRLMD_N_ZRotate_85 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1866 | CTRLMD_N_ZRotate_85 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1867 | CTRLMD_N_ZRotate_85 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1868 | CTRLMD_N_ZRotate_90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1869 | CTRLMD_N_ZRotate_90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1870 | CTRLMD_N_ZRotate_90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1871 | CTRLMD_N_ZRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1872 | CTRLMD_N_ZRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1873 | CTRLMD_N_ZRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1874 | CTRLMD_N_ZRotate_n20 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1875 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1876 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1877 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1878 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1879 | CTRLMD_N_ZRotate_n35 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1880 | CTRLMD_N_ZRotate_n40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1881 | CTRLMD_N_ZRotate_n40 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1882 | CTRLMD_N_ZRotate_n45 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1883 | CTRLMD_N_ZRotate_n50 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1884 | CTRLMD_N_ZRotate_n50 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1885 | CTRLMD_N_ZRotate_n55 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1886 | CTRLMD_N_ZRotate_n60 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1887 | CTRLMD_N_ZRotate_n70 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1888 | CTRLMD_N_ZRotate_n80 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1889 | CTRLMD_N_ZRotate_n85 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1890 | CTRLMD_N_ZRotate_n85 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1891 | CTRLMD_N_ZRotate_n85 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1892 | CTRLMD_N_ZRotate_n90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1893 | CTRLMD_N_ZRotate_n90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1894 | CTRLMD_N_ZRotate_n90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G81-1895 | Gums Smoother | Genesis8Female | /Hidden/Correctives/Aging Morphs | 控制器 | 否 |
| G81-1896 | JCMYouthChestAssist | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-1897 | JCMYouthMorph Assist | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-1898 | JCMYouthMouth | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-1899 | Liloo Ears | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-1900 | Liloo Head Neutralizer Assist | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-1901 | Liloo Mouth Realism Null | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G81-1902 | SFHD Abdo L &amp; Pelvis Bend | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1903 | SFHD Abdo L &amp; Pelvis Bend Null | Genesis8Female | /Hidden/Skin Folds &amp; Creases HD/Base | 控制器 | 否 |
| G81-1904 | SFHD Abdo L &amp; U &amp; Pelvis Bend | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1905 | SFHD Abdo L &amp; U BS2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1906 | SFHD Abdo L &amp; U BS2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1907 | SFHD Abdo L &amp; U Twist &amp; U Bend Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1908 | SFHD Abdo L &amp; U Twist &amp; U Bend Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1909 | SFHD Abdo L &amp; U Twist Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1910 | SFHD Abdo L &amp; U Twist Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1911 | SFHD Abdo L BS2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1912 | SFHD Abdo L BS2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1913 | SFHD Abdo L S2S &amp; BT Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1914 | SFHD Abdo L S2S &amp; BT Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1915 | SFHD Abdo L S2SBT &amp; US2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1916 | SFHD Abdo L S2SBT &amp; US2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1917 | SFHD Abdo U &amp; L BBT Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1918 | SFHD Abdo U &amp; L BBT Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1919 | SFHD Abdo U BS2S &amp; BT Left Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1920 | SFHD Abdo U BS2S &amp; BT Right Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1921 | SFHD Abdo U BS2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1922 | SFHD Abdo U BS2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1923 | SFHD Abdo U BT Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1924 | SFHD Abdo U BT Left Null | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1925 | SFHD Abdo U BT OP Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1926 | SFHD Abdo U BT OP Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1927 | SFHD Abdo U BT Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1928 | SFHD Abdo U BT Right Null | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1929 | SFHD Arm Bend Back Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1930 | SFHD Arm Bend Back Left Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1931 | SFHD Arm Bend Back Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1932 | SFHD Arm Bend Back Right Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1933 | SFHD Chest Lower Bend Front Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1934 | SFHD Chest Lower Bend Front Left Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1935 | SFHD Chest Lower Bend Front Right Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1936 | SFHD Navel Fold | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1937 | SFHD Navel Fold Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1938 | SFHD Neck Twist Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1939 | SFHD Neck Twist Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1940 | SFHD Pelvis &amp; Abdo L Twist Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1941 | SFHD Pelvis &amp; Abdo L Twist Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G81-1942 | SFHD Pelvis Bend Front Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1943 | SFHD Torso Bend Front Lower Navel Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1944 | SFHD Torso Bend Front U &amp; L &amp; P Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1945 | SFHD Torso Bend Front U &amp; L Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1946 | SFHD Torso Bend Front Upper &amp; SL Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1947 | SFHD Torso Bend Front Upper &amp; SR Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1948 | SFHD Torso Bend Side Lower Left &amp; U Abdo Bend Front Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1949 | SFHD Torso Bend Side Lower Right &amp; U Abdo Bend Front Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1950 | SFHD Torso Bend Side U &amp; L &amp; LF Left Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1951 | SFHD Torso Bend Side U &amp; L &amp; LF Right Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1952 | SFHD Torso Bend Side Upper Left &amp; Back Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1953 | SFHD Torso Bend Side Upper Left &amp; Twist Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1954 | SFHD Torso Bend Side Upper Left &amp; Twist Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1955 | SFHD Torso Bend Side Upper Left Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1956 | SFHD Torso Bend Side Upper Right &amp; Back Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1957 | SFHD Torso Bend Side Upper Right &amp; Twist Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1958 | SFHD Torso Bend Side Upper Right &amp; Twist Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1959 | SFHD Torso Bend Side Upper Right Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1960 | SFHD Torso Lower Bulge Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1961 | SFHD Torso Upper Bulge Folds 1 | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1962 | SFHD Torso Upper Bulge Folds 2 | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G81-1963 | SH1&amp;2GenSmoother | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G81-1964 | Youth Head Neutralize | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G81-1965 | Youth Head Neutralize Assist | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |

## 单独取样无明显形变（63 条）

在本次新增可编辑条目的测试中，单独输入 0.5 / 1（受参数范围限制）时，最大顶点位移小于 1e-6 米。可能需要配套体型、姿势或开关；不能据此断言参数永久无效。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G81-1966 | Auto Life Body Shape Neutralize | Genesis8Female | /Full Body/Real World/Auto Life/Age | 稀疏形态 | 是 |
| G81-1967 | AutoLife_AFE_01smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1968 | AutoLife_AFE_05smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1969 | AutoLife_AFE_0smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1970 | AutoLife_AFE_100smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1971 | AutoLife_AFE_16smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1972 | AutoLife_AFE_30smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1973 | AutoLife_AFE_60smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1974 | AutoLife_AFE_80smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1975 | AutoLife_AFE_G8F30sm | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G81-1976 | CNB DI Yangon Win Eyelashes Shape | Genesis8_1Female | /Hidden/People/CNBYWDI/PHMs | 控制器 | 是 |
| G81-1977 | CNB Rainy Koo HD Details | Genesis8_1Female | /People/Real World | 控制器 | 是 |
| G81-1978 | DisparateLashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1979 | DT- Luna Eyelashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1980 | EJ Eyelashes 01 | Genesis8Female | /Hidden/EmmaAndJordi/Common | 控制器 | 否 |
| G81-1981 | Eyelashes Curl Irregular Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1982 | Eyelashes Curl Irregular Lower Point | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1983 | Eyelashes Curl Irregular Upper | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1984 | Eyelashes Curl Irregular Upper Point | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1985 | Eyelashes Curl Uniform Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1986 | Eyelashes Height Uniform Upper | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1987 | Eyelashes Length Irregular Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1988 | Eyelashes Length Uniform Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1989 | Eyelashes Length Uniform Upper | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1990 | Eyelashes Skew Irregular Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1991 | Eyelashes Skew Irregular Upper | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1992 | Legacy Eyelashes Length Lower | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1993 | Legacy Eyelashes Length Upper | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-1994 | Long Lashes by Hinky Punk | Genesis8Female | /Real World | 控制器 | 是 |
| G81-1995 | MCMArinaEyeBlinkL | Genesis8_1Female | /Hidden/People/Arina/MCMs | 稀疏形态 | 否 |
| G81-1996 | MCMArinaEyeBlinkR | Genesis8_1Female | /Hidden/People/Arina/MCMs | 稀疏形态 | 否 |
| G81-1997 | MCMJiEunEyeBlinkL | Genesis8_1Female | /Hidden/People/Ji Eun/MCMs | 稀疏形态 | 否 |
| G81-1998 | MCMJiEunEyeBlinkR | Genesis8_1Female | /Hidden/People/Ji Eun/MCMs | 稀疏形态 | 否 |
| G81-1999 | MCMKarenKaedeEyeBlinkL | Genesis8_1Female | /Hidden/People/Karen Kaede/MCMs | 稀疏形态 | 否 |
| G81-2000 | MCMKarenKaedeEyeBlinkR | Genesis8_1Female | /Hidden/People/Karen Kaede/MCMs | 稀疏形态 | 否 |
| G81-2001 | MCMKimSeohyunLashesFix | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G81-2002 | MCMMomonogiKanaEyeBlinkL | Genesis8_1Female | /Hidden/People/Momonogi Kana/MCMs | 稀疏形态 | 否 |
| G81-2003 | MCMMomonogiKanaEyeBlinkR | Genesis8_1Female | /Hidden/People/Momonogi Kana/MCMs | 稀疏形态 | 否 |
| G81-2004 | MCMYaorenmaoRirichiyoEyeBlinkL | Genesis8_1Female | /Hidden/People/Yaorenmao Ririchiyo/MCMs | 稀疏形态 | 否 |
| G81-2005 | MCMYaorenmaoRirichiyoEyeBlinkR | Genesis8_1Female | /Hidden/People/Yaorenmao Ririchiyo/MCMs | 稀疏形态 | 否 |
| G81-2006 | MCMYuaEyeBlinkL | Genesis8_1Female | /Hidden/People/Yua/MCMs | 稀疏形态 | 否 |
| G81-2007 | MCMYuaEyeBlinkR | Genesis8_1Female | /Hidden/People/Yua/MCMs | 稀疏形态 | 否 |
| G81-2008 | MCMYujinEyeCloseLashesL | Genesis8_1Female | /Hidden/People/Yujin | 控制器 | 否 |
| G81-2009 | MCMYujinEyeCloseLashesR | Genesis8_1Female | /Hidden/People/Yujin | 控制器 | 否 |
| G81-2010 | MCMYunJenEyeBlinkL | Genesis8_1Female | /Hidden/People/Yun Jen/MCMs | 稀疏形态 | 否 |
| G81-2011 | MCMYunJenEyeBlinkR | Genesis8_1Female | /Hidden/People/Yun Jen/MCMs | 稀疏形态 | 否 |
| G81-2012 | Mio Genesis 8.1 Tear | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-2013 | Mouth Smile | Genesis8_1Female | /Pose Controls/Head/Mouth | 控制器 | 是 |
| G81-2014 | Mouth Smile | head | /Pose Controls/Head/Mouth | 别名 | 是 |
| G81-2015 | NEKO Minto Eyelashes Shape | Genesis8_1Female | /Hidden/People/NEKO Minto RE/PBMs | 控制器 | 是 |
| G81-2016 | NMK Right Thumb Bends | Genesis8Female | /Pose Controls/Hands/Right/Nightmare Killer Glove | 控制器 | 是 |
| G81-2017 | PHMKimSeohyunLashes | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G81-2018 | RAV Jang Hee-jin Lashes Shape | Genesis8_1Female | /Hidden/People/RAVJangHee-jin/PBM | 控制器 | 是 |
| G81-2019 | Rikka Eyelashes | Genesis8_1Female | /Real World | 控制器 | 是 |
| G81-2020 | SC Jiao Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-2021 | Teen Josie 8 Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-2022 | Teen Kaylee 8 Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G81-2023 | Thigh bend Both+ | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-2024 | Thigh bend Both- | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-2025 | thigh Both +57 | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-2026 | Thigh side Both | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-2027 | Thight side Both TeenJosie | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G81-2028 | Vo Xiao Mei Head | Genesis8_1Female | /People/Real World | 稀疏形态 | 是 |

## 附录：未成功建立参数条目的资源

以下为读取阶段诊断，不计入以上条目数量；两个角色报告的是同一组源文件。

| 资源文件 | 读取失败原因 |
| --- | --- |
| H:/G3/data/DAZ 3D/Genesis 8/Female/Morphs/Raiya/Neko/eJCMNekoEyesClosedL.dsf | Morph 差值 count 不一致 |
| H:/G3/data/DAZ 3D/Genesis 8/Female/Morphs/Raiya/Neko/eJCMNekoEyesClosedR.dsf | Morph 差值 count 不一致 |
