"""按实际 SwapBuffers 后的呈现事件分析短操作，区别于采样收敛时间。"""
import argparse
import csv
import json
from pathlib import Path


def analyze(directory):
    report = json.loads((directory / "editor-check.json").read_text(encoding="utf-8"))
    with (directory / "events.csv").open(encoding="utf-8", newline="") as stream:
        events = list(csv.DictReader(stream))
    checks = []
    for check in report["checks"]:
        begin, stop = check["begin"], check["stop"]
        frames = [e for e in events if e["event"] == "present_submitted"
                  and float(e["seconds"]) >= begin
                  and check["before_epoch"] < int(e["camera_epoch"]) <= check["final_epoch"]]
        final = [e for e in frames if int(e["camera_epoch"]) == check["final_epoch"]
                 and [int(e["width"]), int(e["height"])] == check["size"]]
        if not frames or not final:
            raise RuntimeError(f"缺少实际呈现证据：{check['input']}")
        end = float(final[0]["seconds"])
        stages = {}
        for event in events:
            if begin <= float(event["seconds"]) <= end and float(event["value_ms"]) > 0:
                stage = stages.setdefault(event["event"], {"calls": 0, "sum_ms": 0, "max_ms": 0})
                value = float(event["value_ms"])
                stage["calls"] += 1
                stage["sum_ms"] += value
                stage["max_ms"] = max(stage["max_ms"], value)
        inputs = [e for e in events if e["event"] == "edit_input"
                  and begin <= float(e["seconds"]) <= stop]
        resets = {int(e["camera_epoch"]): e for e in events if e["event"] == "edit_reset"}
        latest = []
        if inputs:
            revision = int(inputs[-1]["frame_id"])
            latest = [e for e in frames if int(resets.get(int(e["camera_epoch"]), {}).get("frame_id", 0)) >= revision]
        edit_metrics = {}
        if inputs and latest:
            edit_metrics = dict(input_count=len(inputs),
                                presented_revisions=sorted({int(resets[int(e["camera_epoch"])]["frame_id"])
                                                            for e in frames if int(e["camera_epoch"]) in resets}),
                                latest_input_to_frame_ms=(float(latest[0]["seconds"]) - float(inputs[-1]["seconds"])) * 1000,
                                stop_to_latest_frame_ms=max(0, (float(latest[0]["seconds"]) - stop) * 1000))
        checks.append(dict(check, first_ms=(float(frames[0]["seconds"]) - begin) * 1000,
                           restore_ms=(end - stop) * 1000, first_frame_samples=int(frames[0]["samples"]),
                           full_frame_samples=int(final[0]["samples"]), stages=stages, **edit_metrics))
    result = {"source": str(directory), "status": report["status"], "checks": checks,
              "measurement": "input/stop to first actual submitted frame; not noise convergence or physical scanout"}
    (directory / "interaction-analysis.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    for check in checks:
        print(f"{check['input']:24} first={check['first_ms']:9.2f} ms  full={check['restore_ms']:9.2f} ms  sessions+={check['sessions_added']}")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    analyze(parser.parse_args().directory)
