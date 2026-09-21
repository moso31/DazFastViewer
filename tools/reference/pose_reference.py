"""用 Blender mathutils 独立读取 DSF / 姿势，检查基础 DQS；不渲染、不加载用户偏好。"""
import argparse
import gzip
import json
import math
from pathlib import Path
import sys
from urllib.parse import unquote

import bpy
import numpy as np
from mathutils import Euler


def read(path):
    data = Path(path).read_bytes()
    return json.loads(gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data)


def vector(node, name, default=(0, 0, 0)):
    values = dict(zip("xyz", default))
    for channel in node.get(name, []):
        values[channel["id"]] = channel.get("current_value", channel.get("value", 0))
    return np.array([values[axis] for axis in "xyz"], dtype=np.float32).astype(float)


def multiply(a, b):
    """批量 Hamilton 乘积，四元数顺序 wxyz。"""
    return np.concatenate((a[..., :1]*b[..., :1] - np.sum(a[..., 1:]*b[..., 1:], axis=-1, keepdims=True),
                           a[..., :1]*b[..., 1:] + b[..., :1]*a[..., 1:] + np.cross(a[..., 1:], b[..., 1:])), axis=-1)


def reference(export):
    raw = read(export["dsf"])
    binding = next(m["skin"] for m in raw["modifier_library"] if "skin" in m)
    nodes = {node["id"]: node for node in raw["node_library"]}
    names = {}
    for ident, node in nodes.items():
        for name in [node.get("name", ident), *node.get("name_aliases", [])]:
            names[name] = ident
    root = binding["node"].split("#")[-1]
    poses = {ident: {"rotation": vector(node, "rotation"), "translation": vector(node, "translation")} for ident, node in nodes.items()}
    poses[root] = {"rotation": np.zeros(3), "translation": np.zeros(3)}
    for animation in read(export["pose"])["scene"]["animations"]:
        address, prop = unquote(animation["url"]).split("?")
        if "#" in address:
            continue
        name = address.split("@selection", 1)[1].strip("/:")
        ident = names[name] if name else root
        group, axis, _ = prop.split("/")
        if group not in ("rotation", "translation"):
            raise RuntimeError(f"参考暂不支持 {prop}")
        poses[ident][group]["xyz".index(axis)] = float(np.float32(animation["keys"][0][1]))

    matrices = {}

    def calculate(ident):
        if ident in matrices:
            return matrices[ident]
        node, pose = nodes[ident], poses[ident]
        center = vector(node, "center_point")
        orientation = np.array(Euler(tuple(vector(node, "orientation") * math.pi / 180), "XYZ").to_matrix())
        local = np.array(Euler(tuple(pose["rotation"] * math.pi / 180), node.get("rotation_order", "XYZ")).to_matrix())
        rot = orientation @ local @ orientation.T
        origin = center + pose["translation"]
        if ident != root:
            parent_id = node["parent"].split("#")[-1]
            parent_rot, parent_origin, _ = calculate(parent_id)
            origin = parent_rot @ (center - vector(nodes[parent_id], "center_point") + pose["translation"]) + parent_origin
            rot = parent_rot @ rot
        matrices[ident] = rot, origin, origin - rot @ center
        return matrices[ident]

    # 用独立原始 DSF 顶点验证坐标转换，避免直接拿 Runtime 的输入掩盖错误。
    geometry_id = binding["geometry"].split("#")[-1]
    geometry = next(g for g in raw["geometry_library"] if g["id"] == geometry_id)
    source = np.asarray(geometry["vertices"]["values"], dtype=np.float32)
    source_ir = source[:, [0, 2, 1]] * np.array([.01, -.01, .01], dtype=np.float32)
    input_error = float(np.max(np.abs(source_ir - np.asarray(export["source"]))))
    if input_error > 1e-6:
        raise RuntimeError(f"基础顶点不一致：{input_error} m")
    original = source_ir[:, [0, 2, 1]].astype(float) * np.array([100, 100, -100])
    # 与运行时采用不同遍历：逐关节累计 NumPy 顶点数组。
    from mathutils import Matrix
    joints = []
    for joint in binding["joints"]:
        rot, _, trans = calculate(joint["node"].split("#")[-1])
        quat = np.array(Matrix(rot.tolist()).to_quaternion(), dtype=float)
        quat /= np.linalg.norm(quat)
        dual = .5 * multiply(np.r_[0, trans], quat)
        weights = np.array(joint.get("node_weights", {}).get("values", []), dtype=float).reshape((-1, 2))
        joints.append((quat, dual, weights))
    reference_q = np.zeros((len(original), 4))
    largest = np.zeros(len(original))
    for quat, _, weights in joints:
        indices = weights[:, 0].astype(int)
        chosen = weights[:, 1] > largest[indices]
        reference_q[indices[chosen]] = quat
        largest[indices[chosen]] = weights[chosen, 1]
    real, dual = np.zeros_like(reference_q), np.zeros_like(reference_q)
    for quat, dq, weights in joints:
        indices, factors = weights[:, 0].astype(int), weights[:, 1].copy()
        factors[np.sum(reference_q[indices]*quat, axis=1) < 0] *= -1
        real[indices] += factors[:, None] * quat
        dual[indices] += factors[:, None] * dq
    bound = largest > 0
    length = np.linalg.norm(real[bound], axis=1, keepdims=True)
    real[bound] /= length
    dual[bound] /= length
    dual -= real * np.sum(real*dual, axis=1, keepdims=True)
    inverse = real * np.array([1, -1, -1, -1])
    points = np.column_stack((np.zeros(len(original)), original))
    result = multiply(multiply(real, points), inverse) + 2*multiply(dual, inverse)
    result[~bound, 1:] = original[~bound]
    expected = result[:, [1, 3, 2]] * np.array([.01, -.01, .01])
    errors = np.linalg.norm(expected - np.asarray(export["positions"]), axis=1)
    return {"pose": export["pose"], "source_error_m": input_error, "max_vertex_error_m": float(errors.max()),
            "rms_vertex_error_m": float(np.sqrt(np.mean(errors**2))), "vertices": len(original),
            "status": "PASS" if errors.max() < 2e-5 else "FAIL"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--")+1:])
    files = sorted(args.directory.rglob("*.mesh.json"))
    if not files:
        raise RuntimeError("没有找到运行时几何导出")
    results = [reference(read(path)) for path in files]
    report = {"blender": bpy.app.version_string, "scope": "DSF / pose + independent mathutils and NumPy DQS; not DAZ Studio Golden",
              "threshold_m": 2e-5, "results": results, "status": "PASS" if all(r["status"] == "PASS" for r in results) else "FAIL"}
    (args.directory / "reference-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=True, indent=2))
    if report["status"] != "PASS":
        raise RuntimeError("基础蒙皮数学对照未通过")


if __name__ == "__main__":
    main()
