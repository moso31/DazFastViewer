"""提取 HYG v4.1 的亮星，生成可离线使用的星表（CC BY-SA 4.0）。"""
import csv
import hashlib
import io
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://raw.githubusercontent.com/astronexus/HYG-Database/3bf37f4b2d5460e1278286320d1d62fab9b493c1/hyg/CURRENT/hygdata_v41.csv'

if __name__ == '__main__':
    data = urllib.request.urlopen(URL, timeout=90).read()
    stars = [s for s in csv.DictReader(io.StringIO(data.decode('utf-8'))) if s['id'] != '0' and float(s['mag']) <= 7]
    target = ROOT / 'third_party/hyg'
    target.mkdir(parents=True, exist_ok=True)
    rows = ['// HYG v4.1 / David Nash. CC BY-SA 4.0. See LICENSE.txt.', '// ra (hours), dec (degrees), visual magnitude, B-V.']
    for s in stars:
        rows.append('{' + ','.join(f'{float(s[k] or .65):.6f}f' for k in ('ra', 'dec', 'mag', 'ci')) + '},')
    (target / 'bright_stars.inl').write_text('\n'.join(rows) + '\n', encoding='utf-8')
    print(f'{len(stars)} stars; input sha256={hashlib.sha256(data).hexdigest()}')
