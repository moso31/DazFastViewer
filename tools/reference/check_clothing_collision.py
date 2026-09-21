"""用 Blender BVH 独立测量原生服装输出的近表面穿入量；不替换 Runtime 顶点。"""
import argparse
import gzip
import json
import sys
from pathlib import Path
from urllib.parse import unquote

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def read(path):
    data = Path(path).read_bytes()
    return json.loads(gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("geometry", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--body", required=True)
    parser.add_argument("--cloth", required=True)
    parser.add_argument("--content-root", action="append", required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    native = read(args.geometry)
    source = read(native["input"])
    objects = {o["label"]: o for o in native["objects"]}
    body, cloth = objects[args.body], objects[args.cloth]

    def geometry(obj):
        node_id, _, mesh_id = obj["id"].rpartition("/")
        node = next(n for n in source["scene"]["nodes"] if n["id"] == node_id)
        uri = next(g["url"] for g in node["geometries"] if g["id"] == mesh_id)
        file, _, fragment = unquote(uri).partition("#")
        doc = source if not file else read(next(Path(root) / file.lstrip("/")
            for root in args.content_root if (Path(root) / file.lstrip("/")).is_file()))
        return node, next(g for g in doc["geometry_library"] if g["id"] == fragment)

    _, base = geometry(body)
    grafts, hidden = [], set()
    for obj in native["objects"]:
        node, asset = geometry(obj)
        graft = asset.get("graft", {})
        if (unquote(node.get("conform_target", "")) == "#" + body["id"].rpartition("/")[0]
                and graft.get("vertex_count") == len(body["world"])):
            grafts.append(obj)
            hidden.update(graft["hidden_polys"]["values"])
    vertices = list(body["world"])
    faces = []
    for index, poly in enumerate(base["polylist"]["values"]):
        if index not in hidden:
            faces.extend((poly[2], poly[k], poly[k + 1]) for k in range(3, len(poly) - 1))
    for graft in grafts:
        offset = len(vertices)
        vertices.extend(graft["world"])
        faces.extend(tuple(offset + v for v in f) for f in graft["triangles"])
    bvh = BVHTree.FromPolygons(vertices, faces, all_triangles=True)
    base_bvh = BVHTree.FromPolygons(body["world"], body["triangles"], all_triangles=True)

    def measure(positions):
        positions = np.asarray(positions)
        samples = [positions]
        triangles = positions[np.asarray(cloth["triangles"])]
        # 每个面的规则重心网格独立于 Runtime 的四个采样点。
        for i in range(5):
            for j in range(5 - i):
                samples.append(triangles[:, 0] * (i / 4) + triangles[:, 1] * (j / 4)
                               + triangles[:, 2] * (1 - (i + j) / 4))
        depths = []
        samples = np.concatenate(samples)
        for p in samples:
            inward = 0.0
            for surface in (base_bvh, bvh):
                q, normal, _, _ = surface.find_nearest(Vector(p))
                inward = max(inward, -(Vector(p) - q).dot(normal))
            depths.append(inward * 1000)
        depths = np.asarray(depths)
        return {"samples": len(depths), "inward_over_0_5mm": int((depths > .5).sum()),
                "inward_over_2mm": int((depths > 2).sum()), "max_inward_mm": float(depths.max()),
                "worst_world_m": samples[depths.argmax()].tolist()}

    delta = np.linalg.norm(np.asarray(cloth["world"]) - cloth["uncorrected_world"], axis=1)
    report = {"method": "Blender BVH nearest-face signed distance; open seams are not a solid-volume certificate",
              "input": str(args.geometry), "body": args.body, "cloth": args.cloth,
              "grafts": [g["label"] for g in grafts], "before": measure(cloth["uncorrected_world"]),
              "after": measure(cloth["world"]), "corrected_vertices": int((delta > 1e-6).sum()),
              "max_correction_mm": float(delta.max() * 1000),
              "body_max_difference_m": float(np.linalg.norm(np.asarray(body["world"])
                                                            - body["uncorrected_world"], axis=1).max())}
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
