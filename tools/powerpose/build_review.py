"""从已解码的 G8.1 模板生成 009 人工核对页、逐方向清单及审阅数据。"""
from __future__ import annotations

import argparse
import base64
import csv
import gzip
import html
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

AXES = ("lmb_horiz", "lmb_vert", "rmb_horiz", "rmb_vert")
DIRECTIONS = (("上", "vert", -1), ("下", "vert", 1), ("左", "horiz", -1), ("右", "horiz", 1))
LABELS = {
    "Figure": "角色整体", "Rotate Figure": "旋转角色整体", "Translate Figure": "平移角色整体",
    "Translate Hip": "平移髋部", "Hip": "髋部", "Pelvis": "骨盆", "Head": "头部", "Neck": "下颈部",
    "Neck Lower": "下颈部", "Neck Upper": "上颈部", "Chest Lower": "下胸部", "Chest Upper": "上胸部",
    "Abdomen Lower": "下腹部", "Abdomen Upper": "上腹部", "Neck Group": "头颈联动组",
    "Torso Group": "躯干联动组", "Legs Group": "双腿联动组", "Shoulders Group": "双肩联动组",
    "Eyes Group": "双眼联动组", "lEye": "角色左眼", "rEye": "角色右眼",
    "Body Controls": "转到 Body", "Head Controls": "转到 Head", "Hand Controls": "转到 Hands", "Face Controls": "Face 入口（不支持）",
}
for side, cn in (("Left", "左"), ("Right", "右")):
    for part, meaning in (("Collar", "锁骨"), ("Shoulder", "上臂"), ("Shoulder Bend", "上臂"), ("Forearm", "前臂"), ("Forearm Bend", "前臂"), ("Hand", "手腕"), ("Thigh", "大腿"), ("Thigh Bend", "大腿"), ("Shin", "小腿"), ("Foot", "脚踝"), ("Toes", "脚趾"), ("Arm Group", "手臂联动组"), ("Leg Group", "腿部联动组"), ("Hand Group", "整手联动组")):
        LABELS[side + " " + part] = "角色" + cn + meaning
    prefix = side[0].lower()
    LABELS[prefix + "Hand"] = "角色" + cn + "手腕"
    for finger, name in (("Pinky", "小指"), ("Ring", "无名指"), ("Mid", "中指"), ("Index", "食指"), ("Thumb", "拇指")):
        for segment, desc in ((1, "近节"), (2, "中节"), (3, "末节")):
            LABELS[f"{prefix}{finger}{segment}"] = cn + name + desc
        LABELS[side + " " + ("Middle" if finger == "Mid" else finger) + " Curl"] = cn + name + "三节联动"


def leaves(node):
    return {c.tag: c.text or "" for c in node if not len(c)}


def load_dson(path):
    data = path.read_bytes()
    return json.loads(gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data)


def resolve(nodes, raw_node, raw_prop):
    """记录原生通道；工具通道代理只提出候选，不伪装成 DAZ 动态观测。"""
    if raw_node == "Figure":
        node = next(n for n in nodes if n.get("type") == "figure")
    else:
        node = next((n for n in nodes if raw_node.casefold() in [str(n.get(k, "")).casefold() for k in ("id", "name", "label")] + [s.casefold() for s in n.get("name_aliases", [])]), None)
    if not node:
        return {"name": raw_node, "property": raw_prop, "note": "节点别名待验证", "locked": False}
    axis = None
    group = "rotation"
    prop = raw_prop.lower()
    if prop in ("xrot", "yrot", "zrot"):
        axis = prop[0]
    elif prop in ("firstrot", "secondrot", "thirdrot"):
        axis = node["rotation_order"][("firstrot", "secondrot", "thirdrot").index(prop)].lower()
    elif prop in ("xtranslate", "ytranslate", "ztranslate"):
        axis, group = prop[0], "translation"
    channels = node.get(group, [])
    channel = next((c for c in channels if c["id"] == axis or c.get("label", "").casefold() == prop), None)
    result = {"name": node["id"], "property": raw_prop, "note": "", "locked": False}
    if channel:
        result.update(axis=channel["id"], label=channel.get("label", channel["id"]), group=group,
                      minimum=channel.get("min"), maximum=channel.get("max"), locked=channel.get("locked", False))
        if group == "rotation" and result["locked"]:
            candidates = {("lShldr", "x"): "lShldrTwist", ("rShldr", "x"): "rShldrTwist",
                          ("lThigh", "y"): "lThighTwist", ("rThigh", "y"): "rThighTwist",
                          ("lForeArm", "x"): "lWrist", ("rForeArm", "x"): "rWrist"}
            candidate = candidates.get((node["id"], channel["id"]))
            if candidate:
                result["tool_candidate"] = candidate
                result["note"] = "工具旋转可能转交 " + candidate + "；待 DAZ 实测"
            else:
                result["note"] = "基础 DSF 此轴锁定，预案不改变该轴；仍需核对工具映射"
    return result


def effect(binding, direction):
    positive = binding["sign"] * direction > 0
    change = "增加" if positive else "减小"
    resolved = binding["resolved"]
    name = LABELS.get(binding["node"], binding["node"])
    prop = binding["property"]
    if resolved.get("tool_candidate"):
        return name + "扭转值" + change + "〔代理待核〕"
    if resolved.get("locked"):
        return name + "无变化〔" + resolved.get("axis", prop).upper() + "轴锁定〕"
    if prop.lower().endswith("translate"):
        axis = prop[0].upper()
        return name + "沿 " + ("+" if positive else "−") + axis + " 平移"
    node, axis = resolved.get("name", ""), resolved.get("axis")
    hint = ""
    if node in ("head", "neck", "neck_2", "abdomenLower", "abdomen2", "chest", "chest_2"):
        if axis == "x":
            hint = ("低头" if positive else "抬头") if node == "head" else ("前屈" if positive else "后伸")
        elif axis == "y":
            hint = "向角色左转" if positive else "向角色右转"
        elif axis == "z":
            hint = "向角色右侧倾" if positive else "向角色左侧倾"
    elif node in ("lEye", "rEye"):
        hint = ("向上看" if positive else "向下看") if axis == "x" else ("向角色右看" if positive else "向角色左看") if axis == "y" else ""
    elif re.fullmatch(r"[lr](Index|Mid|Ring|Pinky)[123]", node) and axis == "z":
        hint = "屈曲" if positive == node.startswith("r") else "伸展"
    elif re.fullmatch(r"[lr]Thumb[123]", node) and axis == "y":
        hint = "屈曲" if positive == node.startswith("l") else "伸展"
    elif node in ("lShin", "rShin") and axis == "x":
        hint = "屈膝" if positive else "伸膝"
    elif node in ("lThigh", "rThigh") and axis == "x":
        hint = "大腿向后摆" if positive else "大腿向前抬"
    label = resolved.get("label", prop)
    translated = {"Bend": "屈伸", "Twist": "扭转", "Side-Side": "侧摆", "Front-Back": "前后摆", "Up-Down": "上下摆", "X Rotate": "X旋转", "Y Rotate": "Y旋转", "Z Rotate": "Z旋转"}.get(label, label)
    return name + translated + "值" + change + ("（" + hint + "）" if hint else "")


def collect(decoded, template_dir, nodes):
    templates = []
    omitted = []
    for template, prefix in (("Body", "B"), ("Hands", "H"), ("Head", "N")):
        path = decoded / ("G8_1F_" + template + ".dsx")
        root = ET.parse(path).getroot()
        points = []
        for category, kind in (("node_points", "node"), ("node_group_points", "group"), ("property_points", "property"), ("template_points", "template")):
            for element in root.findall(category + "/*"):
                values = leaves(element)
                if template == "Head" and kind == "property":
                    omitted.append(values)
                    continue
                raw_label = values.get("label") or values.get("node_label")
                point = {"kind": kind, "x": int(values["x"]), "y": int(values["y"]), "label": LABELS.get(raw_label, raw_label), "raw_label": raw_label, "raw": values, "bindings": {}}
                if kind == "template":
                    point["target"] = values["tpl_label"]
                    point["disabled"] = values["tpl_label"] == "Face"
                for control in AXES:
                    bindings = []
                    members = element.findall("group_members/node_data") if kind == "group" else [element]
                    for member in members:
                        v = leaves(member)
                        prop = v.get(control + "_prop")
                        if not prop:
                            continue
                        node = v.get(control + "_node") if kind == "property" else v.get("node_label")
                        bindings.append({"node": node, "property": prop, "sign": -1 if v.get(control + "_sign") == "neg" else 1, "size": float(v.get(control + "_size", 300)), "size_explicit": control + "_size" in v, "resolved": resolve(nodes, node, prop)})
                    point["bindings"][control] = bindings
                points.append(point)
        points.sort(key=lambda p: (p["y"], p["x"]))
        counts = {"template": 0, "edit": 0}
        for point in points:
            which = "template" if point["kind"] == "template" else "edit"
            counts[which] += 1
            point["id"] = prefix + ("T" if which == "template" else "") + f"{counts[which]:02d}"
            point["directions"] = {}
            for button in ("lmb", "rmb"):
                for direction, axis, sign in DIRECTIONS:
                    bindings = point["bindings"][button + "_" + axis]
                    point["directions"][button + direction] = "；".join(effect(b, sign) for b in bindings) if bindings else "无姿态变化"
        image = template_dir / root.findtext("bg_file")
        templates.append({"name": template, "width": int(root.findtext("width")), "height": int(root.findtext("height")), "points": points, "counts": counts, "image": "data:image/png;base64," + base64.b64encode(image.read_bytes()).decode()})
    return templates, omitted


def write_markdown(templates, dest):
    lines = ["# 009 PowerPose：逐点、逐方向人工核对清单", "", "由 `tools/powerpose/build_review.py` 从本机已解码的 G8.1 模板生成。交互预案尚待人工验证；不代表编辑器已实现。", "", "原始坐标以模板画布 **480 × 720** 左上角为原点，X 向右、Y 向下，不含 Source 和 Controls。左右肢体均指角色自身；Hands 左上是左手、右下是右手。", "", "**读表约定：** 向右／向下取正鼠标增量，乘 DXE 的 `sign`；向左／向上反向。表中“值增加／减小”指目标参数数值，不一律等于弯曲／伸直。全局鼠标符号和工具代理尚待 DAZ 实测。所有方向都受实际资产锁定与限位约束。", "", "`〔代理待核〕` 是工具旋转访问器与基础骨骼通道不一致的候选映射，不应直接写入基础 DSF 的锁定轴。原始绑定、方向符号、size 均可在交互核对页展开。", "", "单击姿态点只选择；拖动才改值。右键未拖动弹出点菜单，右键拖动不弹菜单。导航点左键单击切页，按住拖动及右键均不改姿态；Face 不跳转。这些点击规则是本项目预案。", ""]
    for template in templates:
        lines.extend(["## " + template["name"], "", "| 编号 | 名称 | 坐标 (x, y) | 类型 |", "|---|---|---|---|"])
        for p in template["points"]:
            lines.append(f'| {p["id"]} | {p["label"]} | ({p["x"]}, {p["y"]}) | ' + {"node": "实心圆／节点", "group": "虚线圈／节点组", "property": "菱形／属性", "template": "三角／导航"}[p["kind"]] + " |")
        lines.append("")
        for p in template["points"]:
            lines.extend([f'### {p["id"]} · {p["label"]} · ({p["x"]}, {p["y"]})', "", f'官方名称：`{p["raw_label"]}`。', ""])
            if p["kind"] == "template":
                lines.extend([("左键单击：无跳转，提示 Face 不在本阶段范围。" if p.get("disabled") else "左键单击：切换到 " + p["target"] + "，保持姿态。"), "右键单击：无操作。左键／右键向上、下、左、右拖动：均无姿态变化，也不以拖动触发导航。", ""])
                continue
            lines.extend(["| 按键 | 向上 | 向下 | 向左 | 向右 |", "|---|---|---|---|---|"])
            for key, label in (("lmb", "左键拖动"), ("rmb", "右键拖动")):
                lines.append("| " + label + " | " + " | ".join(p["directions"][key + direction] for direction, _, _ in DIRECTIONS) + " |")
            lines.extend(["", "人工结果：□ 未核对　□ 一致　□ 有差异；备注：________", ""])
    dest.write_text("\n".join(lines), encoding="utf-8")


def write_html(templates, path):
    # 数据是本机已验证模板；仍对嵌入脚本的结束标签进行转义。
    data = json.dumps(templates, ensure_ascii=False).replace("</", "<\\/")
    source = Path(__file__).with_name("review_template.html").read_text(encoding="utf-8")
    path.write_text(source.replace("/* TEMPLATE_DATA */[]", data), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoded", type=Path, default=Path("artifacts/009-powerpose/g81"))
    parser.add_argument("--template-dir", required=True, type=Path)
    parser.add_argument("--figure", required=True, type=Path)
    args = parser.parse_args()
    templates, omitted = collect(args.decoded, args.template_dir, load_dson(args.figure)["node_library"])
    expected = {"Body": 36, "Hands": 44, "Head": 5}
    for template in templates:
        assert template["counts"] == {"edit": expected[template["name"]], "template": 2}
        assert all(0 <= p["x"] <= 480 and 0 <= p["y"] <= 720 for p in template["points"])
        assert len({p["id"] for p in template["points"]}) == len(template["points"])
    assert len(omitted) == 14
    spec = Path("specs/009-powerpose")
    spec.mkdir(parents=True, exist_ok=True)
    output = Path("artifacts/009-powerpose")
    output.mkdir(parents=True, exist_ok=True)
    write_markdown(templates, spec / "interaction-map.md")
    write_html(templates, output / "review.html")
    dataset = [{k: v for k, v in t.items() if k != "image"} for t in templates]
    (output / "review-data.json").write_text(json.dumps({"status": "awaiting_manual_review", "templates": dataset, "omitted_head_properties": omitted}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with (spec / "manual-review.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["模板", "编号", "名称", "X", "Y", "左上", "左下", "左左", "左右", "右上", "右下", "右左", "右右", "结果", "备注"])
        for t in templates:
            for p in t["points"]:
                writer.writerow([t["name"], p["id"], p["label"], p["x"], p["y"], *[p["directions"][button + direction] for button in ("lmb", "rmb") for direction, _, _ in DIRECTIONS], "未核对", ""])
    print(json.dumps({"editable_points": sum(t["counts"]["edit"] for t in templates), "navigation_points": 6, "direction_cases": 85 * 8, "omitted_head_properties": len(omitted), "review": str(output / "review.html")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
