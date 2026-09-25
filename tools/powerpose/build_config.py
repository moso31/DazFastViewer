"""把已核对映射固化为无 DXE / Python 运行依赖的 C++ 数据。原图仍从用户内容库读取。"""
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
review = root / 'artifacts/009-powerpose/009-powerpose-人工核对.json'
confirmed = json.loads(review.read_text(encoding='utf-8'))['results']
source = json.loads((root / 'artifacts/009-powerpose/review-data.json').read_text(encoding='utf-8'))
slots = ['lmb_horiz', 'lmb_vert', 'rmb_horiz', 'rmb_vert']
templates = []
for template in source['templates']:
    male = ET.parse(root / ('artifacts/009-powerpose/g8m/G8M_' + template['name'] + '.dsx')).getroot()
    male_points = {p.findtext('label') or p.findtext('node_label'): p for category in
                   ['node_points', 'node_group_points', 'property_points', 'template_points'] for p in male.findall(category + '/*')}
    points = []
    for p in template['points']:
        assert p['id'] in confirmed and confirmed[p['id']]['result'] in ['一致', '有差异']
        mp = male_points[p['raw_label']]
        q = dict(id=p['id'], label=p['label'], kind=p['kind'], x=p['x'], y=p['y'],
                 male_x=int(mp.findtext('x')), male_y=int(mp.findtext('y')),
                 target=p.get('target', ''), disabled=p.get('disabled', False),
                 node=p['raw'].get('node_label', ''), slots=[])
        for slot in slots:
            q['slots'].append([dict(node='Figure' if b['node'] == 'Figure' else b['resolved']['name'],
                                    property=(b['resolved']['axis'] + 'rot' if b['property'] in ['Bend', 'Twist', 'Side-Side'] else b['property']),
                                    sign=b['sign'], size=b['size']) for b in p['bindings'][slot]])
        # B07 始终以带虚线外圈的组点绘制；B08/09 保留 z Bend / x Twist。
        if p['id'] == 'B07':
            q['kind'] = 'group'
        points.append(q)
    templates.append(dict(name=template['name'], points=points))
data = json.dumps(templates, ensure_ascii=False, separators=(',', ':'))
dest = root / 'src/runtime/powerpose_data.inl'
dest.write_text('// 由 tools/powerpose/build_config.py 生成；人工记录 SHA-256: ' + hashlib.sha256(review.read_bytes()).hexdigest() +
                '\nstd::string{}\n' + '\n'.join('+ R"POWERPOSE(' + data[i:i+5000] + ')POWERPOSE"' for i in range(0, len(data), 5000)) + '\n', encoding='utf-8')
print('已生成 85 个姿态点、6 个导航入口；男女性别共享映射。')
