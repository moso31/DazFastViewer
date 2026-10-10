"""Stage a relocatable runtime from the actual CMake cache, for any configuration."""
from pathlib import Path
import argparse
import hashlib
import json
import platform
import shutil
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def read_cache(path):
    values = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        if line and not line.startswith(('#', '//')) and ':' in line and '=' in line:
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    return values


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def runtime_kernels(build, cache):
    folder = Path(build) / 'cycles/src/kernel/device'
    paths = [folder / 'optix' / f'{name}.ptx.zst' for name in
             ('kernel_optix', 'kernel_optix_mnee', 'kernel_optix_shader_raytrace')]
    for arch in cache['CYCLES_CUDA_BINARIES_ARCH'].split(';'):
        extension = 'ptx' if arch.startswith('compute_') else 'cubin'
        paths.append(folder / 'cuda' / f'kernel_{arch}.{extension}.zst')
    for path in paths:
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f'Missing GPU kernel: {path}. Build all configured CUDA/OptiX kernels before staging.')
    return paths


def stage_file(source, destination):
    if not source.is_file():
        raise RuntimeError(f'Missing runtime file: {source}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_file() and sha256(source) == sha256(destination):
        return
    try:
        shutil.copy2(source, destination)
    except PermissionError:
        if destination.suffix.lower() != '.exe':
            raise
        # Preserve the original developer workflow: running EXEs can be renamed
        # on Windows. Keep the old image without terminating the user's process.
        backup = destination.with_name(destination.stem + '.previous-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.exe')
        destination.rename(backup)
        try:
            shutil.copy2(source, destination)
        except Exception:
            if destination.exists():
                destination.unlink()
            backup.rename(destination)
            raise


def runtime_manifest(out, deployed):
    # The IDE output directory also contains targets still being linked in parallel.
    # Hash only files deployed by this invocation, never arbitrary build products.
    return {p.relative_to(out).as_posix(): sha256(p) for p in sorted(deployed)}


def runtime_output(build, configuration, cache, output=None):
    """从配置确定部署目录；preset 的多配置构建按实际配置分开部署。"""
    if output is not None:
        destination = Path(output)
    else:
        configured = cache.get('CMAKE_INSTALL_PREFIX')
        if configured:
            destination = Path(configured)
            if (destination.name in ('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')
                    and destination.parent.name == Path(build).name):
                destination = destination.with_name(configuration)
        else:
            generator = cache.get('CMAKE_GENERATOR', '')
            year = generator.rsplit(' ', 1)[-1] if generator.startswith('Visual Studio ') else '2022'
            destination = ROOT / 'out' / ('vs' + year) / configuration
    destination = destination.resolve()
    if destination == (ROOT / 'out').resolve():
        raise RuntimeError('Choose a configuration directory such as out/vs2022/Release.')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/vs2022')
    parser.add_argument('--configuration', choices=['Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'], default='Release')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--replace-build', action='store_true', help='Replace a runtime package from another build directory, keeping the same configuration')
    parser.add_argument('--editor-only', action='store_true', help='Deploy the IDE startup target without requiring the benchmark to be built')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    configuration = args.configuration
    cache = read_cache(build / 'CMakeCache.txt')
    out = runtime_output(build, configuration, cache, args.output)
    kernels = runtime_kernels(build, cache)
    out.mkdir(parents=True, exist_ok=True)
    marker = out / '.dfv-runtime.json'
    identity = {'build_dir': str(build), 'configuration': configuration}
    if marker.is_file():
        owner = json.loads(marker.read_text())
        if owner != identity and not (args.replace_build and owner.get('configuration') == configuration):
            raise RuntimeError(f'{out} belongs to a different build/configuration; choose a separate --output or use --replace-build for an intentional migration within the same configuration.')
    qt = Path(cache['DFV_QT_ROOT'])
    libraries = Path(cache['DFV_LIB_DIR'])
    executables = ['DazFastViewer.exe'] if args.editor_only else ['CyclesViewportBench.exe', 'DazFastViewer.exe']
    deployed = set()

    def stage(source, destination):
        stage_file(source, destination)
        deployed.add(destination)

    for name in executables:
        stage(build / 'bin' / configuration / name, out / name)
    qt_result = subprocess.run([str(qt / 'bin/windeployqt.exe'), '--debug' if configuration == 'Debug' else '--release',
                    '--no-compiler-runtime', '--no-opengl-sw', '--translations', 'zh_CN',
                    '--list', 'target', '--dir', str(out), str(out / 'DazFastViewer.exe')],
                    check=True, stdout=subprocess.PIPE, text=True, encoding='utf-8')
    # Qt lists source translation catalogs too; only the merged .qm is created.
    for name in qt_result.stdout.splitlines():
        if name.strip():
            path = Path(name.strip())
            if path.is_file():
                deployed.add(path)
    lock = json.loads((ROOT / 'tools/dependencies.lock.json').read_text(encoding='utf-8'))
    for package in lock['git']['windows-libs']['sparse']:
        for source in (libraries / package).rglob('*.dll'):
            with source.open('rb') as stream:
                if stream.read(2) != b'MZ':
                    raise RuntimeError(f'Unmaterialized Git LFS DLL: {source}; rerun bootstrap.ps1')
            stage(source, out / source.name)
    for source in kernels:
        stage(source, out / 'lib' / source.name)
    for source in (ROOT / 'third_party/moon').iterdir():
        stage(source, out / 'data/moon' / source.name)
    # Ship the Release CRT app-locally; debug CRT is development-only and not redistributable.
    if configuration != 'Debug':
        vs = Path(cache['CMAKE_GENERATOR_INSTANCE'].split(',')[0])
        selected_redist = Path(cache.get('MSVC_REDIST_DIR', '')) / 'x64/Microsoft.VC143.CRT'
        redists = [selected_redist] if selected_redist.is_dir() else sorted((vs / 'VC/Redist/MSVC').glob('*/x64/Microsoft.VC143.CRT'))
        if not redists:
            raise RuntimeError('MSVC v143 redistributable files missing; install the VS C++ workload.')
        for source in redists[-1].glob('*.dll'):
            stage(source, out / source.name)
    for name in ('qt', 'nlohmann', 'jolt', 'opensubdiv', 'hyg'):
        shutil.copytree(ROOT / 'third_party' / name, out / 'licenses' / name, dirs_exist_ok=True)
        deployed.update(p for p in (out / 'licenses' / name).rglob('*') if p.is_file())
    if (qt / 'sbom').exists():
        for source in (qt / 'sbom').glob('*.spdx*'):
            stage(source, out / 'licenses/qt' / source.name)
    source_manifest = ROOT / '.deps/cycles/dfv-source-manifest.json'
    stage(source_manifest, out / source_manifest.name)
    source_files = [ROOT / 'CMakeLists.txt', ROOT / 'CMakePresets.json']
    for folder in ('src', 'cmake', 'tools', 'tests', 'third_party'):
        source_files.extend(p for p in (ROOT / folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    manifest = {
        'staged_at': datetime.now(timezone.utc).isoformat(), **identity,
        'machine': {'platform': platform.platform(), 'processor': platform.processor()},
        'cmake': {key: cache.get(key) for key in ('CMAKE_GENERATOR', 'CMAKE_GENERATOR_INSTANCE', 'CMAKE_GENERATOR_TOOLSET',
                  'CMAKE_CXX_COMPILER', 'CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION', 'DFV_LIB_DIR', 'DFV_QT_ROOT',
                  'CUDA_TOOLKIT_ROOT_DIR', 'CUDA_VERSION', 'OPTIX_ROOT_DIR', 'CYCLES_CUDA_BINARIES_ARCH')},
        'dependency_lock': lock,
        'source_hashes': {p.relative_to(ROOT).as_posix(): sha256(p) for p in source_files},
        'cycles_assembly': json.loads(source_manifest.read_text(encoding='utf-8')),
        'files': runtime_manifest(out, deployed)
    }
    (out / 'build-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    marker.write_text(json.dumps(identity), encoding='utf-8')
    print(f'Runtime staged: {out}')


if __name__ == '__main__':
    main()
