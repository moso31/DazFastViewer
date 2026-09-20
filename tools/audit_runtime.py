"""记录 PE 导入依赖和已复制文件摘要；不将其视为完整静态依赖许可审计。"""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
dumpbin=Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe")
out=ROOT/"out"
files={p.name.lower():p for p in out.iterdir() if p.suffix.lower() in (".dll",".exe")}
queue=["cyclesviewportbench.exe"]
visited={}
while queue:
    name=queue.pop()
    if name in visited:
        continue
    file=files[name]
    result=subprocess.check_output([str(dumpbin),"/dependents",str(file)],text=True,errors="replace")
    dependencies=re.findall(r"^\s+([A-Za-z0-9_.-]+\.dll)\s*$",result,re.MULTILINE|re.IGNORECASE)
    visited[name]={"sha256":hashlib.sha256(file.read_bytes()).hexdigest(),"bytes":file.stat().st_size,"imports":dependencies}
    queue.extend(d.lower() for d in dependencies if d.lower() in files)
report={"scope":"PE imports reachable from CyclesViewportBench.exe; excludes static and dynamically loaded dependencies", "files":visited,
        "unneeded_staged_dlls":[p.name for k,p in files.items() if k not in visited and p.suffix==".dll"]}
target=ROOT/"specs/001-cycles-static-camera/evidence/runtime-imports.json"
target.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
print("实际 PE 导入闭包:",", ".join(visited))
print("本地暂存但不在导入闭包中:",", ".join(report["unneeded_staged_dlls"]))
