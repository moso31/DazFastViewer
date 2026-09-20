"""单次副屏预览中的相机更新验证；不运行性能回放或修改系统鼠标位置。"""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes
import csv
import json
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from runtime_checks import USER, hwnd_for


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    command = [str(ROOT / "out/CyclesViewportBench.exe"), "--file", args.file,
               "--monitor", "2", "--width", "1600", "--height", "900",
               "--samples", "64", "--preview-seconds", "6", "--output", str(directory)]
    with (directory / "preview.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic() + 20
            hwnd = None
            while not hwnd and process.poll() is None and time.monotonic() < deadline:
                hwnd = hwnd_for(process.pid)
                time.sleep(.05)
            assert hwnd, "未找到副屏预览窗口"
            rect = wintypes.RECT()
            USER.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
            assert USER.GetWindowRect(hwnd, ctypes.byref(rect))
            # 这里只匹配用户此次确认的右侧副屏布局；程序本身按枚举结果选屏。
            assert 2560 <= rect.left < rect.right <= 4480 and 0 <= rect.top < rect.bottom <= 1080
            time.sleep(2)
            for message, wparam, lparam in (
                (0x0204, 2, (400 << 16) | 700),
                (0x0200, 2, (404 << 16) | 712),
                (0x0205, 0, (404 << 16) | 712),
                (0x0204, 6, (400 << 16) | 700),
                (0x0200, 6, (403 << 16) | 704),
                (0x0205, 0, (403 << 16) | 704),
                (0x020A, 120 << 16, 0),
            ):
                assert USER.PostMessageW(hwnd, message, wparam, lparam)
                time.sleep(.12)
            assert process.wait(timeout=30) == 0, "预览进程失败"
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    state = json.loads((directory / "run.json").read_text(encoding="utf-8"))
    assert state["completed"] and not state["benchmark"]
    assert state["adapter_mesh_creations"] == 2, "相机更新重建了网格（预期资产加地板）"
    assert state["adapter_camera_updates"] >= 2
    assert state["final_input_epoch"] == state["final_submitted_epoch"]
    assert state["interop_readback_bytes"] == 0
    with (directory / "events.csv").open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    dirty_epochs = sorted({int(r["camera_epoch"]) for r in rows if r["event"] == "scene_data_dirty"})
    assert dirty_epochs == [2], f"相机更新使静态场景变脏: {dirty_epochs}"
    first_frame = next(float(r["seconds"]) for r in rows if r["event"] == "produced")
    assert all(float(r["seconds"]) > first_frame for r in rows
               if r["event"] == "camera_input" and int(r["camera_epoch"]) > 2)
    result = {"status": "PASS", "window_rect": [rect.left, rect.top, rect.right, rect.bottom],
              "camera_updates": state["adapter_camera_updates"], "mesh_creations": 2,
              "initial_scene_dirty_epochs": dirty_epochs, "final_epoch": state["final_submitted_epoch"],
              "visible_fps_verified": False}
    (directory / "camera-check.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
