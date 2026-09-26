"""根据材质清点选取预设，使用正式 C++ 导入器检查，并记录成功与失败。"""
import argparse
import gzip
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", required=True)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    audit = json.loads(Path(args.audit).read_text(encoding="utf-8"))
    selected = ["H:/g3/Shader Presets/SF-Design/Universal Hair Shader Presets/UH 01.duf"]
    features = ["Hair Root Color", "Root Transmission Color", "Line Start Width", "Diffuse Overlay Weight",
                "Specular Lobe 1 Roughness", "Emission Color", "Transmitted Color", "Top Coat Weight",
                "Normal Map", "Bump Strength", "Cutout Opacity", "Thin Walled", "displacement", "glossiness"]
    for feature in features:
        info = audit["channels"].get(feature, {})
        for name in (info.get("examples", []) if isinstance(info, dict) else []):
            if name in selected:
                continue
            raw = Path(name).read_bytes()
            data = json.loads(gzip.decompress(raw) if raw[:2] == b"\x1f\x8b" else raw)
            kind = data.get("asset_info", {}).get("type", "")
            if kind in ("preset_material", "preset_shader") and not data.get("scene", {}).get("nodes"):
                selected.append(name)
                break
    for category in ("preset_material", "preset_shader", "hierarchical", "makeup", "eyes", "nails", "prop", "skin"):
        examples = audit.get("examples", {}).get(category, [])
        for example in examples[:1] + examples[-1:]:
            if example["file"] not in selected:
                selected.append(example["file"])
    # 常见通道的前几个示例往往来自完整场景；另外选取两库的独立材质预设。
    for root in audit["roots"]:
        found = set()
        categories = {"characters", "clothing", "hair", "environments"}
        for folder, _, files in os.walk(root):
            parts = {part.lower() for part in Path(folder).parts}
            matches = (categories & parts) - found
            if not matches or not any("material" in part or "iray" in part for part in parts):
                continue
            for name in sorted(files):
                if not name.lower().endswith(".duf"):
                    continue
                file = Path(folder) / name
                try:
                    raw = file.read_bytes()
                    data = json.loads(gzip.decompress(raw) if raw[:2] == b"\x1f\x8b" else raw)
                    if data.get("asset_info", {}).get("type") not in ("preset_material", "preset_shader") or data.get("scene", {}).get("nodes"):
                        continue
                except (ValueError, OSError):
                    continue
                if str(file) not in selected:
                    selected.append(str(file))
                found.update(matches)
                break
            if found == categories:
                break
    env = os.environ.copy()
    env["PATH"] = "C:/Qt/6.10.3/msvc2022_64/bin;" + env["PATH"]
    env["QT_QPA_PLATFORM"] = "offscreen"
    report = []
    for file in selected:
        run = subprocess.run([args.executable, "--library", file], env=env, capture_output=True, timeout=90)
        out = run.stdout.decode("utf-8", errors="replace")
        record = dict(file=file, returncode=run.returncode, stderr=run.stderr.decode("utf-8", errors="replace"))
        if run.returncode == 0:
            record["result"] = json.loads(out)
        else:
            record["stdout"] = out
        report.append(record)
    Path(args.output).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("samples", len(report), "passed", sum(item["returncode"] == 0 for item in report))
    raise SystemExit(any(item["returncode"] != 0 for item in report))


if __name__ == "__main__":
    main()
