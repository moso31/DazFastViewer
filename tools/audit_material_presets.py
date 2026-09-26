"""逐个检查内容库中的 DUF 头部，并完整解析所有材质 / Shader 预设。"""
import collections
import concurrent.futures
import gzip
import itertools
import json
import os
import re
import sys
import time
from pathlib import Path


def inspect(file):
    try:
        with open(file, 'rb') as raw:
            zipped = raw.read(2) == b'\x1f\x8b'
        with (gzip.open(file, 'rb') if zipped else open(file, 'rb')) as stream:
            head = stream.read(8192)
            match = re.search(rb'"asset_info"\s*:\s*\{.*?"type"\s*:\s*"([^"]+)"', head, re.S)
            kind = match[1].decode() if match else 'unknown'
            if kind != 'unknown' and 'material' not in kind and kind != 'preset_shader':
                return dict(file=file, kind=kind)
            data = head + stream.read(32 * 1024 * 1024 - len(head) + 1)
        if len(data) > 32 * 1024 * 1024:
            return dict(file=file, kind=kind, skipped_large=True)
        data = json.loads(data)
        materials = data.get('material_library', []) + data.get('scene', {}).get('materials', [])
        channels = collections.Counter()
        shader = collections.Counter()
        dependencies = collections.Counter()
        groups = set()
        partial = 0
        for m in materials:
            groups.update(m.get('groups', []))
            if m.get('url'):
                dependencies[m['url'].split('#')[0] or 'local material_library'] += 1
            entries = [v for v in m.values() if isinstance(v, dict)]
            for e in m.get('extra', []):
                if not isinstance(e, dict):
                    continue
                shader[e.get('type', 'unknown')] += 1
                entries.extend(e.get('channels', []))
            count = 0
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                c = entry.get('channel', {})
                if not isinstance(c, dict) or not c.get('id'):
                    continue
                channels[c['id']] += 1
                count += int(any(k in c for k in ('value', 'current_value', 'image', 'image_file')))
            partial += int(count < 10)
        return dict(file=file, kind=kind, materials=len(materials), channels=dict(channels), shaders=dict(shader), dependencies=dict(dependencies), groups=sorted(groups), sparse_materials=partial, animations=len(data.get('scene', {}).get('animations', [])), images=len(data.get('image_library', [])))
    except Exception as e:
        return dict(file=file, error=str(e))


def main():
    output = Path(sys.argv[1])
    roots = sys.argv[2:]
    report = dict(roots=roots, scope='每个 DUF 头部；所有材质、层级材质、Shader 预设及未知类型完整解析；32 MiB 解压上限', scanned=0, material_files=0, materials=0, channels={}, shaders={}, dependencies={}, asset_types={}, errors=[], skipped_large=[], examples={}, sparse_materials=0, files_with_animations=0, files_with_images=0)
    files = (str(Path(folder) / name) for root in roots for folder, _, names in os.walk(root) for name in names if name.lower().endswith('.duf'))
    started = time.monotonic()
    last = started
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        while batch := list(itertools.islice(files, 128)):
            for r in pool.map(inspect, batch):
                report['scanned'] += 1
                if 'error' in r:
                    report['errors'].append(r)
                    continue
                kind = r['kind']
                report['asset_types'][kind] = report['asset_types'].get(kind, 0) + 1
                if r.get('skipped_large'):
                    report['skipped_large'].append(r['file'])
                if not r.get('materials'):
                    continue
                report['material_files'] += 1
                report['materials'] += r['materials']
                report['sparse_materials'] += r['sparse_materials']
                report['files_with_animations'] += int(bool(r['animations']))
                report['files_with_images'] += int(bool(r['images']))
                for name in ('channels', 'shaders', 'dependencies'):
                    for key, count in r[name].items():
                        report[name][key] = report[name].get(key, 0) + count
                categories = [kind]
                low = r['file'].lower()
                for key in ('makeup', 'eyes', 'nails', 'lie', 'face', 'skin', 'prop', 'hierarchical'):
                    if key in low or key in kind:
                        categories.append(key)
                for key in categories:
                    examples = report['examples'].setdefault(key, [])
                    if len(examples) < 12:
                        examples.append({k: r[k] for k in ('file', 'groups', 'materials', 'sparse_materials', 'animations', 'images')})
            if time.monotonic() - last > 10:
                status = {k: report[k] for k in ('scanned', 'material_files', 'materials')}
                status['elapsed_s'] = round(time.monotonic() - started)
                output.with_suffix('.progress.json').write_text(json.dumps(status), encoding='utf-8')
                print(status, flush=True)
                last = time.monotonic()
    report['elapsed_s'] = round(time.monotonic() - started)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print({k: report[k] for k in ('scanned', 'material_files', 'materials', 'elapsed_s')})


if __name__ == '__main__':
    main()
