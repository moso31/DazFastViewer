"""在 Blender Python 中读取线性 EXR，生成结构差异、图像指标和本地对照页。"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import sys

import bpy
import numpy as np


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def nodes(tree):
    if tree:
        for node in tree["nodes"]:
            yield node
            yield from nodes(node.get("group"))


def load_image(path):
    image = bpy.data.images.load(str(path.resolve()), check_existing=False)
    image.colorspace_settings.name = "Linear Rec.709"
    width, height = image.size
    pixels = np.empty(width * height * image.channels, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    result = pixels.reshape(height, width, image.channels)[:, :, :3].copy()
    bpy.data.images.remove(image)
    if not np.isfinite(result).all():
        raise RuntimeError(f"图像存在 NaN/Inf：{path}")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--case", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    directory = args.case
    contract = read(directory / "runtime/scene.json")
    check = read(directory / "reference/reference-check.json")
    if check.get("render_status") != "COMPLETE":
        raise RuntimeError("参考渲染尚未完成")
    scene_hash = hashlib.sha256((directory / "runtime/scene.json").read_bytes()).hexdigest()
    if scene_hash != check["scene_sha256"]:
        raise RuntimeError("参考与 Runtime 的场景契约不一致，拒绝比较旧图")
    reference = read(directory / "reference/reference-materials.json")
    refs = {m["name"]: m for m in reference["materials"]}
    mapping = {key: val for item in check["checks"] for key, val in item["material_slots"].items()}
    differences = []
    sockets = {"roughness": "Roughness", "metallic": "Metallic", "opacity": "Alpha",
               "transmission": "Transmission Weight", "ior": "IOR", "base_color": "Base Color"}
    for mat in contract["materials"]:
        if mat["id"] == "preview-floor":
            continue
        ref = refs.get(mat["id"]) or refs.get(mapping.get(mat["id"]))
        if ref is None:
            raise RuntimeError(f"缺少参考材质：{mat['id']}")
        graph_nodes = list(nodes(ref["graph"]))
        principals = [n for n in graph_nodes if n["type"] == "ShaderNodeBsdfPrincipled"]
        constants, linked = [], []
        if len(principals) == 1:
            inputs = {i["name"]: i for i in principals[0]["inputs"]}
            for key, socket in sockets.items():
                target = inputs[socket]
                if target["linked"]:
                    linked.append(socket)
                    continue
                expected = target["value"][:3] if key == "base_color" else target["value"]
                if not np.allclose(mat[key], expected, atol=1e-6):
                    constants.append({"parameter": key, "runtime": mat[key], "reference": expected})
        reference_bumps = [{i["name"]: i["value"] for i in n["inputs"] if not i["linked"]}
                           for n in graph_nodes if n["type"] == "ShaderNodeBump"]
        ref_textures = {str(Path(n["image"]["file"]).resolve()).casefold(): n["image"] for n in graph_nodes if "image" in n}
        runtime_textures = {str(Path(contract["textures"][mat[key]]["file"]).resolve()).casefold()
                            for key in ("color_texture", "roughness_texture", "opacity_texture", "normal_texture", "bump_texture") if mat[key] >= 0}
        differences.append({"id": mat["id"], "reference": ref["name"], "constant_differences": constants,
                            "linked_principled_inputs_need_semantic_comparison": linked,
                            "bump_runtime": {"strength": mat["bump_strength"], "distance_m": mat["bump_distance"], "texture": mat["bump_texture"]},
                            "bump_reference": reference_bumps,
                            "reference_textures_missing_in_runtime": [v for k, v in ref_textures.items() if k not in runtime_textures],
                            "reference_node_types": sorted({n["type"] for n in graph_nodes})})
    actual = load_image(directory / "runtime/smoke.exr")
    expected = load_image(directory / "reference/reference.exr")
    if actual.shape != expected.shape:
        raise RuntimeError("两侧图像尺寸不一致")
    error = np.abs(actual - expected)
    rmse = float(np.sqrt(np.mean(np.square(error))))
    # 同时记录资产包围框内的误差，避免大面积背景稀释角色材质差异。
    camera = np.eye(4)
    camera[:3] = np.array(contract["camera"]["transform"]).reshape(3, 4)
    inverse = np.linalg.inv(camera)
    projected = []
    height, width = actual.shape[:2]
    tangent = np.tan(contract["camera"]["fov_short_axis"] / 2)
    for instance in contract["instances"]:
        if instance["id"] == "preview-floor":
            continue
        transform = np.eye(4)
        transform[:3] = np.array(instance["transform"]).reshape(3, 4)
        vertices = np.array(contract["meshes"][instance["mesh"]]["positions"])
        points = np.c_[vertices, np.ones(len(vertices))] @ (inverse @ transform).T
        points = points[points[:, 2] > 1e-5]
        x = (points[:, 0] / points[:, 2] / tangent / max(width / height, 1) + 1) * width / 2
        y = (points[:, 1] / points[:, 2] / tangent / max(height / width, 1) + 1) * height / 2
        projected.extend(zip(x, y))
    projected = np.array(projected)
    low = np.maximum(np.floor(projected.min(axis=0)).astype(int), [0, 0])
    high = np.minimum(np.ceil(projected.max(axis=0)).astype(int), [width, height])
    crop = error[low[1]:high[1], low[0]:high[0]]
    if not crop.size:
        raise RuntimeError("资产包围框在相机外")
    # 无合格阈值时仅报告差异，不产生误导性的 Golden PASS。
    report = {"status": "DIFFERENCES_RECORDED_NOT_GOLDEN_ACCEPTED", "structure": check["checks"],
              "linear_rgb": {"mae": float(error.mean()), "rmse": rmse,
                             "p95_absolute_error": float(np.percentile(error, 95)), "max_absolute_error": float(error.max()),
                             "fraction_pixels_error_above_0_05": float(np.mean(error.max(axis=2) > .05))},
              "asset_bounding_rectangle": {"pixel_bounds_bottom_origin": [*low.tolist(), *high.tolist()],
                                           "mae": float(crop.mean()), "rmse": float(np.sqrt(np.mean(np.square(crop)))),
                                           "note": "投影包围矩形，包含空隙和背景，不是对象遮罩"},
              "material_differences": differences,
              "limitations": ["完整参考材质与 Runtime 子集比较，随机噪声也计入误差", "尚未建立视觉 Golden 合格阈值", "凹凸距离、皮肤散射、额外高光等仍需对齐"],
              "files_sha256": {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                               (directory / "runtime/scene.json", directory / "runtime/smoke.exr", directory / "reference/reference.exr")}}
    (directory / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    title = "角色材质对照" if directory.name == "character" else "道具材质对照"
    rows = "".join(f"<tr><td>{html.escape(m['id'])}</td><td>{len(m['constant_differences'])}</td>"
                   f"<td>{len(m['reference_textures_missing_in_runtime'])}</td><td>{len(m['linked_principled_inputs_need_semantic_comparison'])}</td></tr>" for m in differences)
    page = f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>{title}</title>
<style>body{{font:16px/1.6 system-ui;background:#181a20;color:#eee;margin:30px}}.pair{{display:flex;gap:20px}}figure{{margin:0;flex:1;min-width:0}}img{{width:100%}}a{{color:#8cbcff}}td,th{{padding:6px 18px;border-bottom:1px solid #555;text-align:left}}</style>
<h1>{title}</h1><p>基础顶点、UV 与材质绑定已核对；材质仍有差异，尚未通过 Golden 验收。</p>
<div class="pair"><figure><figcaption>独立 Runtime</figcaption><img src="runtime/smoke.png"></figure>
<figure><figcaption>Blender + DAZ Importer 参考</figcaption><img src="reference/reference.png"></figure></div>
<p>尺寸 {actual.shape[1]}×{actual.shape[0]}，{contract['render']['samples']} samples，线性 RGB MAE {error.mean():.6f}，RMSE {rmse:.6f}。数值包含采样噪声。</p>
<p>资产投影包围矩形内：MAE {crop.mean():.6f}。矩形仍包含空隙；整图误差会被大面积背景稀释。</p>
<p><a href="comparison.json">完整差异报告</a> · <a href="reference/reference-materials.json">参考材质节点</a> · <a href="runtime/asset-report.json">兼容性诊断</a></p>
<table><tr><th>材质</th><th>直接常量差异</th><th>参考独有贴图</th><th>需进一步分析的连接输入</th></tr>{rows}</table>
<p>表中零项不代表材质等价：节点连接、凹凸尺度、皮肤散射和额外高光仍需结合完整报告审查。</p></html>'''
    (directory / "comparison.html").write_text(page, encoding="utf-8")
    print(json.dumps(report["linear_rgb"]))


if __name__ == "__main__":
    main()
