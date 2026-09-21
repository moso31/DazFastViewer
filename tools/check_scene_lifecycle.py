"""在副屏验证反复增删、贴图释放、场景替换及渲染错误恢复。"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="artifacts/scene-lifecycle/final-ui")
    parser.add_argument("--cycles", type=int, default=8, choices=range(1, 101))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = (root / args.output).resolve()
    fixtures = output / "fixtures"
    fixtures.mkdir(parents=True, exist_ok=True)
    for name, color, size in [("first", [180, 60, 30], 100), ("second", [30, 70, 210], 65)]:
        document = {
            "asset_info": {"type": "scene"},
            "geometry_library": [{
                "id": "mesh",
                "vertices": {"count": 3, "values": [[-size, 0, 0], [size, 0, 0], [0, size * 2, 0]]},
                "polygon_material_groups": {"count": 1, "values": ["Skin"]},
                "polylist": {"count": 1, "values": [[0, 0, 0, 1, 2]]},
                "default_uv_set": "#uv",
            }],
            "uv_set_library": [{"id": "uv", "vertex_count": 3, "uvs": {
                "count": 3, "values": [[0, 0], [1, 0], [.5, 1]],
            }}],
            "modifier_library": [
                {"id": "Cancel", "parent": "#mesh", "channel": {
                    "type": "float", "value": 0, "min": 0, "max": 1, "clamped": True},
                 "morph": {"vertex_count": 3, "deltas": {"count": 1, "values": [[0, 20, 0, 0]]}},
                 "formulas": [{"output": "#Cancel?value", "operations": [{"op": "push", "url": "#Driver?value"}]}]},
                {"id": "Driver", "parent": "#mesh", "channel": {"type": "float", "value": 1}},
            ],
            "scene": {
                "nodes": [{"id": name, "geometries": [{"id": "mesh-instance", "url": "#mesh"}]}],
                "modifiers": [{"url": "#Cancel", "parent": "#mesh-instance", "channel": {"current_value": -1}}],
                "materials": [{"id": "Skin", "geometry": "#mesh-instance", "groups": ["Skin"],
                               "diffuse": {"channel": {"value": [1, 1, 1], "image_file": f"{name}.ppm"}}}],
            },
        }
        (fixtures / f"{name}.duf").write_text(json.dumps(document), encoding="utf-8")
        (fixtures / f"{name}.ppm").write_bytes(b"P6\n1024 1024\n255\n" + bytes(color) * (1024 * 1024))
    with (output / "editor.log").open("w", encoding="utf-8") as log:
        subprocess.run([
            str(root / "out/DazFastViewer.exe"), "--file", str(fixtures / "first.duf"),
            "--lifecycle-test", str(fixtures / "second.duf"), "--output", str(output),
            "--content-root", str(fixtures),
            "--lifecycle-rounds", str(args.cycles),
        ], cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=660, check=True)
    report = json.loads((output / "editor-check.json").read_text(encoding="utf-8"))
    if report["status"] != "PASS" or not report["retired_document_expired"]:
        raise RuntimeError(report.get("error") or "旧文档没有释放")
    empty = [sample for sample in report["samples"] if sample["stage"] % 6 == 3]
    if len(empty) != args.cycles or any(sample[key] for sample in empty for key in ("objects", "meshes", "materials", "textures")):
        raise RuntimeError("循环次数不足或空场景仍持有资源")
    if any(sample["textures"] != 1 for sample in report["samples"] if sample["stage"] % 6 == 0):
        raise RuntimeError("测试未覆盖真实贴图")
    summary = {"status": "PASS", "cycles": args.cycles, "state_samples": len(report["samples"]),
               "old_document_released": True, "empty_resource_counts_zero": True,
               "empty_private_MiB": [round(sample["private_bytes"] / 2**20, 2) for sample in empty],
               "scope": "副屏真实 Cycles 与贴图；内存包括驱动和分配器缓存，不代表任意资产的长期零泄漏证明。"}
    (output / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Scene lifecycle: PASS ({args.cycles} cycles); empty private MiB: {summary['empty_private_MiB']}")


if __name__ == "__main__":
    main()
