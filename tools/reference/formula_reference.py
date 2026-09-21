"""独立读取 DSF 差值 / 权重，以 mathutils 和 NumPy 对照 ERC 后的缩放与蒙皮结果。"""
import argparse
import json
import math
from pathlib import Path
import sys
from urllib.parse import unquote

import bpy
import numpy as np
from mathutils import Euler, Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pose_reference import read, vector, multiply


def formula_channels(contract, raw, catalog=None):
    """从资源重算直接输出；其他激活形态使用导出权重，默认控制器使用资源初值。"""
    edited = contract.get("edited_parameter")
    if not edited:
        return dict(checked=0, max_error=0)
    poses = {p["id"]: p for p in contract["joints"]}
    modifier = next(m for m in read(edited["source"])["modifier_library"] if m["id"] == edited["id"])
    expected = {}

    def address(uri):
        decoded = unquote(uri)
        ident, prop = decoded.split("?", 1)
        return ident.split("#")[-1], prop.removesuffix("/value")

    def expression(formula, inputs):
        stack = []
        for operation in formula["operations"]:
            op = operation["op"]
            if op == "push":
                if "url" in operation:
                    key = address(operation["url"])
                    if key not in inputs:
                        return None
                    stack.append(inputs[key])
                elif isinstance(operation["val"], (int, float)):
                    stack.append(operation["val"])
                else:
                    return None
            elif op == "mult":
                stack.append(stack.pop()*stack.pop())
            else:
                return None
        return stack[0] if len(stack) == 1 else None

    for formula in modifier.get("formulas", []):
        key = address(formula["output"])
        if key[0] not in poses or formula.get("stage", "sum") != "sum":
            continue
        value = expression(formula, {(edited["id"], "value"): edited["value"]})
        if value is not None:
            expected[key] = expected.get(key, 1.0 if key[1].startswith("scale/") else 0.0) + value
    direct_checked = len(expected)
    # 同一骨骼属性可以同时接收多个形态的贡献。旧检查只计算当前滑块，
    # 遗漏了 Yuki 的 Body / Head / Youth 链及 G8.1 默认开启的 CTRL Vo Xiao Mei。
    additional = {(m["source"], m["id"]): m["weight"] for m in contract["morphs"]}
    if catalog:
        for target in catalog["targets"]:
            for m in target["morphs"]:
                if m.get("evaluable") and m["kind"] != "alias" and not m["offsets"] and m["initial"]:
                    additional.setdefault((m["source"], m["channel_id"]), m["initial"])
    additional.pop((edited["source"], edited["id"]), None)
    contributors = []
    for (source, ident), weight in additional.items():
        asset = next(m for m in read(source)["modifier_library"] if m["id"] == ident)
        count = 0
        for formula in asset.get("formulas", []):
            key = address(formula["output"])
            if key not in expected or formula.get("stage", "sum") != "sum":
                continue
            value = expression(formula, {(ident, "value"): weight})
            if value is not None:
                expected[key] += value
                count += 1
        if count:
            contributors.append(dict(id=ident, weight=weight, channels=count))
    # 从已知缩放输入拓展依赖，每个节点 mult 只合并一次。
    pending = [f for n in raw["node_library"] for f in n.get("formulas", []) if f.get("stage") == "mult"]
    while pending:
        remaining = []
        for formula in pending:
            value = expression(formula, expected)
            key = address(formula["output"])
            if value is None or not key[1].startswith("scale/"):
                remaining.append(formula)
            else:
                expected[key] = expected.get(key, 1.0)*value
        if len(remaining) == len(pending):
            break
        pending = remaining
    errors = []
    for (ident, prop), value in expected.items():
        field, axis = prop.split("/")
        field = {"center_point": "center_offset", "end_point": "end_offset", "orientation": "orientation_offset"}.get(field, field)
        actual = poses[ident]["general_scale"] if axis == "general" else poses[ident][field]["xyz".index(axis)]
        errors.append(abs(value-actual))
    if edited["id"] in ("SCLArmsLength", "SCLPropagatingChest", "CRTLHSSannyShy") and not errors:
        raise RuntimeError("直接控制器缺少独立公式检查")
    return dict(checked=len(errors), direct_checked=direct_checked, additional_contributors=contributors,
                max_error=max(errors, default=0))


def check(path):
    contract = read(path)
    raw = read(contract["dsf"])
    catalog_path = path.parent/"morph-catalog.json"
    channels = formula_channels(contract, raw, read(catalog_path) if catalog_path.exists() else None)
    skin = next(m["skin"] for m in raw["modifier_library"] if "skin" in m)
    geometry = next(g for g in raw["geometry_library"] if g["id"] == skin["geometry"].split("#")[-1])
    daz = np.asarray(geometry["vertices"]["values"], dtype=np.float32)
    source = daz[:, [0, 2, 1]] * np.array([.01, -.01, .01], dtype=np.float32)
    for morph in contract["morphs"]:
        asset = next(m for m in read(morph["source"])["modifier_library"] if m["id"] == morph["id"])
        rows = np.asarray(asset["morph"]["deltas"]["values"], dtype=np.float32)
        indices = rows[:, 0].astype(int)
        delta = rows[:, [1, 3, 2]] * np.array([.01, -.01, .01], dtype=np.float32)
        source[indices] += delta * np.float32(morph["weight"])
    original = source[:, [0, 2, 1]].astype(float) * np.array([100, 100, -100])
    nodes = {n["id"]: n for n in raw["node_library"]}
    poses = {p["id"]: p for p in contract["joints"]}
    root = skin["node"].split("#")[-1]
    states = {}

    def evaluate(ident):
        if ident in states:
            return states[ident]
        node, pose = nodes[ident], poses[ident]
        center = (vector(node, "center_point").astype(np.float32) + np.array(pose["center_offset"], dtype=np.float32)).astype(float)
        angles = (vector(node, "orientation").astype(np.float32) + np.array(pose["orientation_offset"], dtype=np.float32)).astype(float)
        orient = np.array(Euler(tuple(angles*math.pi/180), "XYZ").to_matrix())
        local_rotation = np.array(Euler(tuple(np.asarray(pose["rotation"])*math.pi/180), node.get("rotation_order", "XYZ")).to_matrix())
        local_scale = np.diag((np.asarray(pose["scale"], dtype=np.float32)*np.float32(pose["general_scale"])).astype(float))
        rotation = orient @ local_rotation @ orient.T
        scale = orient @ local_scale @ orient.T
        origin = center + pose["translation"]
        rest_origin = center.copy()
        if ident != root:
            parent = evaluate(node["parent"].split("#")[-1])
            delta = center-parent["center"]
            origin = parent["rotation"] @ parent["scale"] @ (delta + pose["translation"]) + parent["origin"]
            rest_origin = parent["scale"] @ delta + parent["rest_origin"]
            rotation = parent["rotation"] @ rotation
            inherited = parent["scale"]
            if not node.get("inherits_scale", parent["root"]):
                inherited = inherited @ np.linalg.inv(parent["local_scale"])
            scale = inherited @ scale
        quat = np.asarray(Matrix(rotation.tolist()).to_quaternion(), dtype=float)
        quat /= np.linalg.norm(quat)
        translation = origin - rotation @ rest_origin
        result = dict(center=center, local_scale=local_scale, rotation=rotation, scale=scale,
                      origin=origin, rest_origin=rest_origin, root=ident == root,
                      quat=quat, dual=.5*multiply(np.r_[0, translation], quat), stretch=rest_origin-scale@center)
        states[ident] = result
        return result

    joints = []
    reference_q = np.zeros((len(source), 4))
    largest = np.zeros(len(source))
    for joint in skin["joints"]:
        state = evaluate(joint["node"].split("#")[-1])
        weights = np.asarray(joint.get("node_weights", {}).get("values", []), dtype=float).reshape((-1, 2))
        joints.append((state, weights))
        indices = weights[:, 0].astype(int)
        chosen = weights[:, 1] > largest[indices]
        reference_q[indices[chosen]] = state["quat"]
        largest[indices[chosen]] = weights[chosen, 1]
    real, dual = np.zeros_like(reference_q), np.zeros_like(reference_q)
    stretched = np.zeros_like(original)
    total = np.zeros(len(source))
    for state, weights in joints:
        indices, factors = weights[:, 0].astype(int), weights[:, 1].copy()
        stretched[indices] += factors[:, None] * (original[indices] @ state["scale"].T + state["stretch"])
        total[indices] += factors
        factors[np.sum(reference_q[indices]*state["quat"], axis=1) < 0] *= -1
        real[indices] += factors[:, None]*state["quat"]
        dual[indices] += factors[:, None]*state["dual"]
    bound = total > 0
    stretched[bound] /= total[bound, None]
    norm = np.linalg.norm(real[bound], axis=1, keepdims=True)
    real[bound] /= norm
    dual[bound] /= norm
    dual -= real*np.sum(real*dual, axis=1, keepdims=True)
    inverse = real*np.array([1, -1, -1, -1])
    result = multiply(multiply(real, np.column_stack((np.zeros(len(source)), stretched))), inverse) + 2*multiply(dual, inverse)
    result[~bound, 1:] = original[~bound]
    expected = result[:, [1, 3, 2]]*np.array([.01, -.01, .01])
    errors = np.linalg.norm(expected-np.asarray(contract["positions"]), axis=1)
    return dict(case=str(path), max_vertex_error_m=float(errors.max()), rms_vertex_error_m=float(np.sqrt(np.mean(errors**2))),
                morphs=len(contract["morphs"]), vertices=len(source), formula_channels=channels,
                status="PASS" if errors.max() < 2e-5 and channels["max_error"] < 1e-5 else "FAIL")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--")+1:])
    files = sorted(args.directory.rglob("*.deformation.json"))
    if not files:
        raise RuntimeError("没有变形对照数据")
    results = [check(p) for p in files]
    report = dict(blender=bpy.app.version_string, threshold_m=2e-5, results=results,
                  scope="raw direct-controller formulas plus active-morph weights and default-controller contributions; node-scale propagation; DSF morph accumulation and two-phase DQS with evaluated channels; not a full independent ERC graph or DAZ Studio Golden",
                  status="PASS" if all(r["status"] == "PASS" for r in results) else "FAIL")
    (args.directory/"reference-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=True, indent=2))
    if report["status"] != "PASS":
        raise RuntimeError("独立变形数学对照失败")


if __name__ == "__main__":
    main()
