"""从只读场景提取一个角色及全部挂接物，用于 StrandBasedHair 的 IK 专项验证。"""
import gzip
import hashlib
import json
import sys
from pathlib import Path
from urllib.parse import unquote

source, destination, figure = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
data = source.read_bytes()
document = json.loads(gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data)
scene = document["scene"]
kept = {figure}
reference = lambda value: unquote(value).removeprefix("#")
while True:
    before = len(kept)
    for node in scene["nodes"]:
        if reference(node.get("parent", "")) in kept or reference(node.get("conform_target", "")) in kept:
            kept.add(node["id"])
    if len(kept) == before:
        break
scene["nodes"] = [node for node in scene["nodes"] if node["id"] in kept]
geometries = {g["id"] for n in scene["nodes"] for g in n.get("geometries", [])}
scene["modifiers"] = [m for m in scene.get("modifiers", []) if reference(m.get("parent", "")) in kept | geometries]
scene["materials"] = [m for m in scene.get("materials", []) if reference(m.get("geometry", "")) in geometries]
owners = kept | geometries | {m["id"] for m in scene["modifiers"]}
scene["animations"] = [a for a in scene.get("animations", []) if reference(a["url"].split(":", 1)[0]) in owners]
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(document, ensure_ascii=False), encoding="utf-8")
destination.with_suffix(".source.json").write_text(json.dumps({"source": str(source), "sha256": hashlib.sha256(data).hexdigest(), "figure": figure, "nodes": len(kept), "geometries": sorted(geometries)}, indent=2), encoding="utf-8")
print(destination)
