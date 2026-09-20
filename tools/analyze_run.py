"""分析实际事件；缺少外部显示观察时，不把提交帧率视为 Visible FPS。"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math


def stats(values):
    values = sorted(values)
    def percentile(p):
        if not values:
            return None
        at = (len(values)-1)*p
        low = int(at)
        return values[low] + (values[min(low+1, len(values)-1)]-values[low])*(at-low)
    return {"count": len(values), "p50": percentile(.5), "p95": percentile(.95),
            "p99": percentile(.99), "max": max(values, default=None)}


def analyze(directory):
    root = Path(__file__).resolve().parents[1]
    report = json.loads((directory/"run.json").read_text())
    manifest = json.loads((directory/"manifest.json").read_text())
    rows = list(csv.DictReader((directory/"events.csv").open()))
    start, end = report["measurement_start"], report["measurement_end"]
    inputs = {int(r["camera_epoch"]): float(r["seconds"]) for r in rows if r["event"] == "camera_input"}
    applied = {int(r["camera_epoch"]): float(r["seconds"]) for r in rows if r["event"] == "camera_apply"}
    measured, refinements, seen_frames, seen_epochs = [], [], set(), set()
    epoch_errors = []
    missing_input_epochs = set()
    prior_epoch = 0
    for row in rows:
        if row["event"] != "present_submitted":
            continue
        frame, epoch, time = int(row["frame_id"]), int(row["camera_epoch"]), float(row["seconds"])
        if frame in seen_frames:
            epoch_errors.append("duplicate frame_id")
        if epoch < prior_epoch:
            epoch_errors.append("epoch decreased")
        prior_epoch = epoch
        if epoch > 2 and epoch not in applied:
            epoch_errors.append(f"unknown camera epoch {epoch}")
        if epoch > 2 and epoch not in inputs:
            missing_input_epochs.add(epoch)
        if start <= time < end:
            (refinements if epoch in seen_epochs else measured).append(row)
        seen_frames.add(frame)
        seen_epochs.add(epoch)
    times = [float(row["seconds"]) for row in measured]
    fps = [sum(start+i <= t < min(start+i+1, end) for t in times) for i in range(math.ceil(end-start))] if start >= 0 else []
    durations = {}
    for name in ("scene_sync", "render_work_cpu", "camera_apply", "input_to_apply"):
        durations[name+"_ms"] = stats([float(r["value_ms"]) for r in rows if r["event"] == name and start <= float(r["seconds"]) < end])
    input_present = [(float(r["seconds"])-inputs[int(r["camera_epoch"])] )*1000 for r in measured if int(r["camera_epoch"]) in inputs]
    report.update({
        "manifest": manifest,
        "navigation_submission_fps": len(measured)/(end-start) if end > start else None,
        "submission_fps_per_second": fps,
        "submission_interval_ms": stats([(b-a)*1000 for a,b in zip(times,times[1:])]),
        "input_to_submission_ms": stats(input_present),
        "refinement_submission_frames_during_navigation": len(refinements),
        "scene_data_dirty_during_navigation": sum(r["event"]=="scene_data_dirty" and start <= float(r["seconds"]) < end for r in rows),
        "epoch_validation_errors": sorted(set(epoch_errors)),
        "missing_input_event_epochs": sorted(missing_input_epochs),
        "actual_input_hz": sum(start <= t < end for t in inputs.values())/(end-start) if end > start else None,
        "refinement_samples_after_stop": [int(r["samples"]) for r in rows if r["event"]=="present_submitted" and float(r["seconds"])>=end],
        "durations": durations,
        "acceptance_passed": False,
        "limitations": ["缺少已关联的外部显示事件，Visible FPS 未验证", "render_work_cpu 为含同步的 CPU 墙钟时间，不是 GPU kernel 计时", "scene_data_dirty 是场景更新条件观测，细分上传/重建计数仍待补充"]
    })
    if (directory/"build-manifest.json").exists():
        report["build"] = json.loads((directory/"build-manifest.json").read_text(encoding="utf-8"))
    else:
        report["limitations"].append("早期诊断运行未记录构建时源码哈希")
    if missing_input_epochs:
        report["limitations"].append("部分实际应用相机缺少输入时间事件，不能计算这些状态的输入延迟")
    (directory/"summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"run": str(directory), **{k: report[k] for k in ("completed", "display_invalid", "navigation_submission_fps", "actual_input_hz", "scene_data_dirty_during_navigation", "epoch_validation_errors", "interop_readback_bytes")}}, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="+")
    for directory in parser.parse_args().directory:
        analyze(directory)
