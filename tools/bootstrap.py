"""Prepare pinned dependencies inside .deps; no admin rights or system PATH changes."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / ".deps"
LOCK = json.loads((ROOT / "tools/dependencies.lock.json").read_text(encoding="utf-8"))


def run(*args, **kwargs):
    print("+", " ".join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def download(url, path, sha256, *, offline=False, headers=None, size=None):
    path = Path(path)
    if path.is_file() and digest(path) == sha256:
        return path
    if offline:
        raise RuntimeError(f"Missing or corrupt offline file: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    partial = path.with_suffix(path.suffix + ".part")
    for attempt in range(3):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=headers or {}), timeout=60) as response, partial.open("wb") as output:
                shutil.copyfileobj(response, output, 1024 * 1024)
            if digest(partial) != sha256 or (size is not None and partial.stat().st_size != size):
                raise RuntimeError(f"Checksum/size mismatch: {path.name}")
            partial.replace(path)
            return path
        except Exception:
            if attempt == 2:
                raise
            time.sleep(2)


def extract_zip(archive, destination):
    """Strip one archive root, reject absolute paths, traversal and symlinks."""
    destination = Path(destination).resolve()
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            parts = Path(item.filename.replace("\\", "/")).parts
            if not parts or Path(item.filename).is_absolute() or ".." in parts or any(":" in p for p in parts):
                raise RuntimeError(f"Unsafe archive entry: {item.filename}")
            if (item.external_attr >> 16) & 0o170000 == 0o120000:
                raise RuntimeError(f"Archive symlink is unsupported: {item.filename}")
            if item.is_dir() or len(parts) < 2:
                continue
            target = destination.joinpath(*parts[1:]).resolve()
            if not target.is_relative_to(destination):
                raise RuntimeError(f"Archive escapes destination: {item.filename}")
            target.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)


def prepare_archive(name, spec, offline):
    destination = DEPS / spec["path"]
    marker = destination / f".dfv-{name}.json"
    if marker.is_file() and json.loads(marker.read_text()) == spec and (destination / spec["check"]).is_file():
        return
    archive = download(spec["url"], DEPS / "downloads" / f"{name}.zip", spec["sha256"], offline=offline)
    print(f"Extracting {name}", flush=True)
    extract_zip(archive, destination)
    marker.write_text(json.dumps(spec, indent=2) + "\n", encoding="utf-8")


def git(repo, *args, capture=False):
    # Do not depend on Git LFS being installed, or alter global Git configuration.
    command = ["git", "-c", "filter.lfs.required=false", "-c", "filter.lfs.smudge=", "-c", "filter.lfs.process=", "-C", str(repo), *args]
    return subprocess.run(command, check=True, stdout=subprocess.PIPE if capture else None).stdout


def prepare_git(spec, offline):
    destination = DEPS / spec["path"]
    if not (destination / ".git").exists():
        if offline:
            raise RuntimeError(f"Missing offline repository: {destination}")
        destination.mkdir(parents=True, exist_ok=True)
        git(destination, "init")
        git(destination, "remote", "add", "origin", spec["url"])
    has_commit = subprocess.run(["git", "-C", str(destination), "cat-file", "-e", spec["revision"] + "^{commit}"], capture_output=True).returncode == 0
    if not has_commit:
        if offline:
            raise RuntimeError(f"Missing commit {spec['revision']} in {destination}")
        git(destination, "fetch", "--depth", "1", "origin", spec["revision"])
    head = subprocess.run(["git", "-C", str(destination), "rev-parse", "HEAD"], capture_output=True, text=True)
    if head.returncode == 0 and head.stdout.strip() != spec["revision"]:
        raise RuntimeError(f"Unexpected HEAD in {destination}; use a fresh dependency directory.")
    marker = destination / ".git/dfv-prepared.json"
    if not marker.exists() or json.loads(marker.read_text()) != spec:
        if spec.get("sparse"):
            git(destination, "sparse-checkout", "set", *spec["sparse"])
        git(destination, "checkout", "--detach", spec["revision"])
        marker.write_text(json.dumps(spec), encoding="utf-8")
    return destination


def materialize_lfs(repo, spec, offline):
    """Use the public LFS batch API; each object is verified against the Git pointer."""
    pending = {}
    for package in spec["sparse"]:
        folder = repo / package
        if not folder.is_dir():
            raise RuntimeError(f"Dependency package is missing: {folder}")
        for path in folder.rglob("*"):
            if not path.is_file() or path.stat().st_size > 1024:
                continue
            data = path.read_bytes()
            if not data.startswith(b"version https://git-lfs.github.com/spec/v1"):
                continue
            pointer = dict(line.split(" ", 1) for line in data.decode("ascii").strip().splitlines())
            oid = pointer["oid"].removeprefix("sha256:")
            pending.setdefault(oid, {"oid": oid, "size": int(pointer["size"]), "paths": []})["paths"].append(path)
    if not pending:
        print("LFS dependencies already materialized", flush=True)
        return
    if offline:
        raise RuntimeError(f"{len(pending)} Git LFS objects are still pointers; run bootstrap online first.")
    total = sum(obj["size"] for obj in pending.values())
    print(f"Materializing {len(pending)} LFS objects ({total / 1024**2:.0f} MiB)", flush=True)
    objects = list(pending.values())
    for offset in range(0, len(objects), 50):
        batch = [{"oid": o["oid"], "size": o["size"]} for o in objects[offset:offset + 50]]
        payload = json.dumps({"operation": "download", "transfers": ["basic"], "objects": batch}).encode()
        request = urllib.request.Request(spec["url"] + "/info/lfs/objects/batch", data=payload,
                                         headers={"Content-Type": "application/vnd.git-lfs+json", "Accept": "application/vnd.git-lfs+json"})
        with urllib.request.urlopen(request, timeout=60) as response:
            answer = json.load(response)
        if {o["oid"] for o in answer["objects"]} != {o["oid"] for o in batch}:
            raise RuntimeError("The LFS server did not return every requested object; retry bootstrap.")

        def materialize(obj):
            if "error" in obj:
                raise RuntimeError(f"LFS error: {obj['error']}")
            expected = pending[obj["oid"]]
            action = obj["actions"]["download"]
            paths = expected["paths"]
            download(action["href"], paths[0], obj["oid"], headers=action.get("header"), size=expected["size"])
            for path in paths[1:]:
                shutil.copy2(paths[0], path)

        with ThreadPoolExecutor(max_workers=4) as executor:
            list(executor.map(materialize, answer["objects"]))
        print(f"LFS: {min(offset + 50, len(objects))}/{len(objects)}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--qt-root", type=Path, help="Use an existing Qt MSVC x64 installation")
    parser.add_argument("--cuda-root", type=Path, help="Use an existing CUDA 12.9 Toolkit")
    args = parser.parse_args()
    # Offline mode must also check the tools, not merely the dependency folders.
    from importlib.metadata import version
    for package, expected in LOCK["tools"].items():
        if version(package) != expected:
            raise RuntimeError(f"Expected {package}=={expected}; run bootstrap.ps1 online first.")
    os.environ["GIT_LFS_SKIP_SMUDGE"] = "1"
    for name, spec in LOCK["git"].items():
        print(f"Preparing {name} @ {spec['revision']}", flush=True)
        repo = prepare_git(spec, args.offline)
        if name == "windows-libs":
            materialize_lfs(repo, spec, args.offline)
    for name, spec in LOCK["archives"].items():
        if name.startswith("cuda_") and args.cuda_root:
            continue
        prepare_archive(name, spec, args.offline)
    qt = LOCK["qt"]
    qt_root = args.qt_root or DEPS / "qt" / qt["version"] / qt["directory"]
    qt_files = ("lib/cmake/Qt6/Qt6Config.cmake", "bin/windeployqt.exe", "bin/lconvert.exe", "bin/Qt6Widgets.dll", "bin/Qt6Widgetsd.dll",
                "bin/Qt6Test.dll", "bin/Qt6Svg.dll", "plugins/platforms/qwindows.dll", "translations/qtbase_zh_CN.qm")
    if not all((qt_root / file).is_file() for file in qt_files):
        if args.qt_root or args.offline:
            raise RuntimeError(f"Qt installation missing: {qt_root}")
        run(sys.executable, "-m", "aqt", "install-qt", "windows", "desktop", qt["version"], qt["arch"],
            "--outputdir", DEPS / "qt", "--archives", *qt["archives"])
    for file in qt_files:
        if not (qt_root / file).is_file():
            raise RuntimeError(f"Qt installation is incomplete: {qt_root / file}")
    cuda_root = args.cuda_root or DEPS / "cuda"
    for path in (cuda_root / "bin/nvcc.exe", qt_root / "bin/windeployqt.exe"):
        if not path.is_file():
            raise RuntimeError(f"Required tool is missing: {path}")
    # A generated initial cache is optional: default paths also work with CMakePresets.json.
    values = {"DFV_QT_ROOT": qt_root.resolve(), "CUDA_TOOLKIT_ROOT_DIR": cuda_root.resolve(), "Python3_EXECUTABLE": Path(sys.executable)}
    local_cache = "# Generated by bootstrap.py; local paths, do not commit.\n" + "".join(
        f'set({key} "{value.as_posix()}" CACHE PATH "Local dependency")\n' for key, value in values.items())
    cache_file = DEPS / "local.cmake"
    if not cache_file.exists() or cache_file.read_text(encoding="utf-8") != local_cache:
        cache_file.write_text(local_cache, encoding="utf-8")
    print("Dependencies ready. Run tools/build.ps1.", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"Dependency setup failed: {error}")
