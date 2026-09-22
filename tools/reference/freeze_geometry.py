"""将诊断顶点生成独立静态 DUF，用于快速检查；从不覆盖原始资产。"""
import argparse,copy,gzip,json
from pathlib import Path
from urllib.parse import unquote

def read(path):
    raw=Path(path).read_bytes()
    return json.loads(gzip.decompress(raw) if raw[:2]==b"\x1f\x8b" else raw)

def freeze(native, output, roots):
    d=read(native);source=read(d['input']);result=copy.deepcopy(source)
    if output.resolve() in {native.resolve(),Path(d['input']).resolve()}:
        raise ValueError('输出不能覆盖输入或源 DUF')
    result['asset_info']={'type':'scene'};result['scene']['nodes']=[];result['scene']['modifiers']=[];result['geometry_library']=[]
    result.pop('modifier_library',None)
    shapes=set()
    for obj in d['objects']:
        node=next(n for n in source['scene']['nodes'] if n['id']==obj['id'].rsplit('/',1)[0]);instance=copy.deepcopy(node['geometries'][0]);shapes.add('#'+instance['id'])
        uri=unquote(instance['url']);file,ident=uri.split('#');path=next(r/file.lstrip('/') for r in roots if (r/file.lstrip('/')).exists()) if file else Path(d['input']);asset=read(path)
        g=copy.deepcopy(next(g for g in asset['geometry_library'] if g['id']==ident));g['id']=instance['id']+'_frozen';g.pop('source',None);g.pop('graft',None)
        g['vertices']={'count':len(obj['world']),'values':[[x*100,z*100,-y*100] for x,y,z in obj['world']]}
        if g.get('default_uv_set','').startswith('#'):g['default_uv_set']=file+g['default_uv_set']
        instance['url']='#'+g['id'];result['geometry_library'].append(g);result['scene']['nodes'].append({'id':node['id'],'type':'node','label':node['label'],'geometries':[instance]})
    result['scene']['materials']=[m for m in result['scene']['materials'] if unquote(m.get('geometry','')) in shapes]
    for n in source['scene']['nodes']:
        if 'Options' in n.get('label',''):result['scene']['nodes'].append(n)
    with output.open('x',encoding='utf-8') as f:
        json.dump(result,f,ensure_ascii=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('native',type=Path);p.add_argument('output',type=Path);p.add_argument('--content-root',action='append',type=Path,required=True);a=p.parse_args();freeze(a.native,a.output,a.content_root)
