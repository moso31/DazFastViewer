"""暂存已编译程序和确实物化的运行时依赖，不复制 LFS 指针。"""
from pathlib import Path
import shutil
import hashlib
import json
from datetime import datetime, timezone

root = Path(__file__).resolve().parents[1]
out = root / "out"
out.mkdir(exist_ok=True)
shutil.copy2(root / "build/bin/Release/CyclesViewportBench.exe", out)
libraries = root / ".research/windows-libs-metadata"
packages = "OpenImageIO tbb epoxy opencolorio openexr imath openjph fmt zlib zstd pugixml aom jpeg png webp openjpeg pystring yamlcpp".split()
for package in packages:
    for file in (libraries / package).rglob("*.dll"):
        with file.open("rb") as stream:
            if stream.read(2) != b"MZ":
                continue
        shutil.copy2(file, out / file.name)
(out / "lib").mkdir(exist_ok=True)
for file in (root / "build/cycles/src/kernel/device").rglob("*.zst"):
    shutil.copy2(file, out / "lib" / file.name)
shutil.copy2(root / ".deps/cycles/dfv-source-manifest.json", out)
license_dir = out / "licenses/nlohmann-json"
license_dir.mkdir(parents=True, exist_ok=True)
for name in ("LICENSE.MIT", "SOURCE.md"):
    shutil.copy2(root / "third_party/nlohmann" / name, license_dir / name)
source_files = [root / "CMakeLists.txt"]
for base in ("src", "cmake", "tools", "tests", "third_party"):
    source_files.extend(p for p in (root/base).rglob("*")
                        if p.is_file() and "__pycache__" not in p.parts)
manifest = {"staged_at": datetime.now(timezone.utc).isoformat(),
            "source_hashes": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in source_files},
            "exe_sha256": hashlib.sha256((out/"CyclesViewportBench.exe").read_bytes()).hexdigest(),
            "cycles_assembly": json.loads((out/"dfv-source-manifest.json").read_text()),
            "environment": json.loads((root/"specs/001-cycles-static-camera/evidence/environment.json").read_text(encoding="utf-8-sig")),
            "cuda": "12.9.41", "optix": "9.1.0"}
(out/"build-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+"\n",encoding="utf-8")
print(out)
