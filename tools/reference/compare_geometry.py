"""将原生求值顶点与 DAZ 导出的 DBZ 世界坐标逐顶点对照；不替换待测几何。"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

import numpy as np


def read(path):
    data = Path(path).read_bytes()
    return json.loads(gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data)


def compare(native, dbz):
    result = {"native": str(native), "reference": str(dbz),
              "reference_sha256": hashlib.sha256(Path(dbz).read_bytes()).hexdigest(),
              "reference_kind": "DAZ Studio evaluated DBZ vertices", "objects": []}
    reference = read(dbz)
    for obj in read(native)["objects"]:
        matches = [f for f in reference["figures"] if f.get("label") == obj["label"]]
        if len(matches) != 1:
            raise ValueError(f"DBZ 身份匹配不唯一：{obj['label']}")
        vertices = np.asarray(matches[0]["vertices"], dtype=np.float64)
        expected = vertices[:, [0, 2, 1]] * [0.01, -0.01, 0.01]
        actual = np.asarray(obj["world"])
        if expected.shape != actual.shape:
            raise ValueError(f"顶点数不一致：{obj['label']}")
        error = np.linalg.norm(actual - expected, axis=1)
        item = {"label": obj["label"], "vertices": len(error),
                "rms_mm": float(np.sqrt(np.mean(error ** 2)) * 1000),
                "max_mm": float(error.max() * 1000),
                "p95_mm": float(np.percentile(error, 95) * 1000)}
        if obj["label"] in ("lit", "lit2"):
            head = np.asarray(obj["base"])[:, 2] > 1.6
            item["head_rms_mm"] = float(np.sqrt(np.mean(error[head] ** 2)) * 1000)
        result["objects"].append(item)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("dbz", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = compare(args.native, args.dbz)
    text = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    print(text)
