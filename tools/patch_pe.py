#!/usr/bin/env python3
"""Выставить флаги ASLR в заголовке PE после сборки FASM.

У FASM нет директив для DYNAMIC_BASE и HIGH_ENTROPY_VA (ARCHITECTURE.md, раздел 12),
поэтому поле DllCharacteristics дописывается здесь. NX_COMPAT FASM ставит сам
(format PE64 NX), но скрипт выставляет и его — на случай изменения заголовка.
Контрольная сумма пересчитывается. Скрипт не зависит от сторонних модулей.
"""
import struct
import sys

HIGH_ENTROPY_VA = 0x0020
DYNAMIC_BASE = 0x0040
NX_COMPAT = 0x0100
REQUIRED = HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT

PE32_PLUS = 0x20B
IMAGE_FILE_RELOCS_STRIPPED = 0x0001


def pe_checksum(data: bytes, checksum_offset: int) -> int:
    total = 0
    size = len(data)
    padded = data + b"\0" * (-size % 4)
    for i in range(0, len(padded), 4):
        if i == checksum_offset:
            continue
        total += struct.unpack_from("<I", padded, i)[0]
        total = (total & 0xFFFFFFFF) + (total >> 32)
    total = (total & 0xFFFF) + (total >> 16)
    total = (total & 0xFFFF) + (total >> 16)
    return (total + size) & 0xFFFFFFFF


def patch(path: str) -> None:
    data = bytearray(open(path, "rb").read())
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        sys.exit(f"{path}: not a PE file")
    characteristics = struct.unpack_from("<H", data, pe + 4 + 18)[0]
    if characteristics & IMAGE_FILE_RELOCS_STRIPPED:
        sys.exit(f"{path}: relocations stripped — ASLR impossible (missing 'data fixups'?)")
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != PE32_PLUS:
        sys.exit(f"{path}: not a PE32+ image")
    dll_char_off = opt + 70
    checksum_off = opt + 64
    flags = struct.unpack_from("<H", data, dll_char_off)[0]
    struct.pack_into("<H", data, dll_char_off, flags | REQUIRED)
    struct.pack_into("<I", data, checksum_off, 0)
    struct.pack_into("<I", data, checksum_off, pe_checksum(bytes(data), checksum_off))
    open(path, "wb").write(data)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: patch_pe.py <file.exe>...")
    for name in sys.argv[1:]:
        patch(name)
