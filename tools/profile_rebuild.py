"""运行副屏场景重建诊断，记录进程内存、整卡显存及实际首帧延迟。"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import time

import psutil


def analyze(directory):
    report = json.loads((directory / "editor-check.json").read_text(encoding="utf-8"))
    with (directory / "events.csv").open(encoding="utf-8", newline="") as stream:
        events = list(csv.DictReader(stream))
    result = {"status": report["status"], "checks": [],
              "measurement": "input to first submitted frame of a new render epoch; not convergence or physical scanout"}
    for check in report["checks"]:
        begin = check["begin"]
        frames = [e for e in events if e["event"] == "present_submitted"
                  and float(e["seconds"]) >= begin
                  and check["before_epoch"] < int(e["camera_epoch"]) <= check["final_epoch"]]
        if not frames:
            raise RuntimeError(f"缺少新版本呈现记录：{check['input']}")
        end = float(frames[0]["seconds"])
        stages = {}
        for e in events:
            value = float(e["value_ms"])
            if begin <= float(e["seconds"]) <= end and value > 0:
                stages[e["event"]] = stages.get(e["event"], 0) + value
        blank_start, blank_seconds = None, 0
        for e in events:
            at = float(e["seconds"])
            if at > end:
                break
            if e["event"] == "blank_present_begin":
                blank_start = at
            elif e["event"] == "blank_present_end" and blank_start is not None:
                blank_seconds += max(0, at - max(begin, blank_start))
                blank_start = None
        if blank_start is not None:
            blank_seconds += max(0, end - max(begin, blank_start))
        result["checks"].append(dict(check, first_present=end, first_ms=(end-begin)*1000,
                                     first_samples=int(frames[0]["samples"]), blank_ms=blank_seconds*1000,
                                     stages_ms=stages))
    (directory / "rebuild-analysis.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    return result


def run(args):
    binary, directory = args.binary.resolve(), args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    command = [str(binary), "--self-test", "--file", str(args.scene.resolve()),
               "--rebuild-test", str(args.wearable.resolve()), "--project", str(args.project.resolve()),
               "--output", str(directory)]
    measurements = []
    started = time.monotonic()
    with (directory / "process.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=binary.parent, stdout=log, stderr=subprocess.STDOUT)
        observed = psutil.Process(process.pid)
        while process.poll() is None:
            try:
                memory, cpu = observed.memory_info(), observed.cpu_times()
                sample = dict(seconds=time.monotonic()-started, working_set=memory.rss,
                              private_bytes=memory.private, cpu_seconds=cpu.user+cpu.system,
                              system_available=psutil.virtual_memory().available)
                query = subprocess.run(["nvidia-smi", "--query-gpu=memory.used,memory.total,utilization.gpu", "--format=csv,noheader,nounits"],
                                       capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
                if query.returncode == 0:
                    sample["gpu_used_mib"], sample["gpu_total_mib"], sample["gpu_utilization"] = map(int, query.stdout.splitlines()[0].split(","))
                measurements.append(sample)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                break
            time.sleep(0.5)
        code = process.wait()
    metadata = {"command": command, "exit_code": code, "wall_seconds": time.monotonic()-started,
                "exe_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(), "samples": measurements,
                "gpu_scope": "whole device including other applications; sampled, not allocation peak"}
    (directory / "resources.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    if code:
        raise RuntimeError(f"诊断程序失败，退出码 {code}；见 {directory / 'process.log'}")
    result = analyze(directory)
    for check in result["checks"]:
        print(check["input"], f"first={check['first_ms']:.1f} ms", f"blank={check['blank_ms']:.1f} ms", f"sessions+={check['sessions_added']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--wearable", type=Path, required=True)
    parser.add_argument("--project", type=Path, default=Path("DazFastViewer.project.json"))
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
