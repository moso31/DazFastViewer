"""Regression checks for dependency integrity, safe extraction and cache paths."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


bootstrap = load('bootstrap')
stage = load('stage_runtime')


class BuildToolsTest(unittest.TestCase):
    def test_staging_uses_configured_runtime_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp) / 'build/vs2022'
            runtime = Path(temp) / 'out/vs2022/Release'
            cache = {'CMAKE_INSTALL_PREFIX': str(runtime)}
            self.assertEqual(stage.runtime_output(build, 'Release', cache), runtime.resolve())
            self.assertEqual(stage.runtime_output(build, 'Debug', cache), runtime.with_name('Debug').resolve())
            custom = Path(temp) / 'custom package'
            self.assertEqual(stage.runtime_output(build, 'Release', cache, custom), custom.resolve())
            self.assertEqual(stage.runtime_output(build, 'Release', {'CMAKE_INSTALL_PREFIX': str(custom)}), custom.resolve())

    def test_staging_without_install_prefix_uses_generator(self):
        build = ROOT / 'build/vs2022'
        for year, generator in (('2022', 'Visual Studio 17 2022'), ('2026', 'Visual Studio 18 2026')):
            cache = {'CMAKE_GENERATOR': generator}
            self.assertEqual(stage.runtime_output(build, 'Release', cache), (ROOT / 'out' / ('vs' + year) / 'Release').resolve())

    def test_staging_requires_configuration_directory(self):
        build = ROOT / 'build/vs2022'
        for explicit in (None, ROOT / 'out'):
            with self.assertRaisesRegex(RuntimeError, 'configuration directory'):
                stage.runtime_output(build, 'Release', {'CMAKE_INSTALL_PREFIX': str(ROOT / 'out')}, explicit)

    def test_manifest_does_not_open_unrelated_linker_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            owned = {out / 'DazFastViewer.exe', out / 'platforms/qwindows.dll', out / 'lib/kernel_sm_86.cubin.zst'}
            for path in owned:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'runtime')
            for name in ('RenderOptionsTest.exe', 'RenderOptionsTest.pdb', 'DazFastViewer.previous.exe'):
                (out / name).write_bytes(b'unrelated')
            digest = stage.sha256

            def hash_owned(path):
                if path not in owned:
                    raise PermissionError('Unrelated target is still being linked')
                return digest(path)

            with patch.object(stage, 'sha256', side_effect=hash_owned):
                manifest = stage.runtime_manifest(out, owned)
            self.assertEqual(set(manifest), {p.relative_to(out).as_posix() for p in owned})

    def test_staging_rejects_partial_multi_gpu_build(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp)
            root = build / 'cycles/src/kernel/device'
            for name in ('kernel_optix', 'kernel_optix_mnee', 'kernel_optix_shader_raytrace'):
                path = root / 'optix' / (name + '.ptx.zst')
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'kernel')
            cuda = root / 'cuda'
            cuda.mkdir()
            (cuda / 'kernel_sm_86.cubin.zst').write_bytes(b'kernel')
            cache = {'CYCLES_CUDA_BINARIES_ARCH': 'sm_86;sm_89'}
            with self.assertRaisesRegex(RuntimeError, 'kernel_sm_89'):
                stage.runtime_kernels(build, cache)
            (cuda / 'kernel_sm_89.cubin.zst').write_bytes(b'kernel')
            self.assertEqual(len(stage.runtime_kernels(build, cache)), 5)

    def test_download_rejects_corrupt_offline_cache(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'archive.zip'
            file.write_bytes(b'incomplete download')
            with self.assertRaisesRegex(RuntimeError, 'corrupt offline'):
                bootstrap.download('https://unused.invalid', file, hashlib.sha256(b'expected').hexdigest(), offline=True)

    def test_verified_download_is_usable_offline(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'archive.zip'
            file.write_bytes(b'complete')
            self.assertEqual(bootstrap.download('https://unused.invalid', file, bootstrap.digest(file), offline=True), file)

    def test_extract_preserves_spaces_and_unicode(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / 'source.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('source-commit/include/folder with spaces/文件.h', b'header')
            destination = Path(temp) / 'destination'
            bootstrap.extract_zip(archive, destination)
            self.assertEqual((destination / 'include/folder with spaces/文件.h').read_bytes(), b'header')

    def test_extract_rejects_parent_traversal(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / 'source.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('source/../../outside.txt', b'bad')
            with self.assertRaisesRegex(RuntimeError, 'Unsafe'):
                bootstrap.extract_zip(archive, Path(temp) / 'destination')
            self.assertFalse((Path(temp) / 'outside.txt').exists())

    def test_cache_keeps_path_separators_spaces_and_equals(self):
        with tempfile.TemporaryDirectory() as temp:
            cache = Path(temp) / 'CMakeCache.txt'
            cache.write_text('// comment\n# comment\nDFV_QT_ROOT:PATH=D:/SDK folder/Qt\nCUSTOM:STRING=a=b;c\n', encoding='utf-8')
            self.assertEqual(stage.read_cache(cache), {'DFV_QT_ROOT': 'D:/SDK folder/Qt', 'CUSTOM': 'a=b;c'})


if __name__ == '__main__':
    unittest.main()
