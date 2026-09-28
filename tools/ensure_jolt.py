"""下载本机验证过的 Jolt CPU 引擎版本，不修改系统环境。"""
import subprocess
import sys
from pathlib import Path

destination = Path(sys.argv[1])
revision = "e77f175595e64cb44218cc9d9d56fc365ad0e36a"
if not (destination / "Build/CMakeLists.txt").is_file():
    subprocess.run(["git", "clone", "--depth", "1", "--branch", "v5.6.0", "https://github.com/jrouwe/JoltPhysics.git", str(destination)], check=True)
actual = subprocess.check_output(["git", "-C", str(destination), "rev-parse", "HEAD"], text=True).strip()
if actual != revision:
    raise SystemExit(f"Jolt 版本不匹配：{actual}，预期 {revision}")
