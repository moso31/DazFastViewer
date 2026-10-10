"""汇总真实水体导航实验，区分移动期间与停下后的几何/渲染成本。"""
import csv
import json
import statistics
import sys
from pathlib import Path


def summary(values):
    values = sorted(values)
    return dict(count=len(values), total_ms=sum(values),
                median_ms=statistics.median(values) if values else 0,
                p95_ms=values[min(len(values)-1, int(len(values)*.95))] if values else 0,
                max_ms=max(values, default=0))


directory = Path(sys.argv[1])
report = json.loads((directory / "report.json").read_text(encoding="utf-8"))
with (directory / "events.csv").open(encoding="utf-8") as stream:
    events = list(csv.DictReader(stream))
results = []
for case in report["cases"]:
    rows = [r for r in events if case["begin"] <= float(r["seconds"]) <= case["end"]]
    stages = {}
    for name in ("water_lod", "water_adapter", "adapter_apply", "overlay_update", "picking_update",
                 "scene_sync", "path_trace", "render_work_cpu", "display_update"):
        stages[name] = summary([float(r["value_ms"]) for r in rows if r["event"] == name])
    result = {k: v for k, v in case.items() if k != "gaps_ms"}
    result["new_frame_intervals"] = summary(case["gaps_ms"])
    result["stages"] = stages
    result["resets"] = sum(r["event"] == "edit_reset" for r in rows)
    result["lod_while_moving"] = sum(r["event"] == "water_adapter" and float(r["seconds"]) <= case["motion_end"] for r in rows)
    results.append(result)
    print(case["name"], "topology=", case["topology_updates"],
          "frame intervals=", result["new_frame_intervals"], "scene sync=", stages["scene_sync"])
(directory / "analysis.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
