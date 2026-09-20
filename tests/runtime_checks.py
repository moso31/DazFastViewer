"""实际窗口和错误路径回归：不要求人工操作，不把诊断运行计入性能验收。"""
from pathlib import Path
import ctypes
from ctypes import wintypes
import json
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/"out/CyclesViewportBench.exe"
USER=ctypes.WinDLL("user32",use_last_error=True)
USER.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
USER.ShowWindow.argtypes=[wintypes.HWND,ctypes.c_int]
USER.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
USER.IsWindowVisible.argtypes=[wintypes.HWND]
CALLBACK=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
USER.EnumWindows.argtypes=[CALLBACK,wintypes.LPARAM]


def hwnd_for(pid):
    found=[]
    @CALLBACK
    def visit(hwnd,_):
        owner=wintypes.DWORD()
        USER.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        if owner.value==pid and USER.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True
    USER.EnumWindows(visit,0)
    return found[0] if found else None


def main():
    evidence=ROOT/"artifacts/runtime-checks"
    evidence.mkdir(parents=True,exist_ok=True)
    results={}
    for name,args in {"invalid_device":["--device","UNAVAILABLE"],"invalid_resolution":["--width","0"]}.items():
        result=subprocess.run([str(EXE),*args],cwd=ROOT,capture_output=True,timeout=15)
        (evidence/(name+".log")).write_bytes(result.stdout+result.stderr)
        assert result.returncode==1,(name,result.returncode)
        results[name]="PASS"
    directory=evidence/"slow-render"
    command=[str(EXE),"--benchmark","--scene","smoke","--width","640","--height","360",
             "--warmup","1","--seconds","5","--refine","1","--simulate-render-delay-ms","200","--output",str(directory)]
    with (evidence/"slow-render.log").open("wb") as log:
        result=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=60)
    assert result.returncode==0,"注入延迟运行失败"
    state=json.loads((directory/"run.json").read_text())
    assert state["ui_max_loop_gap_ms"]<100,"UI 出现接近渲染等待的停顿"
    assert state["final_input_epoch"]==state["final_submitted_epoch"],"停动后未显示最终相机"
    results["render_delay_200ms"]={"status":"PASS","ui_max_loop_gap_ms":state["ui_max_loop_gap_ms"]}
    directory=evidence/"window-lifecycle"
    command=[str(EXE),"--benchmark","--scene","smoke","--width","640","--height","360",
             "--warmup","0","--seconds","30","--refine","0","--output",str(directory)]
    with (evidence/"window-lifecycle.log").open("wb") as log:
        process=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+15
            hwnd=None
            while not hwnd and time.monotonic()<deadline:
                hwnd=hwnd_for(process.pid);time.sleep(.05)
            assert hwnd,"没有找到测试窗口"
            time.sleep(1)
            for _ in range(5):
                USER.ShowWindow(hwnd,6);time.sleep(.1);USER.ShowWindow(hwnd,9);time.sleep(.1)
            USER.PostMessageW(hwnd,0x0010,0,0)
            assert process.wait(timeout=15)==2,"主动中止必须以未完成状态退出"
        finally:
            if process.poll() is None:
                process.terminate();process.wait(timeout=10)
    state=json.loads((directory/"run.json").read_text())
    assert not state["completed"] and state["invalid_minimized"],"最小化/提前关闭没有使验收失效"
    results["minimize_restore_close"]="PASS"
    (evidence/"results.json").write_text(json.dumps(results,indent=2),encoding="utf-8")
    print(json.dumps(results))


if __name__=="__main__":
    main()
