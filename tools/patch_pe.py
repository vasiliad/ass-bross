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
    
    # Sort .pdata (Exception Directory = DataDirectory[3])
    # DataDirectories start at opt_hdr + 112. Index 3 is at 112 + 24 = 136.
    num_rva_sizes = struct.unpack_from("<I", data, opt + 108)[0]
    if num_rva_sizes > 3:
        exc_rva, exc_size = struct.unpack_from("<II", data, opt + 136)
        if exc_rva != 0 and exc_size > 0:
            num_sections = struct.unpack_from("<H", data, pe + 6)[0]
            opt_hdr_size = struct.unpack_from("<H", data, pe + 20)[0]
            sections_start = pe + 24 + opt_hdr_size
            
            file_offset = 0
            for i in range(num_sections):
                sec_hdr = sections_start + i * 40
                sec_rva, sec_vsize, sec_raw_size, sec_raw_ptr = struct.unpack_from("<IIII", data, sec_hdr + 12)
                if sec_rva <= exc_rva < sec_rva + max(sec_vsize, sec_raw_size):
                    file_offset = sec_raw_ptr + (exc_rva - sec_rva)
                    break
            
            if file_offset > 0:
                entries = []
                for i in range(0, exc_size, 12):
                    entries.append(data[file_offset + i : file_offset + i + 12])
                entries.sort(key=lambda e: struct.unpack("<I", e[:4])[0])
                for i, e in enumerate(entries):
                    data[file_offset + i * 12 : file_offset + (i + 1) * 12] = e

    struct.pack_into("<I", data, checksum_off, 0)
    struct.pack_into("<I", data, checksum_off, pe_checksum(bytes(data), checksum_off))
    open(path, "wb").write(data)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: patch_pe.py <file.exe>...")
    for name in sys.argv[1:]:
        patch(name)
