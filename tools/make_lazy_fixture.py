"""生成只用于 Morph 异步界面回归的独立小场景。"""
import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)


def write(relative, data):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")


write("data/Lazy/figure.dsf", {
    "node_library": [{"id": "figure", "type": "figure"}],
    "geometry_library": [{"id": "geometry",
                          "vertices": {"count": 3, "values": [[0, 0, 0], [100, 0, 0], [0, 100, 0]]},
                          "polylist": {"count": 1, "values": [[0, 0, 0, 1, 2]]},
                          "polygon_material_groups": {"count": 1, "values": ["Skin"]},
                          "default_uv_set": "#uv"}],
    "uv_set_library": [{"id": "uv", "vertex_count": 3,
                        "uvs": {"count": 3, "values": [[0, 0], [1, 0], [0, 1]]}}],
})
for name, vertex in [("A", 0), ("B", 1)]:
    write(f"data/Lazy/Morphs/{name}.dsf", {"modifier_library": [{
        "id": name, "parent": "/data/Lazy/figure.dsf#geometry", "group": "/测试",
        "channel": {"type": "float", "label": name, "value": 0, "min": 0, "max": 1},
        "morph": {"vertex_count": 3, "deltas": {"count": 1, "values": [[vertex, 20, 0, 0]]}},
    }]})
write("scene.duf", {"scene": {
    "nodes": [{"id": "person", "url": "/data/Lazy/figure.dsf#figure",
               "geometries": [{"id": "shape", "url": "/data/Lazy/figure.dsf#geometry"}]}],
    "materials": [{"id": "mat", "geometry": "#shape", "groups": ["Skin"]}],
}})
write("project.json", {"version": 1, "content_roots": [root.as_posix()]})
print(root / "scene.duf")
