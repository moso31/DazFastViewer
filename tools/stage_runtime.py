"""暂存已编译程序和确实物化的运行时依赖，不复制 LFS 指针。"""
from pathlib import Path
import shutil
import hashlib
import json
import subprocess
from datetime import datetime, timezone

root = Path(__file__).resolve().parents[1]
out = root / "out"
out.mkdir(exist_ok=True)


def stage_file(source, destination):
    destination = destination / source.name if destination.is_dir() else destination
    if destination.is_file() and source.stat().st_size == destination.stat().st_size:
        if hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(destination.read_bytes()).digest():
            return
    try:
        shutil.copy2(source, destination)
    except PermissionError:
        if destination.suffix.lower() != ".exe":
            raise
        # Windows 允许重命名正在运行的 EXE，旧会话继续使用原映像。
        # 保留备份，不终止用户进程；替换失败时恢复原路径。
        backup = destination.with_name(destination.stem + ".previous-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f") + ".exe")
        destination.rename(backup)
        try:
            shutil.copy2(source, destination)
        except Exception:
            if destination.exists():
                destination.unlink()
            backup.rename(destination)
            raise


stage_file(root / "build/bin/Release/CyclesViewportBench.exe", out)
editor = root / "build/bin/Release/DazFastViewer.exe"
qt_root = Path("C:/Qt/6.10.3/msvc2022_64")
cache = (root / "build/CMakeCache.txt").read_text(encoding="utf-8")
for line in cache.splitlines():
    if line.startswith("DFV_QT_ROOT:PATH="):
        qt_root = Path(line.split("=", 1)[1])
if editor.exists():
    stage_file(editor, out)
    subprocess.run([str(qt_root / "bin/windeployqt.exe"), "--release", "--no-compiler-runtime", "--no-opengl-sw",
                    "--translations", "zh_CN", "--dir", str(out), str(out / editor.name)], check=True)
    qt_license = out / "licenses/qt"
    qt_license.mkdir(parents=True, exist_ok=True)
    for item in (root / "third_party/qt").glob("*"):
        if item.is_file():
            shutil.copy2(item, qt_license / item.name)
    for module in ("qtbase", "qtsvg", "qttranslations"):
        for item in (qt_root / "sbom").glob(f"{module}-*.spdx*"):
            shutil.copy2(item, qt_license / item.name)
libraries = root / ".research/windows-libs-metadata"
packages = "OpenImageIO tbb epoxy opencolorio openexr imath openjph fmt zlib zstd pugixml aom jpeg png webp openjpeg pystring yamlcpp".split()
for package in packages:
    for file in (libraries / package).rglob("*.dll"):
        with file.open("rb") as stream:
            if stream.read(2) != b"MZ":
                continue
        stage_file(file, out / file.name)
(out / "lib").mkdir(exist_ok=True)
for file in (root / "build/cycles/src/kernel/device").rglob("*.zst"):
    stage_file(file, out / "lib" / file.name)
shutil.copy2(root / ".deps/cycles/dfv-source-manifest.json", out)
license_dir = out / "licenses/nlohmann-json"
license_dir.mkdir(parents=True, exist_ok=True)
for name in ("LICENSE.MIT", "SOURCE.md"):
    shutil.copy2(root / "third_party/nlohmann" / name, license_dir / name)
source_files = [root / "CMakeLists.txt"]
subdiv_license = out / "licenses/opensubdiv"
subdiv_license.mkdir(parents=True, exist_ok=True)
for item in (root / "third_party/opensubdiv").iterdir():
    if item.is_file():
        shutil.copy2(item, subdiv_license / item.name)
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
if editor.exists():
    manifest["editor_exe_sha256"] = hashlib.sha256((out/editor.name).read_bytes()).hexdigest()
    manifest["qt_root"] = str(qt_root)
(out/"build-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+"\n",encoding="utf-8")
print(out)
