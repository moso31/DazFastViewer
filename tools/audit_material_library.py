"""只读清点 DSON 材质预设与通道；支持 gzip，不加载贴图或执行脚本。"""
import argparse
import collections
import gzip
import json
import os
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("roots", nargs="+")
    parser.add_argument("--output", required=True)
    parser.add_argument("--per-folder", type=int, default=1, help="每目录读取数量；0 表示读取所有候选文件")
    args = parser.parse_args()
    report = dict(roots=args.roots, per_folder=args.per_folder, scanned=0, skipped_data_dsf=0, skipped_samples=0, materials=0, material_files=0,
                  skipped_large=[], errors=[], shaders={}, channels={}, asset_types={}, material_shapes={}, dependencies={})
    channels = collections.defaultdict(lambda: dict(count=0, types={}, examples=[], values=[]))
    shaders = collections.Counter()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    last_progress = time.monotonic()
    for root in args.roots:
        for folder, _, files in os.walk(root):
            sampled = 0
            for name in sorted(files):
                if not name.lower().endswith((".duf", ".dsf")):
                    continue
                file = Path(folder) / name
                report["scanned"] += 1
                if time.monotonic() - last_progress > 10:
                    progress = {k: report[k] for k in ("scanned", "skipped_data_dsf", "material_files", "materials")}
                    progress["folder"] = folder
                    output.with_suffix(".progress.json").write_text(json.dumps(progress, ensure_ascii=False), encoding="utf-8")
                    print(progress["scanned"], progress["material_files"], flush=True)
                    last_progress = time.monotonic()
                # data 中的 DSF 是几何、蒙皮和 Morph；材质以场景 DUF 或材质目录 DSF 发布。
                # 明确记录排除数量，清点报告不声称检查了每个几何数据文件。
                relative = str(file.relative_to(root)).lower().replace("\\", "/")
                if name.lower().endswith(".dsf") and relative.startswith("data/") and not any(part in relative for part in ("material", "shader")):
                    report["skipped_data_dsf"] += 1
                    continue
                if args.per_folder and sampled >= args.per_folder:
                    report["skipped_samples"] += 1
                    continue
                sampled += 1
                try:
                    with file.open("rb") as stream:
                        zipped = stream.read(2) == b"\x1f\x8b"
                    with (gzip.open(file, "rb") if zipped else file.open("rb")) as stream:
                        raw = stream.read(32 * 1024 * 1024 + 1)
                    if len(raw) > 32 * 1024 * 1024:
                        report["skipped_large"].append(str(file))
                        continue
                    # 跳过不含材质的形态/骨骼文件，避免构建大 JSON 对象。
                    if b'"material_library"' not in raw and b'"materials"' not in raw:
                        continue
                    data = json.loads(raw)
                    materials = data.get("material_library", []) + data.get("scene", {}).get("materials", [])
                    if materials:
                        report["material_files"] += 1
                        kind = data.get("asset_info", {}).get("type", "unknown")
                        report["asset_types"][kind] = report["asset_types"].get(kind, 0) + 1
                    for material in materials:
                        report["materials"] += 1
                        shape = "groups" if material.get("groups") else "url-only" if material.get("url") else "ungrouped"
                        report["material_shapes"][shape] = report["material_shapes"].get(shape, 0) + 1
                        url = material.get("url", "")
                        if url:
                            dependency = url.split("#")[0] or "local material_library"
                            report["dependencies"][dependency] = report["dependencies"].get(dependency, 0) + 1
                        groups = [material]
                        for extra in material.get("extra", []):
                            if not isinstance(extra, dict):
                                continue
                            shaders[extra.get("type", "unknown")] += 1
                            groups.extend(extra.get("channels", []))
                        groups.extend(v for v in material.values() if isinstance(v, dict))
                        for entry in groups:
                            if not isinstance(entry, dict):
                                continue
                            channel = entry.get("channel", {})
                            if not isinstance(channel, dict) or not channel.get("id"):
                                continue
                            info = channels[channel["id"]]
                            info["count"] += 1
                            value = channel.get("current_value", channel.get("value"))
                            kind = channel.get("type", type(value).__name__)
                            info["types"][kind] = info["types"].get(kind, 0) + 1
                            if len(info["examples"]) < 3 and str(file) not in info["examples"]:
                                info["examples"].append(str(file))
                            if len(info["values"]) < 5 and value not in info["values"]:
                                info["values"].append(value)
                except Exception as error:
                    report["errors"].append(dict(file=str(file), error=str(error)))
                if report["scanned"] % 5000 == 0:
                    print(report["scanned"], report["material_files"], flush=True)
    report["shaders"] = dict(shaders.most_common())
    report["channels"] = dict(sorted(channels.items(), key=lambda item: -item[1]["count"]))
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k not in ("channels", "shaders", "errors", "skipped_large")}, ensure_ascii=False))
    print("channels", len(channels), "errors", len(report["errors"]), "large", len(report["skipped_large"]))


if __name__ == "__main__":
    main()
