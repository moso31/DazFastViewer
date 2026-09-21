# Genesis 8 Female 尚不能手动有效调整的参数清单

依据：2026-09-21 最终兼容性验证快照。本清单包含控制器、形态和子节点别名，保留隐藏项；同名条目不合并。

共 826 条：755 条禁止手动编辑（其中 378 条默认可见），另有 71 条在新增项独立取样中无明显形变。

“禁止手动编辑”包含资产主动锁定，不能将全部条目都理解为程序故障。独立取样仅覆盖本次新增可编辑集合，不是所有可编辑参数的全范围视觉验收。G8.1 的旧控制器可能受代际覆盖影响。

源文件、内部 ID、完整原因、未解析 URI 和取样数值见 [完整 CSV](remaining-morphs.csv)，可按下表编号定位。

| 分类 | 数量 |
| --- | --- |
| 公式引用缺失、被覆盖或属性不支持 | 160 |
| 别名目标未解析 | 6 |
| 下游参数受阻 | 9 |
| 没有有效输出 | 47 |
| 骨骼属性别名尚未支持编辑 | 16 |
| 目标顶点数不匹配 | 14 |
| HD 尚未支持 | 310 |
| 资产明确锁定 | 193 |
| 单独取样无明显形变 | 71 |

## 公式引用缺失、被覆盖或属性不支持（160 条）

需要逐项区分错误地址、代际覆盖及真正缺失的依赖；不能统一视为缺文件。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0001 | !Aging Body Controller | Genesis8Female | /Full Body/Real World/Aging Morphs | 控制器 | 是 |
| G8-0002 | 01 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0003 | 01 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0004 | 05 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0005 | 05 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0006 | 06 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0007 | 06 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0008 | 07 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0009 | 07 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0010 | 08 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0011 | 08 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0012 | 09 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0013 | 09 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0014 | 10 EJ Midori Expression | Genesis8Female | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 控制器 | 是 |
| G8-0015 | 10 EJ Midori Expression | head | /Pose Controls/Head/Expressions/EmmaAndJordi/EJ Midori | 别名 | 是 |
| G8-0016 | Amber Smile 01 | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 控制器 | 是 |
| G8-0017 | Amber Smile 01 | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G8-0018 | AutoLife_Age000s | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0019 | AutoLife_Age000s Shape Null | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0020 | BendfixAbdoLowbackUpfrontSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0021 | BendfixAbdolowerbackSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0022 | BendfixAbdolowerbackSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0023 | BendfixAbdolowfrontUpbackSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0024 | BendfixLThigh90SH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0025 | BendfixLthighSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0026 | BendfixRThigh90SH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0027 | BendfixRthighSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0028 | Breast Helper Super Fixer | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0029 | CTRLMD_N_eCTRLEyesClosed_1 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0030 | CTRLMD_N_Value_1_AM_Z | geometry | /Hidden/CTRLMDs | 稀疏形态 | 否 |
| G8-0031 | CTRLMD_N_YRotate_22 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0032 | CTRLMD_N_YRotate_n22 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0033 | DDGamerGirlPose03 | Genesis8Female | /Pose Controls/Full Body/DD Gamer Girl Poses | 控制器 | 是 |
| G8-0034 | eCTRLAuroreMouthSmile | Genesis8Female | /Hidden/People/Aurore/eCTRLs | 控制器 | 否 |
| G8-0035 | eJCM_Bridget8_Fear | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0036 | eJCM_Bridget8_Glare | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0037 | eJCM_Bridget8_Happy | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0038 | eJCM_Bridget8_Pain | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0039 | eJCM_Bridget8_Rage | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0040 | eJCM_Bridget8_Scream | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0041 | eJCM_Bridget8_Serious | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0042 | eJCM_Bridget8_Triumph | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0043 | eJCM_Bridget8_Wink | geometry | /Hidden/People/Bridget 8/eJCMs | 稀疏形态 | 否 |
| G8-0044 | eJCMAiko8Bereft | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0045 | eJCMAiko8Concentrate | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0046 | eJCMAiko8Confident | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0047 | eJCMAiko8Confused | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0048 | eJCMAiko8Desire | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0049 | eJCMAiko8Disgust | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0050 | eJCMAiko8Excitement | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0051 | eJCMAiko8Fear | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0052 | eJCMAiko8Happy | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0053 | eJCMAiko8Ill | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0054 | eJCMAiko8Pain | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0055 | eJCMAiko8Pleased | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0056 | eJCMAiko8Pouty | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0057 | eJCMAiko8Rage | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0058 | eJCMAiko8Sarcastic | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0059 | eJCMAiko8Scream | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0060 | eJCMAiko8Silly | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0061 | eJCMAiko8Snarl | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0062 | eJCMAiko8Tired | Genesis8Female | /Hidden/People/Aiko 8/eJCMs | 控制器 | 否 |
| G8-0063 | eJCMAiko8Triumph | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0064 | eJCMAiko8Wink | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0065 | eJCMAuroreFear_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0066 | eJCMAuroreHappy_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0067 | eJCMAurorePain_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0068 | eJCMAurorePouty_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0069 | eJCMAuroreRage_HD_div2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0070 | eJCMAuroreScream_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0071 | eJCMGabriela8_Fear_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0072 | eJCMGabriela8_Happy_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0073 | eJCMGabriela8_Pain_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0074 | eJCMGabriela8_Rage_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0075 | eJCMGabriela8_Scream_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0076 | eJCMGabriela8_Triumph_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0077 | eJCMGabriela8_Wink_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0078 | eJCMMrsChow8_Excitement_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0079 | eJCMMrsChow8_Fear_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0080 | eJCMMrsChow8_Happy_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0081 | eJCMMrsChow8_Pain_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0082 | eJCMMrsChow8_Rage_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0083 | eJCMMrsChow8_Scream_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0084 | eJCMMrsChow8_Triumph_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0085 | eJCMMrsChow8_Wink_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0086 | Elisa v2 | geometry | /Full Body | 稀疏形态 | 是 |
| G8-0087 | Elsie Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0088 | GluteFixBendthigh90LSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0089 | GluteFixBendthigh90RSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0090 | GluteFixBendthighS2SLSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0091 | GluteFixBendthighS2SLSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0092 | GluteFixBendthighS2SRSH1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0093 | GluteFixBendthighS2SRSH2 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0094 | Heavy LAbdo Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G8-0095 | Heavy LAbdo Details Neg and Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G8-0096 | Heavy Lower Details &amp; LAbdoBend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G8-0097 | Heavy U&amp;L Details Neg and Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G8-0098 | Heidi Kiss | Genesis8Female | /Pose Controls/Head/Expressions/Heidi (3DU) | 控制器 | 是 |
| G8-0099 | Heidi Kiss | head | /Pose Controls/Head/Expressions/Heidi (3DU) | 别名 | 是 |
| G8-0100 | Heidi Tongue Out 02 | Genesis8Female | /Pose Controls/Head/Expressions/Heidi (3DU) | 控制器 | 是 |
| G8-0101 | Heidi Tongue Out 02 | head | /Pose Controls/Head/Expressions/Heidi (3DU) | 别名 | 是 |
| G8-0102 | Hide Eyes | Genesis8Female | /General | 控制器 | 是 |
| G8-0103 | JCM Heavy LThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0104 | JCM Heavy RThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0105 | JCM Heavy UAbdoback | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0106 | JCM Heavy UAbdoback Labdo Forward | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0107 | JCM Liloo ArmBendFix L | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0108 | JCM Liloo ArmBendFix L Null | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0109 | JCM Liloo ArmBendFix R | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0110 | JCM Liloo ArmBendFix R Null | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0111 | JCM Pear LAbdo Bend | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0112 | JCM Pear LThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0113 | JCM Pear RThigh Bendfix | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0114 | JCM Pear UAbdoback Labdo Forward | geometry | /Hidden/Body Mixer | 稀疏形态 | 否 |
| G8-0115 | JCMAbdoUChestLSide-sideFixLCombo | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0116 | JCMAbdoUChestLSide-sideFixRCombo | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0117 | JCMAbdoUpperChestLSide-sideFixL | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0118 | JCMAbdoUpperChestLSide-sideFixR | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0119 | Kirara Body Busty | geometry | /Kirara | 稀疏形态 | 是 |
| G8-0120 | Kirara Face | geometry | /Kirara | 稀疏形态 | 是 |
| G8-0121 | Liya | Genesis8Female | /Full Body/People/Real World | 控制器 | 是 |
| G8-0122 | Liya | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0123 | MCMBereftEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0124 | MCMFierceEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0125 | MCMHSLurysEyesClosedL | geometry | /Hidden/People/Lurys | 稀疏形态 | 否 |
| G8-0126 | MCMHSLurysEyesClosedR | geometry | /Hidden/People/Lurys | 稀疏形态 | 否 |
| G8-0127 | MCMKannaEyesClosedL-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G8-0128 | MCMKannaEyesClosedR-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G8-0129 | MCMPainEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0130 | MCMRageEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0131 | MCMScreamEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0132 | MCMSillyEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0133 | MCMVWNewbornEllaLEyeClosed | Genesis8Female | /Hidden/Correctives/Base Head | 控制器 | 否 |
| G8-0134 | MCMVWNewbornEllaREyeClosed | Genesis8Female | /Hidden/Correctives/Base Head | 控制器 | 否 |
| G8-0135 | MCMWinkEJMidori | geometry | /Hidden/People/EJ Midori | 稀疏形态 | 否 |
| G8-0136 | Momo | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0137 | Nyao Expression 01 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G8-0138 | Nyao Expression 04 | Genesis8Female | /Pose Controls/Head/Expressions | 控制器 | 是 |
| G8-0139 | Pear &amp; Heavy LAbdo Bend | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G8-0140 | Pear Lower Details Bendback UAbdo&amp;Lower | geometry | /Hidden/Body Mixer/Assist Morphs | 稀疏形态 | 否 |
| G8-0141 | QX Ferin Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0142 | QX Ferin Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0143 | SFHD Arm Bend Down Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0144 | SFHD Arm Bend Down Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0145 | SFHD Collar Bend Up Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0146 | SFHD Collar Bend Up Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0147 | Thigh bend Both+ Stephanie | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0148 | Thigh side Both Olympia | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0149 | Thight side Both Victoria | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0150 | Tifa Body | geometry | /Tifa G8F | 稀疏形态 | 是 |
| G8-0151 | Tifa Face | geometry | /Tifa G8F | 稀疏形态 | 是 |
| G8-0152 | Vo Xiao Hong  | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0153 | VO Xiao Xin | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0154 | VW Baby Ella | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0155 | Z SC 01 Exhale | Genesis8Female | /Pose Controls/Head/Expressions/Z Smokers Club | 控制器 | 是 |
| G8-0156 | Z SC 01 Exhale | head | /Pose Controls/Head/Expressions/Z Smokers Club | 别名 | 是 |
| G8-0157 | Z SC 02 Blow Smoke | Genesis8Female | /Pose Controls/Head/Expressions/Z Smokers Club | 控制器 | 是 |
| G8-0158 | Z SC 02 Blow Smoke | head | /Pose Controls/Head/Expressions/Z Smokers Club | 别名 | 是 |
| G8-0159 | Z SC 03 Inhale | Genesis8Female | /Pose Controls/Head/Expressions/Z Smokers Club | 控制器 | 是 |
| G8-0160 | Z SC 03 Inhale | head | /Pose Controls/Head/Expressions/Z Smokers Club | 别名 | 是 |

## 别名目标未解析（6 条）

该面板条目没有找到唯一可编辑目标。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0161 | HS Lurys Unhappy | head | /Pose Controls/Head/Expressions/Lurys Expression | 别名 | 是 |
| G8-0162 | Z FI 01 Terror G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G8-0163 | Z FI 02 Fright G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G8-0164 | Z FI 03 Distress G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G8-0165 | Z FI 04 Panic G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |
| G8-0166 | Z FI 05 Dread G8F | head | /Pose Controls/Head/Expressions/Z Fear Itself | 别名 | 是 |

## 下游参数受阻（9 条）

控制器自身存在，但实际驱动的下游仍受 HD 或引用问题影响。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0167 | GamerGirl Expression 07 | Genesis8Female | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 控制器 | 是 |
| G8-0168 | Liloo HD Details | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0169 | Liya | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0170 | LY Sterling | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0171 | Neko HD Details | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0172 | Paisley | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0173 | QX Ferin | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0174 | Shuang | Genesis8Female | /People/Real World | 控制器 | 是 |
| G8-0175 | Skin Crease Controller | Genesis8Female | /Full Body/Real World/Skin Folds &amp; Creases HD/Controllers | 控制器 | 是 |

## 没有有效输出（47 条）

当前角色内无有效输出；部分服装 / 睫毛项需跨对象驱动进一步验证。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0176 | AG Jenna Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0177 | Alice Eyelashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0178 | BGM Heavy Clothing Smoother | Genesis8Female | /Full Body/Real World | 控制器 | 是 |
| G8-0179 | BGM Heavy Clothing Smoother Skirts | Genesis8Female | /Full Body/Real World | 控制器 | 是 |
| G8-0180 | Body Shapes - Children Clothing Helper | Genesis8Female | /Full Body/Real World/Body Shapes Children | 控制器 | 是 |
| G8-0181 | Breast Helper Default Lower | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0182 | Breast Helper Default Upper | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0183 | Breast Helper Larger Lower | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0184 | Breast Helper Larger Upper | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0185 | Breast Helper Midzone Delumpifier 01 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0186 | Breast Helper Midzone Delumpifier 02 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0187 | Breast Helper Midzone Delumpifier 03 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0188 | Breast Helper Strap Lifter 01 | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0189 | Breast Helper Strap Width | Genesis8Female | /Real World/Universal Breast Helper | 控制器 | 是 |
| G8-0190 | Clothing Fit Helper Back Helper Lower | Genesis8Female | /Real World/Clothing Fit Helper | 控制器 | 是 |
| G8-0191 | DD Long Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0192 | DD Short Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0193 | Difa Eyelashes | Genesis8Female |  | 控制器 | 是 |
| G8-0194 | Eyelashes Long P3Design | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0195 | Eyelashes Natural | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0196 | Fredda Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0197 | JA Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0198 | Kaoruko Eyelashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0199 | KrashWerks - Huan Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0200 | KrashWerks - KerriAnne Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0201 | KrashWerks - Sarah Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0202 | Long Lashes by FWArt | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0203 | Long Lashes by FWSA | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0204 | Long Lashes by Hinky Punk | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0205 | Long Lashes by Hinky Punk | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0206 | LY Long Lash | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0207 | NIPPLE INVERTED Do not require Nipple Base | Genesis8Female | /Sonsy 2025 Nipples | 控制器 | 是 |
| G8-0208 | Paisley Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0209 | RN Kayla Lashes 01 | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0210 | SASE Long Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0211 | SI Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0212 | TMLI Lashes 01 | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0213 | Vo Xiao Bei Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0214 | VO Xiao Xin Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0215 | VO Xiao Xong Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0216 | Xia Lashes | Genesis8Female | /Real World | 控制器 | 否 |
| G8-0217 | Yue Eyelashes Down | Genesis8Female | /People/Stylized | 控制器 | 是 |
| G8-0218 | Z DAH G8F Tipsy | Genesis8Female | /Pose Controls/Head/Expressions/ Z Drunk and Hungover | 控制器 | 是 |
| G8-0219 | Z HCC Hoover Bend Morph | Genesis8Female | /Hidden/Correctives/Zeddicuss | 控制器 | 否 |
| G8-0220 | Z HCC Rubbish Under Rug | Genesis8Female | /Hidden/Correctives/Zeddicuss | 控制器 | 否 |
| G8-0221 | Z HCC Rug Hold Up | Genesis8Female | /Hidden/Correctives/Zeddicuss | 控制器 | 否 |
| G8-0222 | Zhi Lashes | Genesis8Female | /Real World | 控制器 | 否 |

## 骨骼属性别名尚未支持编辑（16 条）

Twist、Bend 等别名需要接入姿势输入与双向同步。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0223 | Bend | lShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0224 | Bend | lThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0225 | Bend | lWrist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0226 | Bend | rShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0227 | Bend | rThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0228 | Bend | rWrist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0229 | Front-Back | lShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0230 | Front-Back | rShldrTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0231 | Side-Side | lThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0232 | Side-Side | rThighTwist | /General/Transforms/Rotation | 别名 | 是 |
| G8-0233 | Twist | lForeArm | /General/Transforms/Rotation | 别名 | 是 |
| G8-0234 | Twist | lShldr | /General/Transforms/Rotation | 别名 | 是 |
| G8-0235 | Twist | lThigh | /General/Transforms/Rotation | 别名 | 是 |
| G8-0236 | Twist | rForeArm | /General/Transforms/Rotation | 别名 | 是 |
| G8-0237 | Twist | rShldr | /General/Transforms/Rotation | 别名 | 是 |
| G8-0238 | Twist | rThigh | /General/Transforms/Rotation | 别名 | 是 |

## 目标顶点数不匹配（14 条）

明确的计数不匹配仍被拒绝，不能直接套用到当前角色。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0239 | Amber Grin | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 未验证形态 | 是 |
| G8-0240 | Amber Grin | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G8-0241 | Amber Open Smile | Genesis8Female | /Pose Controls/Head/Expressions/Amber (3DU) | 未验证形态 | 是 |
| G8-0242 | Amber Open Smile | head | /Pose Controls/Head/Expressions/Amber (3DU) | 别名 | 是 |
| G8-0243 | Anakku Lashes | Genesis8Female | /Face/Eyes/Real World | 未验证形态 | 是 |
| G8-0244 | BendfixUpLowAbdoSKinFoldSH2LS | Genesis8Female | /Hidden/Correctives/Aging Morphs | 未验证形态 | 否 |
| G8-0245 | Eyelashes Curl | Genesis8Female | /Real World | 未验证形态 | 是 |
| G8-0246 | Eyelashes Hide Layer 1 | Genesis8Female | /Real World | 未验证形态 | 是 |
| G8-0247 | Eyelashes Hide Layer 2 | Genesis8Female | /Real World | 未验证形态 | 是 |
| G8-0248 | Eyelashes Top Point | Genesis8Female | /Real World | 未验证形态 | 是 |
| G8-0249 | MCMAnakkuLashesDownL | Genesis8Female | /Hidden/People/Anakku | 未验证形态 | 否 |
| G8-0250 | MCMAnakkuLashesDownR | Genesis8Female | /Hidden/People/Anakku | 未验证形态 | 否 |
| G8-0251 | Neck Wattle Single HD | Genesis8Female | /Full Body/Real World/Aging Morphs/Neck | 未验证形态 | 是 |
| G8-0252 | SFHD Chest Lower Bend Back Folds | Genesis8Female | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 未验证形态 | 是 |

## HD 尚未支持（310 条）

需要完整 HD / SubD 支持，包括部分名称中没有 HD 的形态。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0253 | !! NIPPLE BASE - Not needed if using Sonsy 2024 Nipples !! | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0254 | !! NIPPLE BASE Alternate Version BETTER | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0255 | Aged Body 1 Shape HD | geometry | /Full Body/Real World/Aging Morphs/Body | 稀疏形态 | 是 |
| G8-0256 | Aged Body 2 Assist HD | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0257 | Aged Body 2 Shape HD | geometry | /Full Body/Real World/Aging Morphs/Body | 稀疏形态 | 是 |
| G8-0258 | Aging Arms Detail HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0259 | Aging Body Detail HD 1 | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0260 | Aging Body Detail HD 2 | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0261 | Aging Body Skin Base HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0262 | Aging Feet Detail HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0263 | Aging Hands Detail HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0264 | Alice Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0265 | Alice Liu Happy | geometry | /Pose Controls/Head/Expressions/Alice Liu Expressions | 稀疏形态 | 是 |
| G8-0266 | Alice Liu Sad | geometry | /Pose Controls/Head/Expressions/Alice Liu Expressions | 稀疏形态 | 是 |
| G8-0267 | Alice Liu Sweet | geometry | /Pose Controls/Head/Expressions/Alice Liu Expressions | 稀疏形态 | 是 |
| G8-0268 | Areola Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0269 | Areola Central Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0270 | Areola Edge Big Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0271 | Areola Edge Lines B | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0272 | Areola Edge Small Bumps | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0273 | Areola Gator Skin | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0274 | Areola Mounds | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0275 | Areola Puffy | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0276 | Areola Ridge Lines | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0277 | Athletic HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G8-0278 | Audrey Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0279 | Audrey Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0280 | Aurore Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0281 | Aurore Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0282 | Aurore Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0283 | Auto Life 100 Added Detail Hd | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0284 | AutoLife 100 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0285 | AutoLife 15 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0286 | AutoLife 30 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0287 | AutoLife 50 Gen Smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0288 | AutoLife 75 Gen smoother | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0289 | AutoLife_Age100 Skin HD | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0290 | AutoLife_Age30 Skin HD | geometry | /Hidden/D.Master/Auto Life | 稀疏形态 | 否 |
| G8-0291 | Ayako Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0292 | Ayako Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0293 | Belly Cellulite HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0294 | Belly Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0295 | BGM Cellulite Belly HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0296 | BGM Cellulite Buttocks HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0297 | BGM Cellulite Thighs HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0298 | BGM Chin 01 | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0299 | BGM Chin 02 | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0300 | BGM Laura Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0301 | BGM Sarah Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0302 | BGM Stretch Marks Belly HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0303 | BGM Stretch Marks Hips HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0304 | Bold HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G8-0305 | Breast Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0306 | Brow Compression HD | geometry | /Pose Controls/Head/Brow | 稀疏形态 | 是 |
| G8-0307 | Brow Compression HD | head | /Pose Controls/Head/Brow | 别名 | 是 |
| G8-0308 | Brow Lateral Prominence HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G8-0309 | Brow Slack HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G8-0310 | Brow Superciliary Arch HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G8-0311 | Brow Thin HD | geometry | /Brow/Real World | 稀疏形态 | 是 |
| G8-0312 | Carmelita Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0313 | Carmelita Elf Ears | geometry | /Fantasy SciFi | 稀疏形态 | 是 |
| G8-0314 | Carmelita Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0315 | Casual HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G8-0316 | Cheek Acne Scars HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0317 | Cheekbones Arch Size HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0318 | Cheekbones Depression HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0319 | Cheeks Dimple Crease HD Left | geometry | /Pose Controls/Head/Cheeks and Jaw | 稀疏形态 | 是 |
| G8-0320 | Cheeks Dimple Crease HD Left | head | /Pose Controls/Head/Cheeks and Jaw | 别名 | 是 |
| G8-0321 | Cheeks Dimple Crease HD Right | geometry | /Pose Controls/Head/Cheeks and Jaw | 稀疏形态 | 是 |
| G8-0322 | Cheeks Dimple Crease HD Right | head | /Pose Controls/Head/Cheeks and Jaw | 别名 | 是 |
| G8-0323 | Cheeks Slack HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0324 | Chin Acne Scars HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0325 | Chin Cleft HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0326 | Chin Crease HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0327 | Chin Wrinkles HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0328 | Crows Feet HD Left | geometry | /Real World | 稀疏形态 | 是 |
| G8-0329 | Crows Feet HD Right | geometry | /Real World | 稀疏形态 | 是 |
| G8-0330 | Curvy HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G8-0331 | DD Isabelle Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0332 | DD Isabelle Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0333 | DD Nails | geometry | /Real World | 稀疏形态 | 是 |
| G8-0334 | DD Nails - Square | geometry | /Real World | 稀疏形态 | 是 |
| G8-0335 | DD Nails - Squoval | geometry | /Real World | 稀疏形态 | 是 |
| G8-0336 | DD Nails - Stiletto | geometry | /Real World | 稀疏形态 | 是 |
| G8-0337 | eJCMAfraid_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0338 | eJCMAiko8BrowSqueeze_HDLv1L | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0339 | eJCMAiko8BrowSqueeze_HDLv1R | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0340 | eJCMAiko8MouthSmile_HDLv1 | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0341 | eJCMAiko8MouthSmileOpen_HDLv1 | geometry | /Hidden/People/Aiko 8/eJCMs | 稀疏形态 | 否 |
| G8-0342 | eJCMAngry_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0343 | eJCMAuroreAfraid_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0344 | eJCMAuroreAngry_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0345 | eJCMAuroreMouthSmile | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0346 | eJCMAuroreShock_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0347 | eJCMAuroreSmileFullFace_HD_div2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0348 | eJCMAuroreSmileOpenFullFace_HD_div2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0349 | eJCMAuroreSurprise_HDLv2 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0350 | eJCMFlirting_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0351 | eJCMFrown_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0352 | eJCMGabriela8_Angry_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0353 | eJCMGabriela8_BrowSqueezeL_HDLv2 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0354 | eJCMGabriela8_BrowSqueezeR_HDLv2 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0355 | eJCMGabriela8_ForeheadWrinkleL_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0356 | eJCMGabriela8_ForeheadWrinkleR_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0357 | eJCMGabriela8_MouthSmile_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0358 | eJCMGabriela8_MouthSmileOpen_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0359 | eJCMGabriela8_NoseCompression_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0360 | eJCMGabriela8_SmileFullFace_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0361 | eJCMGabriela8_SmileOpenFullFace_HDLv3 | geometry | /Hidden/People/Gabriela 8/eJCMs | 稀疏形态 | 否 |
| G8-0362 | eJCMMrsChow8_BrowSqueezeL_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0363 | eJCMMrsChow8_BrowSqueezeR_HDLv2 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0364 | eJCMMrsChow8_ForeheadWrinkleL_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0365 | eJCMMrsChow8_ForeheadWrinkleR_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0366 | eJCMMrsChow8_SmileFullFace_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0367 | eJCMMrsChow8_SmileFullFaceOpen_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/eJCMs | 稀疏形态 | 否 |
| G8-0368 | eJCMShock_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0369 | eJCMSmileFullFace_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0370 | eJCMSmileOpenFullFace_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0371 | eJCMSurprise_HD_div2 | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0372 | Elsie Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0373 | Eye Bags HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G8-0374 | Eye Rim Refine HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0375 | Eye Side Wrinkles HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G8-0376 | Eyebrow Centre Folds HD | geometry | /Real World/Aging Morphs/Brow &amp; Forehead | 稀疏形态 | 是 |
| G8-0377 | Eyelids Lower Crease HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0378 | Face Age Shape HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0379 | Face Base Detail HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0380 | Face Base Skin HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0381 | Face Fine Wrinkles HD | geometry | /Real World/Aging Morphs/Face | 稀疏形态 | 是 |
| G8-0382 | Forehead Wrinkle HD Left | geometry | /Real World | 稀疏形态 | 是 |
| G8-0383 | Forehead Wrinkle HD Right | geometry | /Real World | 稀疏形态 | 是 |
| G8-0384 | Forehead Wrinkles HD | geometry | /Real World/Aging Morphs/Brow &amp; Forehead | 稀疏形态 | 是 |
| G8-0385 | GamerGirl Expression 01 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G8-0386 | GamerGirl Expression 02 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G8-0387 | GamerGirl Expression 04 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G8-0388 | GamerGirl Expression 06 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G8-0389 | GamerGirl Expression 08 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G8-0390 | GamerGirl Expression 10 | geometry | /Pose Controls/Head/Expressions/DD Gamer Girl Expressions | 稀疏形态 | 是 |
| G8-0391 | Giada Body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G8-0392 | Giada Head | geometry | /People/Stylized | 稀疏形态 | 是 |
| G8-0393 | Glute Cellulite Left HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0394 | Glute Cellulite Left HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0395 | Glute Cellulite Right HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0396 | Glute Cellulite Right HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0397 | Glute Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0398 | GraceYong Chest Upper Shape HDlv4 | geometry | /Hidden/People/PCGraceYong/PBMs | 稀疏形态 | 否 |
| G8-0399 | Hailey Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0400 | Hailey Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0401 | Hip Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0402 | JA Clary Body | Genesis8Female | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G8-0403 | JA Clary Head | Genesis8Female | /People/Stylized | 稀疏形态 | 是 |
| G8-0404 | Jaw Slack HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0405 | JCMDDGamerGirlPose01 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G8-0406 | JCMDDGamerGirlPose02 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G8-0407 | JCMDDGamerGirlPose03 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G8-0408 | JCMDDGamerGirlPose08 | geometry | /Pose Controls/Full Body/DD Gamer Girl Poses | 稀疏形态 | 是 |
| G8-0409 | Jowls 1 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0410 | Jowls 2 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0411 | Laugh Lines 1 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0412 | Laugh Lines 2 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0413 | Laugh Lines 3 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0414 | Laugh Lines 4 HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0415 | Leony Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0416 | Leony Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0417 | Leony Navel HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0418 | Liloo Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0419 | Liloo Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0420 | Lips Contour HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0421 | Long Nails by FWArt | geometry | /Real World | 稀疏形态 | 是 |
| G8-0422 | Longer Nails | geometry | /Real World | 稀疏形态 | 是 |
| G8-0423 | LY Sterling Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0424 | LY Sterling Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0425 | LY Sterling Head Asymmetry | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0426 | Marionette Lines HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0427 | MCM_LY_Sterling_Left | geometry | /Hidden/Correctives/Lyoness/Sterling | 稀疏形态 | 否 |
| G8-0428 | MCM_LY_Sterling_right | geometry | /Hidden/Correctives/Lyoness/Sterling | 稀疏形态 | 否 |
| G8-0429 | MCMCarynHD_Chubby | geometry | /Hidden/People/Caryn/MCMs | 稀疏形态 | 否 |
| G8-0430 | MCMDDElsieEyesClosedL-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G8-0431 | MCMDDElsieEyesClosedR-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G8-0432 | MCMDDPaisleyEyesClosedL-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G8-0433 | MCMDDPaisleyEyesClosedR-FIX | geometry | /Hidden/Correctives | 稀疏形态 | 是 |
| G8-0434 | MCMGabriela8_MouthRealism_HDLv3 | geometry | /Hidden/People/Gabriela 8/MCMs | 稀疏形态 | 否 |
| G8-0435 | MCMGabriela8_Navel_HDLv4 | geometry | /Hidden/People/Gabriela 8/MCMs | 稀疏形态 | 否 |
| G8-0436 | MCMGabriela8_Nipples_HDLv4 | geometry | /Hidden/People/Gabriela 8/MCMs | 稀疏形态 | 否 |
| G8-0437 | MCMGGExpression09FIX | geometry | /Hidden/Correctives/Expressions | 稀疏形态 | 否 |
| G8-0438 | MCMMrsChow8_MouthRealism_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/MCMs | 稀疏形态 | 否 |
| G8-0439 | MCMMrsChow8_Navel_HDLv4 | geometry | /Hidden/People/Mrs Chow 8/MCMs | 稀疏形态 | 否 |
| G8-0440 | MCMMrsChow8_Nipples_HDLv4 | geometry | /Hidden/People/Mrs Chow 8/MCMs | 稀疏形态 | 否 |
| G8-0441 | MCMNyaoSquareNailsFIX | Genesis8Female | /Hidden/Correctives | 稀疏形态 | 否 |
| G8-0442 | MCMNyaoSquovalNailsFIX | geometry | /Hidden/Correctives | 稀疏形态 | 否 |
| G8-0443 | MCMSayaNavelLv4 | geometry | /Hidden/People/Saya/MCMs | 稀疏形态 | 否 |
| G8-0444 | MCMSayaNipplesLv4 | geometry | /Hidden/People/Saya/MCMs | 稀疏形态 | 否 |
| G8-0445 | MCMSayaSmileLv1 | Genesis8Female | /Hidden/People/Saya/MCMs | 稀疏形态 | 否 |
| G8-0446 | Minami HD Body Details | Genesis8Female | /Full Body/People/Real World | 仅HD | 是 |
| G8-0447 | Mouth Realism HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0448 | Mouth Wrinkles HD | geometry | /Real World/Aging Morphs/Mouth | 稀疏形态 | 是 |
| G8-0449 | MPLeonyOutfitPants | geometry | /Clothing/OOT Micro Pressure | 稀疏形态 | 是 |
| G8-0450 | MPLeonyOutfitTop | geometry | /Clothing/OOT Micro Pressure | 稀疏形态 | 是 |
| G8-0451 | MPVelocityOutfitBra | geometry | /Full Body/OOT Micro Pressure | 稀疏形态 | 是 |
| G8-0452 | MPVelocityOutfitPants | geometry | /Full Body/OOT Micro Pressure | 稀疏形态 | 是 |
| G8-0453 | Naomi HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0454 | Naomi Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0455 | Neck Detail 1 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0456 | Neck Detail 2 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0457 | Neck Detail 3 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0458 | Neck Detail 4 HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0459 | Neck Fine Wrinkles HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0460 | Neck Wattle Double HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0461 | Neck Wrinkles HD | geometry | /Full Body/Real World/Aging Morphs/Neck | 稀疏形态 | 是 |
| G8-0462 | Neko Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0463 | Neko Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0464 | Neko Navel HD | geometry | /Real World | 稀疏形态 | 是 |
| G8-0465 | Nipple Base Bend Adjustment Fix | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0466 | Nipple Bigger HD | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0467 | Nipple Fatter | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0468 | Nipple Inverted 1 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0469 | Nipple Inverted 2 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0470 | Nipple Longer | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0471 | Nipple Realism | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0472 | Nipple Realism OPTION 2 Pointy | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0473 | Nose Compression HD | geometry | /Pose Controls/Head/Nose | 稀疏形态 | 是 |
| G8-0474 | Nose Compression HD | head | /Pose Controls/Head/Nose | 别名 | 是 |
| G8-0475 | Nose Pores HD | geometry | /Real World/Aging Morphs/Nose | 稀疏形态 | 是 |
| G8-0476 | Nose Wrinkles HD | geometry | /Real World/Aging Morphs/Nose | 稀疏形态 | 是 |
| G8-0477 | Nyao Body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G8-0478 | Nyao Body | geometry | /Full Body/People/Stylized | 稀疏形态 | 是 |
| G8-0479 | Nyao Head | geometry | /People/Stylized | 稀疏形态 | 是 |
| G8-0480 | Nyao Smile | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G8-0481 | Nyao Smile Open | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G8-0482 | Nyao Smile Simple | geometry | /Pose Controls/Head/Mouth | 稀疏形态 | 是 |
| G8-0483 | Nylon Pantyhose-Waist-Squeezing1 | geometry | /Clothing/Nylon Pantyhose | 稀疏形态 | 是 |
| G8-0484 | Nylon Pantyhose-Waist-Squeezing2 | geometry | /Clothing/Nylon Pantyhose | 稀疏形态 | 是 |
| G8-0485 | Nylon Pantyhose-Waist-Squeezing3 | geometry | /Clothing/Nylon Pantyhose | 稀疏形态 | 是 |
| G8-0486 | P3D Mirai Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0487 | Paisley Body | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0488 | Paisley Head | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0489 | PBMAuroreNavelHDlv4 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0490 | PBMAuroreNipplesHDlv4 | geometry | /Hidden/People/Aurore/eJCMs | 稀疏形态 | 否 |
| G8-0491 | pJCMGabriela8_HeadTwistL_HDLv3 | geometry | /Hidden/People/Gabriela 8/pJCMs | 稀疏形态 | 否 |
| G8-0492 | pJCMGabriela8_HeadTwistR_HDLv3 | geometry | /Hidden/People/Gabriela 8/pJCMs | 稀疏形态 | 否 |
| G8-0493 | pJCMMrsChow8_Head_TwistL_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/pJCMs | 稀疏形态 | 否 |
| G8-0494 | pJCMMrsChow8_Head_TwistR_HDLv3 | geometry | /Hidden/People/Mrs Chow 8/pJCMs | 稀疏形态 | 否 |
| G8-0495 | Raised Cornea | geometry | /Real World | 稀疏形态 | 是 |
| G8-0496 | RN Fredda Body HD Details | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0497 | RN Fredda Head HD Details | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0498 | RN Kayla Body HD Details | Genesis8Female | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0499 | RN Kayla Head HD Details | Genesis8Female | /People/Real World | 稀疏形态 | 是 |
| G8-0500 | RN Kayla Mouth HD Details | Genesis8Female | /Real World | 稀疏形态 | 是 |
| G8-0501 | Rosabell HD Details | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0502 | Saya HD Details | geometry | /People/Real World | 仅HD | 是 |
| G8-0503 | SFHD Chest Lower Bend Front Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G8-0504 | SFHD Chest Lower Bend Front Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 稀疏形态 | 是 |
| G8-0505 | SFHD Chest Lower Left Side Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Left Side | 稀疏形态 | 是 |
| G8-0506 | SFHD Chest Lower Right Side Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Right Side | 稀疏形态 | 是 |
| G8-0507 | SFHD Collar Back Both Crease | geometry | /Hidden/Skin Folds &amp; Creases HD/Crease | 稀疏形态 | 否 |
| G8-0508 | SFHD Collar Left Back Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0509 | SFHD Collar Right Back Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0510 | SFHD Neck Bend Front Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G8-0511 | SFHD Neck Bend Side Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0512 | SFHD Neck Bend Side Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0513 | SFHD Shoulder Bend Back Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0514 | SFHD Shoulder Bend Back Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0515 | SFHD Torso Bend Back Lower Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G8-0516 | SFHD Torso Bend Back Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G8-0517 | SFHD Torso Bend Back Upper Crease Assist 1 | geometry | /Hidden/Skin Folds &amp; Creases HD/Crease | 稀疏形态 | 否 |
| G8-0518 | SFHD Torso Bend Back Upper Crease Assist 2 | geometry | /Hidden/Skin Folds &amp; Creases HD/Crease | 稀疏形态 | 否 |
| G8-0519 | SFHD Torso Bend Back Upper Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 稀疏形态 | 是 |
| G8-0520 | SFHD Torso Bend Front Lower Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G8-0521 | SFHD Torso Bend Front Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Central | 稀疏形态 | 是 |
| G8-0522 | SFHD Torso Bend Front Upper Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Central | 稀疏形态 | 是 |
| G8-0523 | SFHD Torso Bend Left Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0524 | SFHD Torso Bend Right Upper Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0525 | SFHD Torso Bend Side Upper Left Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Left Side | 稀疏形态 | 是 |
| G8-0526 | SFHD Torso Bend Side Upper Left Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Left Side | 稀疏形态 | 是 |
| G8-0527 | SFHD Torso Bend Side Upper Right Crease | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Crease/Right Side | 稀疏形态 | 是 |
| G8-0528 | SFHD Torso Bend Side Upper Right Folds | geometry | /Full Body/Real World/Skin Folds &amp; Creases HD/Folds/Right Side | 稀疏形态 | 是 |
| G8-0529 | Shizuka HD Details | geometry | /People/Stylized | 稀疏形态 | 是 |
| G8-0530 | Shuang Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0531 | Shuang Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0532 | Slim HD | Genesis8Female | /Full Body/Real World | 稀疏形态 | 是 |
| G8-0533 | Spine HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0534 | Taryn Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0535 | Taryn Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0536 | Thigh Cellulite Left HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0537 | Thigh Cellulite Left HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0538 | Thigh Cellulite Right HD A | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0539 | Thigh Cellulite Right HD B | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0540 | Thigh Stretchmarks HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0541 | Tip Bumps Texture | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0542 | Tip Hole 1 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0543 | Tip Hole 2 | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0544 | Tip Hole Deep | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0545 | Tip Hole Horizontal | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0546 | Tip Hole Lip Shape | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0547 | Tip Hole Star Shape | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0548 | Tip Hole Vertical | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0549 | Tip Hole Y Shape | geometry | /Sonsy 2025 Nipples | 稀疏形态 | 是 |
| G8-0550 | Under Eye 1 HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G8-0551 | Under Eye 2 HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G8-0552 | Under Eye 3 HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G8-0553 | Under Eye Wrinkles HD | geometry | /Real World/Aging Morphs/Eyes | 稀疏形态 | 是 |
| G8-0554 | Under Mouth Slack HD | geometry | /Cheeks and Jaw/Real World | 稀疏形态 | 是 |
| G8-0555 | Varicose Veins Left Leg Less HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0556 | Varicose Veins Left Leg More HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0557 | Varicose Veins Right Leg Less HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0558 | Varicose Veins Right Leg More HD | geometry | /Full Body/Real World/Aging Morphs/!Body HD Details | 稀疏形态 | 是 |
| G8-0559 | Willa Body | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0560 | Willa Head | geometry | /People/Real World | 稀疏形态 | 是 |
| G8-0561 | Xia Body HD Details | geometry | /Full Body/People/Real World | 稀疏形态 | 是 |
| G8-0562 | Xia Head HD Details | geometry | /People/Real World | 稀疏形态 | 是 |

## 资产明确锁定（193 条）

不允许手动编辑；它可能仍由公式驱动，不等于求值失效，也不计作待修复故障。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0563 | Aging Shape Blender 1 | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0564 | CTRLMD_N_CTRLMD_N_YRotate_110_1 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0565 | CTRLMD_N_Value_0_8429 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0566 | CTRLMD_N_Value_0_9082 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0567 | CTRLMD_N_Value_0_942 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0568 | CTRLMD_N_Value_0_9517 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0569 | CTRLMD_N_Value_1 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0570 | CTRLMD_N_Value_n0_5 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0571 | CTRLMD_N_Value_n1 | Genesis8Female | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0572 | CTRLMD_N_XRotate_110 | lShin | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0573 | CTRLMD_N_XRotate_110 | rShin | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0574 | CTRLMD_N_XRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0575 | CTRLMD_N_XRotate_24 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0576 | CTRLMD_N_XRotate_24 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0577 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0578 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0579 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0580 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0581 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0582 | CTRLMD_N_XRotate_35 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0583 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0584 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0585 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0586 | CTRLMD_N_XRotate_35 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0587 | CTRLMD_N_XRotate_35 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0588 | CTRLMD_N_XRotate_35 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0589 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0590 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0591 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0592 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0593 | CTRLMD_N_XRotate_40 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0594 | CTRLMD_N_XRotate_n115 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0595 | CTRLMD_N_XRotate_n115 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0596 | CTRLMD_N_XRotate_n20 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0597 | CTRLMD_N_XRotate_n25 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0598 | CTRLMD_N_XRotate_n25 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0599 | CTRLMD_N_XRotate_n25 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0600 | CTRLMD_N_XRotate_n25 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0601 | CTRLMD_N_XRotate_n25 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0602 | CTRLMD_N_XRotate_n25 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0603 | CTRLMD_N_XRotate_n27 | head | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0604 | CTRLMD_N_XRotate_n27 | head | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0605 | CTRLMD_N_XRotate_n27 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0606 | CTRLMD_N_XRotate_n27 | neck_2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0607 | CTRLMD_N_XRotate_n70 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0608 | CTRLMD_N_XRotate_n70 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0609 | CTRLMD_N_XRotate_n90 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0610 | CTRLMD_N_XRotate_n90 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0611 | CTRLMD_N_XRotate_n90 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0612 | CTRLMD_N_XRotate_n90 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0613 | CTRLMD_N_YRotate_110 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0614 | CTRLMD_N_YRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0615 | CTRLMD_N_YRotate_15 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0616 | CTRLMD_N_YRotate_17 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0617 | CTRLMD_N_YRotate_20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0618 | CTRLMD_N_YRotate_20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0619 | CTRLMD_N_YRotate_22 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0620 | CTRLMD_N_YRotate_40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0621 | CTRLMD_N_YRotate_40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0622 | CTRLMD_N_YRotate_40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0623 | CTRLMD_N_YRotate_90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0624 | CTRLMD_N_YRotate_90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0625 | CTRLMD_N_YRotate_n110 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0626 | CTRLMD_N_YRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0627 | CTRLMD_N_YRotate_n15 | pelvis | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0628 | CTRLMD_N_YRotate_n17 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0629 | CTRLMD_N_YRotate_n20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0630 | CTRLMD_N_YRotate_n20 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0631 | CTRLMD_N_YRotate_n22 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0632 | CTRLMD_N_YRotate_n40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0633 | CTRLMD_N_YRotate_n40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0634 | CTRLMD_N_YRotate_n40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0635 | CTRLMD_N_YRotate_n90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0636 | CTRLMD_N_YRotate_n90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0637 | CTRLMD_N_ZRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0638 | CTRLMD_N_ZRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0639 | CTRLMD_N_ZRotate_15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0640 | CTRLMD_N_ZRotate_20 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0641 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0642 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0643 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0644 | CTRLMD_N_ZRotate_24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0645 | CTRLMD_N_ZRotate_35 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0646 | CTRLMD_N_ZRotate_40 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0647 | CTRLMD_N_ZRotate_40 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0648 | CTRLMD_N_ZRotate_45 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0649 | CTRLMD_N_ZRotate_50 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0650 | CTRLMD_N_ZRotate_50 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0651 | CTRLMD_N_ZRotate_55 | lCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0652 | CTRLMD_N_ZRotate_60 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0653 | CTRLMD_N_ZRotate_70 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0654 | CTRLMD_N_ZRotate_80 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0655 | CTRLMD_N_ZRotate_85 | lThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0656 | CTRLMD_N_ZRotate_85 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0657 | CTRLMD_N_ZRotate_85 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0658 | CTRLMD_N_ZRotate_90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0659 | CTRLMD_N_ZRotate_90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0660 | CTRLMD_N_ZRotate_90 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0661 | CTRLMD_N_ZRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0662 | CTRLMD_N_ZRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0663 | CTRLMD_N_ZRotate_n15 | abdomenLower | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0664 | CTRLMD_N_ZRotate_n20 | chest | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0665 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0666 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0667 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0668 | CTRLMD_N_ZRotate_n24 | abdomen2 | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0669 | CTRLMD_N_ZRotate_n35 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0670 | CTRLMD_N_ZRotate_n40 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0671 | CTRLMD_N_ZRotate_n40 | neck | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0672 | CTRLMD_N_ZRotate_n45 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0673 | CTRLMD_N_ZRotate_n50 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0674 | CTRLMD_N_ZRotate_n50 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0675 | CTRLMD_N_ZRotate_n55 | rCollar | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0676 | CTRLMD_N_ZRotate_n60 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0677 | CTRLMD_N_ZRotate_n70 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0678 | CTRLMD_N_ZRotate_n80 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0679 | CTRLMD_N_ZRotate_n85 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0680 | CTRLMD_N_ZRotate_n85 | lShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0681 | CTRLMD_N_ZRotate_n85 | rThigh | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0682 | CTRLMD_N_ZRotate_n90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0683 | CTRLMD_N_ZRotate_n90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0684 | CTRLMD_N_ZRotate_n90 | rShldr | /Hidden/CTRLMDs | 控制器 | 否 |
| G8-0685 | Gums Smoother | Genesis8Female | /Hidden/Correctives/Aging Morphs | 控制器 | 否 |
| G8-0686 | JCMYouthChestAssist | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0687 | JCMYouthMorph Assist | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0688 | JCMYouthMouth | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0689 | Liloo Ears | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0690 | Liloo Head Neutralizer Assist | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0691 | Liloo Mouth Realism Null | geometry | /Hidden/Zev0/Liloo | 稀疏形态 | 否 |
| G8-0692 | SFHD Abdo L &amp; Pelvis Bend | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0693 | SFHD Abdo L &amp; Pelvis Bend Null | Genesis8Female | /Hidden/Skin Folds &amp; Creases HD/Base | 控制器 | 否 |
| G8-0694 | SFHD Abdo L &amp; U &amp; Pelvis Bend | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0695 | SFHD Abdo L &amp; U BS2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0696 | SFHD Abdo L &amp; U BS2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0697 | SFHD Abdo L &amp; U Twist &amp; U Bend Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0698 | SFHD Abdo L &amp; U Twist &amp; U Bend Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0699 | SFHD Abdo L &amp; U Twist Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0700 | SFHD Abdo L &amp; U Twist Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0701 | SFHD Abdo L BS2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0702 | SFHD Abdo L BS2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0703 | SFHD Abdo L S2S &amp; BT Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0704 | SFHD Abdo L S2S &amp; BT Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0705 | SFHD Abdo L S2SBT &amp; US2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0706 | SFHD Abdo L S2SBT &amp; US2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0707 | SFHD Abdo U &amp; L BBT Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0708 | SFHD Abdo U &amp; L BBT Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0709 | SFHD Abdo U BS2S &amp; BT Left Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0710 | SFHD Abdo U BS2S &amp; BT Right Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0711 | SFHD Abdo U BS2S Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0712 | SFHD Abdo U BS2S Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0713 | SFHD Abdo U BT Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0714 | SFHD Abdo U BT Left Null | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0715 | SFHD Abdo U BT OP Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0716 | SFHD Abdo U BT OP Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0717 | SFHD Abdo U BT Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0718 | SFHD Abdo U BT Right Null | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0719 | SFHD Arm Bend Back Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0720 | SFHD Arm Bend Back Left Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0721 | SFHD Arm Bend Back Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0722 | SFHD Arm Bend Back Right Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0723 | SFHD Chest Lower Bend Front Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0724 | SFHD Chest Lower Bend Front Left Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0725 | SFHD Chest Lower Bend Front Right Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0726 | SFHD Navel Fold | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0727 | SFHD Navel Fold Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0728 | SFHD Neck Twist Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0729 | SFHD Neck Twist Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0730 | SFHD Pelvis &amp; Abdo L Twist Left | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0731 | SFHD Pelvis &amp; Abdo L Twist Right | geometry | /Hidden/Skin Folds &amp; Creases HD/Base | 稀疏形态 | 否 |
| G8-0732 | SFHD Pelvis Bend Front Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0733 | SFHD Torso Bend Front Lower Navel Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0734 | SFHD Torso Bend Front U &amp; L &amp; P Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0735 | SFHD Torso Bend Front U &amp; L Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0736 | SFHD Torso Bend Front Upper &amp; SL Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0737 | SFHD Torso Bend Front Upper &amp; SR Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0738 | SFHD Torso Bend Side Lower Left &amp; U Abdo Bend Front Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0739 | SFHD Torso Bend Side Lower Right &amp; U Abdo Bend Front Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0740 | SFHD Torso Bend Side U &amp; L &amp; LF Left Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0741 | SFHD Torso Bend Side U &amp; L &amp; LF Right Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0742 | SFHD Torso Bend Side Upper Left &amp; Back Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0743 | SFHD Torso Bend Side Upper Left &amp; Twist Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0744 | SFHD Torso Bend Side Upper Left &amp; Twist Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0745 | SFHD Torso Bend Side Upper Left Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0746 | SFHD Torso Bend Side Upper Right &amp; Back Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0747 | SFHD Torso Bend Side Upper Right &amp; Twist Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0748 | SFHD Torso Bend Side Upper Right &amp; Twist Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0749 | SFHD Torso Bend Side Upper Right Folds Assist | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0750 | SFHD Torso Lower Bulge Folds | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0751 | SFHD Torso Upper Bulge Folds 1 | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0752 | SFHD Torso Upper Bulge Folds 2 | geometry | /Hidden/Skin Folds &amp; Creases HD/Folds | 稀疏形态 | 否 |
| G8-0753 | SH1&amp;2GenSmoother | geometry | /Hidden/Correctives/Aging Morphs | 稀疏形态 | 否 |
| G8-0754 | Youth Head Neutralize | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |
| G8-0755 | Youth Head Neutralize Assist | geometry | /Hidden/Growing Up | 稀疏形态 | 否 |

## 单独取样无明显形变（71 条）

在本次新增可编辑条目的测试中，单独输入 0.5 / 1（受参数范围限制）时，最大顶点位移小于 1e-6 米。可能需要配套体型、姿势或开关；不能据此断言参数永久无效。

| 编号 | 参数名称 | 所属节点 | DAZ 分组 | 类型 | 默认可见 |
| --- | --- | --- | --- | --- | --- |
| G8-0756 | Auto Life Body Shape Neutralize | Genesis8Female | /Full Body/Real World/Auto Life/Age | 稀疏形态 | 是 |
| G8-0757 | AutoLife_AFE_01smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0758 | AutoLife_AFE_05smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0759 | AutoLife_AFE_0smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0760 | AutoLife_AFE_100smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0761 | AutoLife_AFE_16smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0762 | AutoLife_AFE_30smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0763 | AutoLife_AFE_60smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0764 | AutoLife_AFE_80smF | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0765 | AutoLife_AFE_G8F30sm | Genesis8Female | /Hidden/D.Master/Auto Life/AFE | 控制器 | 否 |
| G8-0766 | DisparateLashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0767 | DT- Luna Eyelashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0768 | eCTRLGraceYongBrowDownL | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G8-0769 | eCTRLGraceYongBrowDownR | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G8-0770 | eCTRLGraceYongMouthCornerBackL | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G8-0771 | eCTRLGraceYongMouthCornerBackR | Genesis8Female | /Hidden/People/PCGraceYong/eCTRLs | 控制器 | 否 |
| G8-0772 | eCTRLHannMeiCornerBackL | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G8-0773 | eCTRLHannMeiCornerBackR | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G8-0774 | eCTRLHannMeiMouthCornerDownL | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G8-0775 | eCTRLHannMeiMouthCornerDownR | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G8-0776 | eCTRLHannMeiMouthSmile | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G8-0777 | eCTRLHannMeiMouthSmileOpen | Genesis8Female | /Hidden/People/RSHannMei/eJCM | 控制器 | 否 |
| G8-0778 | eCTRLKanade8Frown_HD | Genesis8Female | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G8-0779 | eCTRLKanade8LipsPart | Genesis8Female | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G8-0780 | eCTRLKanade8MouthFrown | Genesis8Female | /Hidden/People/Kanade 8/eJCMs | 稀疏形态 | 否 |
| G8-0781 | eCTRLMintoMouthSmileSimple_n100_L | Genesis8Female | /Hidden/People/Minto | 控制器 | 否 |
| G8-0782 | eCTRLMintoMouthSmileSimple_n100_R | Genesis8Female | /Hidden/People/Minto | 控制器 | 否 |
| G8-0783 | eCTRLNamCheeksBalloon | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G8-0784 | eCTRLNamEyesSquint | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G8-0785 | eCTRLNamMouthCornerBackL | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G8-0786 | eCTRLNamMouthCornerBackR | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G8-0787 | eCTRLNamMouthCornerDownL | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G8-0788 | eCTRLNamMouthCornerDownR | Genesis8Female | /Hidden/People/CSNam/eJCMs | 控制器 | 否 |
| G8-0789 | EJ Eyelashes 01 | Genesis8Female | /Hidden/EmmaAndJordi/Common | 控制器 | 否 |
| G8-0790 | eJCMMarisEyesRelax | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0791 | eJCMMarisLipsClosed-BareTeeth | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0792 | eJCMMarisLipsPart | Genesis8Female | /Hidden/People/Maris/eJCM | 稀疏形态 | 否 |
| G8-0793 | eJCMMarisLipsPartCenter | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0794 | eJCMMarisMouthSmile | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0795 | eJCMMarisMouthSmileSimple | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0796 | eJCMMarisMouthSmileSimpleL | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0797 | eJCMMarisMouthSmileSimpleR | Genesis8Female | /Hidden/People/Maris/eJCM | 控制器 | 否 |
| G8-0798 | Eyelashes Length Lower | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0799 | Eyelashes Length Upper | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0800 | Long Lashes by Hinky Punk | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0801 | MCMArinaEyesClosedL | Genesis8Female | /Hidden/People/Arina/MCMs | 稀疏形态 | 否 |
| G8-0802 | MCMArinaEyesClosedR | Genesis8Female | /Hidden/People/Arina/MCMs | 稀疏形态 | 否 |
| G8-0803 | MCMHana_EyeCloseLeft | geometry | /Hidden/Correctives/Hana | 稀疏形态 | 否 |
| G8-0804 | MCMHana_EyeCloseRight | geometry | /Hidden/Correctives/Hana | 稀疏形态 | 否 |
| G8-0805 | MCMKimSeohyunEyesClose_LashesL | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G8-0806 | MCMKimSeohyunEyesClose_LashesR | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G8-0807 | MCMKimSeohyunLashesFix | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G8-0808 | MCMMeiAsakuraEyesClosedL | Genesis8Female | /Hidden/People/Mei Asakura/MCMs | 稀疏形态 | 否 |
| G8-0809 | MCMMeiAsakuraEyesClosedR | Genesis8Female | /Hidden/People/Mei Asakura/MCMs | 稀疏形态 | 否 |
| G8-0810 | MCMMintoMouthSmileOpen_n50 | Genesis8Female | /Hidden/People/Minto | 控制器 | 否 |
| G8-0811 | MCMRumiEyesClosedL | Genesis8Female | /Hidden/People/Rumi/MCMs | 稀疏形态 | 否 |
| G8-0812 | MCMRumiEyesClosedR | Genesis8Female | /Hidden/People/Rumi/MCMs | 稀疏形态 | 否 |
| G8-0813 | MCMYujinLashesFit | Genesis8Female | /Hidden/People/Yujin | 控制器 | 是 |
| G8-0814 | MCMYujinLashesFitSmall | Genesis8Female | /Hidden/People/Yujin | 控制器 | 否 |
| G8-0815 | NMK Right Thumb Bends | Genesis8Female | /Pose Controls/Hands/Right/Nightmare Killer Glove | 控制器 | 是 |
| G8-0816 | PHMKimSeohyunLashes | Genesis8Female | /Hidden/People/KimSeohyun | 控制器 | 否 |
| G8-0817 | SC Jiao Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0818 | Teen Josie 8 Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0819 | Teen Kaylee 8 Lashes | Genesis8Female | /Real World | 控制器 | 是 |
| G8-0820 | Thigh bend Both+ | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0821 | Thigh bend Both- | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0822 | thigh Both +57 | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0823 | Thigh side Both | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0824 | Thight side Both TeenJosie | Genesis8Female | /Hidden/Correctives/Chungdan | 控制器 | 是 |
| G8-0825 | VOXiaoXin Closedl | geometry | /Hidden/People/VO Xiaoxin/pjCMs | 稀疏形态 | 否 |
| G8-0826 | VOXiaoXin Closedr | geometry | /Hidden/People/VO Xiaoxin/pjCMs | 稀疏形态 | 否 |

## 附录：未成功建立参数条目的资源

以下为读取阶段诊断，不计入以上条目数量；两个角色报告的是同一组源文件。

| 资源文件 | 读取失败原因 |
| --- | --- |
| H:/G3/data/DAZ 3D/Genesis 8/Female/Morphs/Raiya/Neko/eJCMNekoEyesClosedL.dsf | Morph 差值 count 不一致 |
| H:/G3/data/DAZ 3D/Genesis 8/Female/Morphs/Raiya/Neko/eJCMNekoEyesClosedR.dsf | Morph 差值 count 不一致 |
