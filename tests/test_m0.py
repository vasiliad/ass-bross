"""Тесты вехи M0: заголовок PE, командная строка, настройки, бенчмарк, журнал сбоев."""
import re
import shutil
import subprocess
import sys

import pytest

from conftest import BUILD, ROOT, needs_runtime

EXCEPTION_ACCESS_VIOLATION = 0xC0000005


def test_pe_headers():
    exes = [str(BUILD / "nanoweb.exe"), str(BUILD / "nanoweb-con.exe")]
    proc = subprocess.run([sys.executable, str(ROOT / "tools" / "check_pe.py"), *exes],
                          capture_output=True, text=True)
    assert proc.returncode == 0, proc.stdout + proc.stderr


@needs_runtime
def test_version(run, golden):
    proc = run("--version")
    assert proc.returncode == 0
    golden("version.txt", proc.stdout.decode())


@needs_runtime
def test_help(run):
    proc = run("--help")
    assert proc.returncode == 0
    assert proc.stdout.startswith(b"Usage: nanoweb")


@needs_runtime
def test_unknown_option(run):
    proc = run("--no-such-option")
    assert proc.returncode == 2
    assert b"unknown option" in proc.stderr


INI = (
    "﻿; комментарий\r\n"
    "[Search]\r\n"
    "default = https://lite.duckduckgo.com/lite/?q=%s\r\n"
    "  W =https://ru.wikipedia.org/w/index.php?search=%s  \r\n"
    "# ещё комментарий\r\n"
    "[my section]\n"
    "Key With Spaces = значение с пробелами\n"
    "empty =\n"
)


@pytest.fixture
def portable(tmp_path, exe_path):
    """Копия exe в отдельном каталоге с nanoweb.ini рядом (переносной режим)."""
    exe = tmp_path / exe_path.name
    shutil.copy(exe_path, exe)
    (tmp_path / "nanoweb.ini").write_bytes(INI.encode("utf-8"))
    from conftest import Runner
    return Runner(exe)


@needs_runtime
@pytest.mark.parametrize("section, key, expected", [
    ("search", "default", "https://lite.duckduckgo.com/lite/?q=%s"),
    ("SEARCH", "w", "https://ru.wikipedia.org/w/index.php?search=%s"),
    ("my section", "key with spaces", "значение с пробелами"),
    ("my section", "empty", ""),
])
def test_get_setting(portable, section, key, expected):
    proc = portable("--get-setting", section, key)
    assert proc.returncode == 0, proc.stderr
    assert proc.stdout.decode("utf-8") == expected + "\n"


@needs_runtime
def test_get_setting_missing(portable):
    proc = portable("--get-setting", "search", "nope")
    assert proc.returncode == 4
    assert proc.stdout == b""


@needs_runtime
def test_bench_noop(run):
    proc = run("--bench", "noop", "1000")
    assert proc.returncode == 0
    assert re.fullmatch(rb"bench noop iterations=1000 ns=\d+\n", proc.stdout), proc.stdout


@needs_runtime
def test_bench_bad_count(run):
    assert run("--bench", "noop", "12x").returncode == 2


@needs_runtime
def test_crash_log(run_con):
    proc = run_con("--crash-test")
    assert proc.returncode & 0xFFFFFFFF == EXCEPTION_ACCESS_VIOLATION
    text = proc.stderr.decode()
    assert "NanoWeb crash: code=C0000005" in text
    assert re.search(r"rip=[0-9A-F]{16}", text)
