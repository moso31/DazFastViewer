"""一条命令执行指定资产的后台材质对照，日志和商业资产衍生物仅保存在 artifacts。"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

PROJECT = Path(__file__).resolve().parents[2]
SAMPLES = {
    "prop": (Path("H:/G3/Environments/Architecture/ARK Modern Modular Cafe/Props/Tabletop/ARK MM Cafe - Food Plate with Fries.duf"), Path("H:/G3")),
    "character": (Path("C:/Users/Public/Documents/My DAZ 3D Library/People/Genesis 8 Female/Genesis 8.1 Basic Female.duf"), Path("C:/Users/Public/Documents/My DAZ 3D Library")),
}


def run(command, log):
    print(f"执行：{log}", flush=True)
    with log.open("w", encoding="utf-8") as output:
        result = subprocess.run([str(x) for x in command], cwd=PROJECT, stdout=output, stderr=subprocess.STDOUT,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    if result.returncode:
        raise RuntimeError(f"命令失败（{result.returncode}），日志：{log}\n{log.read_text('utf-8', errors='replace')[-5000:]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sample", choices=["prop", "character", "both"], default="both")
    parser.add_argument("--blender", type=Path, default=Path("C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"))
    parser.add_argument("--output", type=Path, default=PROJECT / "artifacts/material-reference")
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--export-only", action="store_true", help="仅导出参数并检查结构，不执行渲染")
    parser.add_argument("--verify-delta", action="store_true", help="在道具同一 Session 中额外验证一次材质增量")
    args = parser.parse_args()
    if args.verify_delta and (args.export_only or args.sample == "character"):
        parser.error("--verify-delta 仅与道具渲染合用")
    for case in SAMPLES if args.sample == "both" else [args.sample]:
        file, root = SAMPLES[case]
        directory = args.output.resolve() / case
        directory.mkdir(parents=True, exist_ok=True)
        runtime, reference = directory / "runtime", directory / "reference"
        run([PROJECT / "out/CyclesViewportBench.exe", "--export-scene" if args.export_only else "--smoke",
             "--file", file, "--content-root", root, "--width", args.width, "--height", args.height,
             "--samples", args.samples, "--output", runtime,
             *(["--material-delta-check"] if args.verify_delta and case == "prop" else [])], directory / "runtime.log")
        run([args.blender, "--background", "--factory-startup", "--python-exit-code", 1,
             "--python", PROJECT / "tools/reference/blender_reference.py", "--", "--file", file,
             "--scene", runtime / "scene.json", "--content-root", root, "--output", reference,
             *(["--export-only"] if args.export_only else [])], directory / "reference.log")
        if not args.export_only:
            run([args.blender, "--background", "--factory-startup", "--python-exit-code", 1,
                 "--python", PROJECT / "tools/reference/compare_reference.py", "--", "--case", directory], directory / "comparison.log")
        print(f"完成：{directory}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
