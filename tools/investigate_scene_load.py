"""只读盘点 DAZ 场景，并顺序测量加载；结果写入独立诊断目录。"""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def save(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def inventory(root: Path, output: Path) -> None:
    rows = []
    for path in sorted(root.rglob("*.duf")):
        row = {"path": path.as_posix()}
        try:
            packed = path.read_bytes()
            raw = gzip.decompress(packed) if packed[:2] == b"\x1f\x8b" else packed
            document = json.loads(raw)
            scene = document.get("scene", {})
            nodes = scene.get("nodes", [])
            row.update(
                bytes=len(packed), json_bytes=len(raw),
                type=document.get("asset_info", {}).get("type"),
                nodes=len(nodes),
                geometry_nodes=sum(bool(n.get("geometries")) for n in nodes),
                geometries=sum(len(n.get("geometries", [])) for n in nodes),
                unique_node_assets=len({n["url"].split("#")[0] for n in nodes if n.get("url")}),
                modifiers=len(scene.get("modifiers", [])),
                materials=len(scene.get("materials", [])),
                sidecar=path.with_suffix(".dufex").exists(),
            )
        except Exception as error:
            row["error"] = str(error)
        rows.append(row)
        print(json.dumps(row, ensure_ascii=False), flush=True)
    save(output, {"root": root.as_posix(), "scenes": rows,
                  "note": "结构规模用于筛选样本，不等于最终网格/贴图规模或加载耗时排名；不跟随外部资产。"})


def measure(args: argparse.Namespace) -> bool:
    import psutil  # 仅性能采样需要；inventory 使用 Python 标准库即可。

    root = Path(__file__).resolve().parents[1]
    executable = root / ("build/Release/SceneLoadProfile.exe" if args.kind == "cpu" else "out/DazFastViewer.exe")
    executable_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    environment = os.environ.copy()
    if args.cache:
        environment["DFV_ASSET_CACHE"] = str(args.cache.resolve())
    succeeded = True
    for scene in args.scenes:
        for repeat in range(1, args.repeats + 1):
            name = f"{args.tag}-{scene.parent.name}-{scene.stem}-{args.kind}-{repeat}"
            output = args.output.resolve() / name
            output.mkdir(parents=True, exist_ok=False)
            if args.kind == "cpu":
                command = [str(executable), str(scene.resolve()), str(args.project.resolve()), str(output), "--prepare-payloads"]
            else:
                command = [str(executable), "--file", str(scene.resolve()), "--project", str(args.project.resolve()),
                           "--output", str(output), "--capture-test", "--capture-samples", "1"]
            result = {"command": command, "executable_sha256": executable_hash,
                      "cache_override": environment.get("DFV_ASSET_CACHE"),
                      "windows_file_cache_cleared": False,
                      "started_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "timeout_seconds": args.timeout}
            save(output / "run.json", result)
            print(f"START {name}", flush=True)
            started = time.perf_counter()
            with (output / "process.log").open("wb") as log, (output / "resources.csv").open("w", newline="", encoding="utf-8") as resources:
                writer = csv.writer(resources)
                writer.writerow(["seconds", "cpu_seconds", "working_set_bytes", "private_bytes", "read_bytes", "write_bytes"])
                process = subprocess.Popen(command, cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                           creationflags=subprocess.CREATE_NO_WINDOW)
                try:
                    tracked = psutil.Process(process.pid)
                except psutil.NoSuchProcess:
                    tracked = None  # 参数/资产错误可能在创建采样器前已经退出。
                result["pid"] = process.pid
                while process.poll() is None:
                    if tracked is None:
                        break
                    elapsed = time.perf_counter() - started
                    if elapsed > args.timeout:
                        result["timed_out"] = True
                        process.terminate()  # 只终止本次启动的诊断进程。
                        break
                    try:
                        cpu, memory, io = tracked.cpu_times(), tracked.memory_info(), tracked.io_counters()
                        writer.writerow([elapsed, cpu.user + cpu.system, memory.rss, memory.private, io.read_bytes, io.write_bytes])
                        resources.flush()
                    except psutil.NoSuchProcess:
                        break
                    time.sleep(1)
                result["exit_code"] = process.wait(timeout=30)
            result["process_seconds"] = time.perf_counter() - started
            # 首帧取 events.csv，CPU 阶段取 profile.json；进程时长包含退出清理。
            result["status"] = "TIMEOUT" if result.get("timed_out") else "PASS" if result["exit_code"] == 0 else "FAIL"
            succeeded &= result["status"] == "PASS"
            save(output / "run.json", result)
            print(json.dumps(result, ensure_ascii=False), flush=True)
    return succeeded


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    scan = commands.add_parser("inventory", help="统计指定目录下全部 DUF 的场景结构")
    scan.add_argument("root", type=Path)
    scan.add_argument("output", type=Path)
    run = commands.add_parser("measure", help="顺序运行场景，避免多个诊断争抢资源")
    run.add_argument("scenes", nargs="+", type=Path)
    run.add_argument("--kind", choices=["cpu", "ui"], default="cpu")
    run.add_argument("--project", type=Path, default=Path("DazFastViewer.project.json"))
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--cache", type=Path, help="独立应用缓存；不清除 Windows 文件缓存")
    run.add_argument("--tag", default="current")
    run.add_argument("--repeats", type=int, default=1)
    run.add_argument("--timeout", type=float, default=900)
    args = parser.parse_args()
    if args.operation == "inventory":
        inventory(args.root, args.output)
    else:
        if args.repeats < 1 or args.timeout <= 0:
            parser.error("repeats 和 timeout 必须为正数")
        if not measure(args):
            raise SystemExit(1)


if __name__ == "__main__":
    main()
