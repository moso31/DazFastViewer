"""区分显示更新间隔、采样批次和路径追踪耗时，分析 --render-profile 产物。"""
import argparse
import csv
import json
import statistics
from pathlib import Path


def analyze(directory):
    records = json.loads((directory / "render-profile.json").read_text(encoding="utf-8"))
    with (directory / "events.csv").open(encoding="utf-8") as stream:
        events = list(csv.DictReader(stream))
    result = []
    for case in records:
        # 只取该相机的完整分辨率 epoch；不计加载、场景同步、导航低分辨率。
        rows = [r for r in events if r.get("camera_epoch") == str(case["epoch"])
                and float(r["seconds"]) <= case["end_seconds"]]
        frames = [r for r in rows if r["event"] == "present_submitted"]
        intervals = [(float(b["seconds"]) - float(a["seconds"])) * 1000
                     for a, b in zip(frames, frames[1:])]
        paths = [r for r in rows if r["event"] == "path_trace" and int(r["samples"]) > 0]
        samples, last_sample, path_ms = 0, 0, 0
        for r in paths:
            current = int(r["samples"])
            # 有完整起点的批次才计入每样本成本；取消和空工作不重复计数。
            if current > last_sample:
                samples += current - last_sample
                path_ms += float(r["value_ms"])
                last_sample = current
        stage_ms = {name: sum(float(r["value_ms"]) for r in rows if r["event"] == name)
                    for name in ("path_trace", "display_update", "scene_sync", "render_work_cpu")}
        result.append(dict(case=case["case"], settings=case["settings"], epoch=case["epoch"],
                           sampling=case.get("sampling"),
                           render_size=case["render_size"], measured_samples=samples,
                           path_ms_per_sample=path_ms / samples if samples else None,
                           display_interval_median_ms=statistics.median(intervals) if intervals else None,
                           display_interval_max_ms=max(intervals) if intervals else None,
                           presented_frames=len(frames), path_batches=len(paths),
                           stage_ms=stage_ms))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    data = analyze(args.directory)
    (args.directory / "analysis.json").write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    for row in data:
        print(json.dumps(row, ensure_ascii=False))
