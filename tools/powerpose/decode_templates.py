"""离线读取本机 PowerPose DXE，输出经过 XML 和密文回写校验的 DSX。

只用于 009 的模板研究；不修改 DAZ 安装、内容库或当前场景。
Twofish 使用 python-twofish 的本机扩展 ABI，不依赖其旧版 imp 包装器。
"""
from __future__ import annotations

import argparse
import ctypes as ct
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


KNOWN_PLUGINS = {
    # 本机 DAZ Studio 4 PowerPose；偏移为文件偏移，不是 RVA。
    "eca69b6042fbdb76e72abb30dbbb66c09abd7cb49042511ac9c1293be4a7acb9": 0xDD810,
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class Twofish:
    """调用 Niels Ferguson Twofish 实现；不复制 DAZ 的解码实现。"""

    class Key(ct.Structure):
        _fields_ = [("s", (ct.c_uint32 * 4) * 256), ("k", ct.c_uint32 * 40)]

    def __init__(self, key: bytes, dependency_dir: Path):
        if len(key) != 32:
            raise ValueError("需要 32 字节 Twofish 密钥")
        sys.path.insert(0, str(dependency_dir.resolve()))
        spec = importlib.util.find_spec("_twofish")
        if spec is None or not spec.origin:
            raise RuntimeError("缺少依赖：python -m pip install --target .research/powerpose-python twofish==0.3.0")
        self.library = ct.CDLL(spec.origin)
        init = self.library.exp_Twofish_initialise
        init.argtypes, init.restype = [], None
        init()
        prepare = self.library.exp_Twofish_prepare_key
        prepare.argtypes = [ct.c_char_p, ct.c_int, ct.POINTER(self.Key)]
        prepare.restype = None
        self.key = self.Key()
        prepare(key, len(key), ct.byref(self.key))
        for name in ("encrypt", "decrypt"):
            function = getattr(self.library, "exp_Twofish_" + name)
            function.argtypes = [ct.POINTER(self.Key), ct.c_char_p, ct.c_void_p]
            function.restype = None

    def blocks(self, data: bytes, encrypt: bool = False) -> bytes:
        if not data or len(data) % 16:
            raise ValueError("DXE 必须是非空的 16 字节整块；拒绝补齐损坏密文")
        function = getattr(self.library, "exp_Twofish_" + ("encrypt" if encrypt else "decrypt"))
        output = bytearray()
        for offset in range(0, len(data), 16):
            block = ct.create_string_buffer(16)
            function(ct.byref(self.key), data[offset:offset + 16], block)
            output.extend(block.raw)
        return bytes(output)


def read_key(plugin: Path) -> tuple[bytes, dict]:
    binary = plugin.read_bytes()
    digest = sha256(binary)
    if digest not in KNOWN_PLUGINS:
        raise ValueError("此插件版本尚未验证；拒绝沿用其他版本的密钥偏移。SHA-256=" + digest)
    offset = KNOWN_PLUGINS[digest]
    return binary[offset:offset + 32], {"path": str(plugin.resolve()), "sha256": digest, "key_file_offset": offset}


def decode(cipher: Twofish, data: bytes) -> tuple[bytes, ET.Element]:
    padded = cipher.blocks(data)
    plain = padded.rstrip(b"\0")
    if len(padded) - len(plain) > 15:
        raise ValueError("零填充长度不符合已验证模板格式")
    if b"<!DOCTYPE" in plain.upper() or b"<!ENTITY" in plain.upper():
        raise ValueError("不接受含 DTD／实体声明的模板")
    root = ET.fromstring(plain.decode("utf-8"))
    if root.tag not in ("template_suite", "template_file"):
        raise ValueError("解码结果不是 PowerPose 模板")
    if cipher.blocks(padded, encrypt=True) != data:
        raise ValueError("重新编码与源密文不一致")
    return plain, root


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("template_dir", type=Path)
    parser.add_argument("--plugin", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--dependencies", type=Path, default=Path(".research/powerpose-python"))
    args = parser.parse_args()
    source, output = args.template_dir.resolve(), args.output.resolve()
    if output == source or output.is_relative_to(source):
        parser.error("输出目录必须位于源模板目录之外")
    key, plugin_info = read_key(args.plugin)
    cipher = Twofish(key, args.dependencies)
    # 公开的 Twofish 256 位已知答案；同时验证 Python 3.12 ctypes 包装。
    known = Twofish(bytes.fromhex("D43BB7556EA32E46F2A282B7D45B4E0D57FF739D4DC92C1BD7FC01700CC8216F"), args.dependencies)
    expected = bytes.fromhex("6CB4561C40BF0A9705931CB6D408E7FA")
    answer = known.blocks(bytes.fromhex("90AFE91BB288544F2C32DC239B2635E6"), encrypt=True)
    if answer != expected or known.blocks(answer) != bytes.fromhex("90AFE91BB288544F2C32DC239B2635E6"):
        raise RuntimeError("Twofish 已知答案校验失败")
    files = sorted(p for p in source.glob("*.dxe") if p.stem == "Templates" or p.stem.endswith(("_Body", "_Hands", "_Head")))
    if len(files) != 4:
        raise ValueError("需要一个 Templates 索引和 Body／Hands／Head 三个模板")
    staged, evidence = [], []
    for path in files:
        data = path.read_bytes()
        plain, root = decode(cipher, data)
        counts = {tag: len(root.findall(tag + "/*")) for tag in ("template_points", "node_points", "node_group_points", "property_points")}
        staged.append((output / (path.stem + ".dsx"), plain))
        evidence.append({"source": str(path), "sha256": sha256(data), "source_bytes": len(data), "xml_sha256": sha256(plain), "xml_bytes": len(plain), "root": root.tag, "counts": counts, "round_trip": True})
    output.mkdir(parents=True, exist_ok=True)
    for path, plain in staged:
        path.write_bytes(plain)
    for item in evidence:
        if sha256(Path(item["source"]).read_bytes()) != item["sha256"]:
            raise RuntimeError("研究期间源文件发生变化")
    report = {"format": "Twofish-256 / ECB / zero padding / UTF-8 XML", "plugin": plugin_info, "known_answer": True, "source_unchanged": True, "files": evidence}
    (output / "decode-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(output), "files": len(files), "xml_valid": True, "round_trip": True, "source_unchanged": True}, ensure_ascii=False))


if __name__ == "__main__":
    main()
