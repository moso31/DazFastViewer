"""从固定 Git 对象生成独立 Cycles 构建树，不改动用户 checkout。"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
STANDALONE = "3b97e190c5ff1a2ed2160d879ad5bf95bea7b8ba"
BLENDER = "d13f752e3b9c4f8c261cda552b1021f8bcc0382c"


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def write_changed(path, data):
    if isinstance(data, str):
        data = data.encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blender", default="D:/Github/blender-5.2.2")
    parser.add_argument("--standalone", default=str(ROOT / ".research/cycles-v5.2.0"))
    parser.add_argument("--output", default=str(ROOT / ".deps/cycles"))
    args = parser.parse_args()
    destination = Path(args.output).resolve()
    if not destination.is_relative_to(ROOT / ".deps"):
        raise SystemExit("生成目录必须位于项目 .deps 内")
    for repo, commit in [(args.blender, BLENDER), (args.standalone, STANDALONE)]:
        git(repo, "cat-file", "-e", commit + "^{commit}")
    archive = git(args.standalone, "archive", STANDALONE)
    files = {}
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for item in tar.getmembers():
            if not item.isfile():
                continue
            target = (destination / item.name).resolve()
            if not target.is_relative_to(destination):
                raise SystemExit("不安全的归档路径")
            files[item.name] = tar.extractfile(item).read()

    differences = json.loads((ROOT / "specs/001-cycles-static-camera/evidence/source-differences.json").read_text())
    adopted = []
    for row in differences["files"]:
        if row["category"] not in ("blender_patch_change", "requires_merge_review"):
            continue
        path = row["path"]
        data = git(args.blender, "show", BLENDER + ":intern/cycles/" + path)
        if row["blender_v5_2_2_spdx"] not in ("Apache-2.0", "BSD-3-Clause"):
            raise SystemExit("未审核的来源许可: " + path)
        files["src/" + path] = data
        adopted.append({"path": path, "sha256": hashlib.sha256(data).hexdigest(), "license": row["blender_v5_2_2_spdx"]})

    def replace(path, before, after):
        original = files[path].decode("utf-8").replace("\r\n", "\n")
        if original.count(before) != 1:
            raise RuntimeError("补丁定位不唯一: " + path)
        files[path] = original.replace(before, after).encode("utf-8")

    replace("src/cmake/external_libs.cmake",
            'set(_cycles_lib_dir "${CMAKE_CURRENT_SOURCE_DIR}/lib/${_cycles_lib_platform}")',
            'set(_cycles_lib_dir "${DFV_LIB_DIR}")')
    # 项目自有目标定义替代完整 Blender 依赖集合；关闭未使用的功能。
    replace("CMakeLists.txt", "include(dependency_targets)", 'include("${DFV_ROOT}/cmake/dependency_targets.cmake")')
    replace("src/app/CMakeLists.txt", "# Application build targets", '# Application build targets\ninclude("${DFV_ROOT}/cmake/bench.cmake")')
    replace("src/util/CMakeLists.txt", "PRIVATE bf::dependencies::openexr", "PRIVATE bf::dependencies::openexr\n  PRIVATE fmt::fmt")
    # 显式启用与已安装依赖一致的 C++20，避免独立上游残留 C++17 旗标。
    path = "src/cmake/configure_build.cmake"
    files[path] = files[path].replace(b"/std:c++17", b"/std:c++20")
    # 在持有 Scene 锁时标记实际渲染工作的相机版本，防止旧结果冒充新输入。
    replace("src/session/session.h", "#include <functional>", "#include <functional>\n#include <atomic>")
    replace("src/session/session.h", "  Stats stats;", """  Stats stats;
  std::atomic<uint64_t> dfv_requested_epoch{0}, dfv_render_epoch{0};
  std::atomic<int> dfv_render_samples{0};
  std::function<void(const char *, uint64_t, double)> dfv_event;
""")
    replace("src/session/session.cpp", "  /* Update scene */\n  const bool reset_scene = update_scene(delayed_reset_.do_reset);", """  const double dfv_sync_start = time_dt();
  const bool reset_scene = update_scene(delayed_reset_.do_reset);
  if (dfv_event) dfv_event("scene_sync", dfv_requested_epoch.load(), (time_dt()-dfv_sync_start)*1000.0);
""")
    replace("src/session/session.cpp", "  const bool reset_scene = update_scene(delayed_reset_.do_reset);", """  if (dfv_event && scene->need_reset(false)) dfv_event("scene_data_dirty", dfv_requested_epoch.load(), 0.0);
  const bool reset_scene = update_scene(delayed_reset_.do_reset);""")
    replace("src/session/session.cpp", "      path_trace_->render(render_work);", """      const double dfv_render_start = time_dt();
      path_trace_->dfv_event = [this](const char *name, double ms) {
        if (dfv_event) dfv_event(name, dfv_render_epoch.load(), ms);
      };
      path_trace_->render(render_work);
      if (dfv_event) dfv_event("render_work_cpu", dfv_render_epoch.load(), (time_dt()-dfv_render_start)*1000.0);""")
    replace("src/session/session.cpp", "    scene->update_camera_resolution(progress, width, height);", """    scene->update_camera_resolution(progress, width, height);
    dfv_render_epoch.store(dfv_requested_epoch.load());
    dfv_render_samples.store(render_work.path_trace.start_sample + render_work.path_trace.num_samples);
""")
    # 独立呈现线程与渲染线程共享的两个状态必须是原子变量。
    for path in ("src/integrator/path_trace.h", "src/integrator/path_trace_display.h"):
        replace(path, "#pragma once", "#pragma once\n#include <atomic>")
    replace("src/integrator/path_trace.h", "  bool did_draw_after_reset_ = true;", "  std::atomic<bool> did_draw_after_reset_{true};")
    replace("src/integrator/path_trace_display.h", "    bool is_outdated = true;", "    std::atomic<bool> is_outdated{true};")
    replace("src/integrator/path_trace.cpp", "  did_draw_after_reset_ |= display_->draw();", "  if (display_->draw()) did_draw_after_reset_.store(true);")
    # 区分实际追踪、工作缓冲和显示上传，避免把整个 render_work 当成 GPU 光照时间。
    replace("src/integrator/path_trace.h", "  void render(const RenderWork &render_work);",
            "  void render(const RenderWork &render_work);\n  std::function<void(const char *, double)> dfv_event;")
    for call, name in (("render_init_kernel_execution()", "render_init"),
                       ("init_render_buffers(render_work)", "render_buffers"),
                       ("path_trace(render_work)", "path_trace"),
                       ("update_display(render_work)", "display_update")):
        replace("src/integrator/path_trace.cpp", f"  {call};",
                f'  {{ const double start = time_dt(); {call}; if (dfv_event) dfv_event("{name}", (time_dt()-start)*1000.0); }}')
    for name, data in files.items():
        write_changed(destination / name, data)
    manifest = {"standalone_commit": STANDALONE, "blender_commit": BLENDER, "adopted_files": adopted,
                "build_adaptations": ["explicit library root", "project dependency targets", "benchmark target", "fmt linkage", "C++20", "actual render epoch and scene sync telemetry", "render stage telemetry", "atomic cross-thread display state"]}
    write_changed(destination / "dfv-source-manifest.json", json.dumps(manifest, indent=2) + "\n")
    print(f"Cycles 构建树已生成：{destination}；接入 {len(adopted)} 个 Blender 5.2.2 文件")


if __name__ == "__main__":
    main()
