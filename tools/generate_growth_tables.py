"""将用户提供的生长表固化为 C++ 常量；程序运行时不读取或执行 DSA。"""
import argparse
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
args = parser.parse_args()
source = args.source.read_text(encoding="utf-8-sig")
tables = dict((name, json.loads(data)) for name, data in re.findall(r"var (\w+) = (\[[\s\S]*?\]);", source))
bindings = re.findall(r'setProp\(oNode, "([^"]+)", getAgeValue\((\w+), targetAge\)', source)
lines = ["#pragma once", "// 来源：用户 growage_common.dsa；仅提取数值表，不执行脚本。", "namespace dfv::runtime {", "struct GrowthTable {const char *label;double values[5][20];};", "inline constexpr GrowthTable growth_tables[]={"]
for label, name in bindings:
    values = tables.get(name + "_all", [tables.get(name)] * 5)
    assert len(values) == 5 and all(len(row) == 20 for row in values)
    rows = ",".join("{" + ",".join(str(v) for v in row) + "}" for row in values)
    lines.append('  {"' + label + '",{' + rows + '}},')
lines += ["};", "}", ""]
(Path(__file__).resolve().parents[1] / "src/runtime/growth_tables.h").write_text("\n".join(lines), encoding="utf-8")
