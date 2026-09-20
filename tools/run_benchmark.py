"""串行运行真实 GPU 基准，保留命令、日志、显存采样和分析结果。"""
from pathlib import Path
import argparse
import json
import subprocess
import time
from analyze_run import analyze

ROOT = Path(__file__).resolve().parents[1]


def run(backend, index, seconds, warmup, refine):
    directory = ROOT/"artifacts"/f"baseline-{backend.lower()}-{index:02}"
    if directory.exists():
        raise RuntimeError(f"保留已有证据，不覆盖: {directory}")
    directory.mkdir(parents=True)
    command = [str(ROOT/"out/CyclesViewportBench.exe"), "--benchmark", "--device", backend,
               "--seconds", str(seconds), "--warmup", str(warmup), "--refine", str(refine), "--output", str(directory)]
    (directory/"command.json").write_text(json.dumps(command, indent=2), encoding="utf-8")
    with (directory/"console.log").open("w", encoding="utf-8") as log, (directory/"gpu.csv").open("w") as gpu:
        gpu.write("host_seconds,name,driver,memory_used_mib,utilization_percent\n")
        process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        start = time.monotonic()
        try:
            while process.poll() is None:
                sample = subprocess.run(["nvidia-smi", "--query-gpu=name,driver_version,memory.used,utilization.gpu", "--format=csv,noheader,nounits"],
                                        capture_output=True, text=True, timeout=10, creationflags=subprocess.CREATE_NO_WINDOW)
                for line in sample.stdout.splitlines():
                    gpu.write(f"{time.monotonic()-start:.6f},{line}\n")
                gpu.flush()
                if time.monotonic()-start > seconds+warmup+refine+240:
                    raise TimeoutError("基准运行超时，保留未完成日志")
                time.sleep(1)
        except BaseException:
            process.terminate()
            process.wait(timeout=20)
            raise
    (directory/"exit.json").write_text(json.dumps({"exit_code": process.returncode}))
    if process.returncode:
        raise RuntimeError(f"渲染程序失败: {directory}, exit={process.returncode}")
    analyze(directory)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backends", nargs="+", choices=["CUDA", "OPTIX"], default=["OPTIX"])
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--start-index", type=int)
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--warmup", type=float, default=10)
    parser.add_argument("--refine", type=float, default=10)
    args = parser.parse_args()
    for backend in args.backends:
        existing=[int(p.name.rsplit("-",1)[1]) for p in (ROOT/"artifacts").glob(f"baseline-{backend.lower()}-*") if p.name.rsplit("-",1)[1].isdigit()]
        first=args.start_index if args.start_index is not None else max(existing,default=0)+1
        for index in range(first, first+args.runs):
            run(backend, index, args.seconds, args.warmup, args.refine)
