#!/usr/bin/env python3
"""Проверка собранного nanoweb.exe (запускается в CI и `make check`).

* флаги HIGH_ENTROPY_VA, DYNAMIC_BASE, NX_COMPAT;
* наличие секции перемещений и отсутствие флага RELOCS_STRIPPED;
* наличие манифеста (RT_MANIFEST);
* размер файла < 1 МБ (ARCHITECTURE.md, раздел 14);
* импорт только из разрешённых DLL (ARCHITECTURE.md, раздел 1).
"""
import os
import sys

import pefile

REQUIRED_FLAGS = {
    "IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA": 0x0020,
    "IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE": 0x0040,
    "IMAGE_DLLCHARACTERISTICS_NX_COMPAT": 0x0100,
}
ALLOWED_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "winhttp.dll", "ole32.dll", "windowscodecs.dll",
}
MAX_SIZE = 1 << 20
RT_MANIFEST = 24


def check(path: str) -> list[str]:
    errors = []
    pe = pefile.PE(path)
    flags = pe.OPTIONAL_HEADER.DllCharacteristics
    for name, bit in REQUIRED_FLAGS.items():
        if not flags & bit:
            errors.append(f"missing {name}")
    if pe.FILE_HEADER.Characteristics & 0x0001:
        errors.append("IMAGE_FILE_RELOCS_STRIPPED is set")
    reloc_dir = pe.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_BASERELOC"]]
    if reloc_dir.VirtualAddress == 0:
        errors.append("no base relocation directory")
    resources = getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None)
    if resources is None or not any(e.id == RT_MANIFEST for e in resources.entries):
        errors.append("no RT_MANIFEST resource")
    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        dll = entry.dll.decode().lower()
        if dll not in ALLOWED_DLLS:
            errors.append(f"import from disallowed DLL {dll}")
    size = os.path.getsize(path)
    if size >= MAX_SIZE:
        errors.append(f"file size {size} >= {MAX_SIZE}")
    pe.close()
    return errors


def main() -> int:
    failed = False
    for path in sys.argv[1:]:
        errors = check(path)
        status = "OK" if not errors else "FAIL"
        print(f"{status}  {path}  ({os.path.getsize(path)} bytes)")
        for err in errors:
            print(f"      {err}")
        failed |= bool(errors)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
