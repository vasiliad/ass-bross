"""Тесты вехи M0: заголовок PE, командная строка, настройки, бенчмарк, журнал сбоев."""
import re
import shutil
import subprocess
import sys

import pytest

from conftest import BUILD, ROOT, needs_runtime, needs_windows

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
def test_crash_log(run_con, tmp_path):
    proc = run_con("--crash-test", appdata=tmp_path)
    assert proc.returncode & 0xFFFFFFFF == EXCEPTION_ACCESS_VIOLATION
    text = proc.stderr.decode()
    assert "NanoWeb crash: code=C0000005" in text
    regs = re.findall(r"(rax|rbx|rsp|rip|r15)=([0-9A-F]{16})", text)
    assert {name for name, _ in regs} == {"rax", "rbx", "rsp", "rip", "r15"}
    rip = int(dict(regs)["rip"], 16)
    assert rip >= 1 << 32, "образ загружен ниже 4 ГБ — high-entropy ASLR не применился"
    log = (tmp_path / "NanoWeb" / "crash.log").read_text()
    assert log == text


@needs_runtime
def test_settings_do_not_create_appdata_dir(run_con, tmp_path):
    run_con("--get-setting", "search", "default", appdata=tmp_path)
    assert not (tmp_path / "NanoWeb").exists()


@needs_runtime
def test_get_setting_wrong_arg_count(run):
    assert run("--get-setting", "search").returncode == 2


@needs_runtime
def test_bench_default_iterations(run):
    proc = run("--bench", "noop")
    assert re.fullmatch(rb"bench noop iterations=1000000 ns=\d+\n", proc.stdout), proc.stdout


@needs_runtime
def test_bench_saturation(run):
    proc = run("--bench", "noop", "99999999999999999999")
    assert proc.returncode == 0
    assert proc.stdout.startswith(b"bench noop iterations=4294967295 ")


@needs_runtime
def test_dump_args_simple(run_con):
    proc = run_con("--dump-args", "один", "two words")
    assert proc.returncode == 0
    lines = proc.stdout.decode("utf-8").splitlines()
    assert lines[0] == "argc=4"
    assert lines[2:] == ["[1] --dump-args", "[2] один", "[3] two words"]


@needs_runtime
def test_too_many_args(run_con):
    proc = run_con("--dump-args", *["x"] * 40)
    assert proc.returncode == 2
    assert b"too many arguments" in proc.stderr


# Правила CommandLineToArgvW: хвост командной строки → ожидаемые аргументы после --dump-args.
CMDLINE_CASES = [
    ('a b', ["a", "b"]),
    ('"a b" c', ["a b", "c"]),
    ('a\\b', ["a\\b"]),                 # слэш без кавычки — буквальный
    ('a\\\\"b c"', ["a\\b c"]),           # 2 слэша + кавычка → 1 слэш, кавычка открывает группу
    ('a\\"b', ['a"b']),                   # 1 слэш + кавычка → буквальная кавычка
    ('a\\\\\\"b', ['a\\"b']),               # 3 слэша + кавычка → слэш и буквальная кавычка
    ('"a""b"', ['a"b']),                  # "" внутри кавычек → кавычка
    ('""', [""]),                         # пустой аргумент
    ('\t a \t b ', ["a", "b"]),
    ('x\\', ["x\\"]),                     # слэш в конце
]


@needs_windows
@pytest.mark.parametrize("tail, expected", CMDLINE_CASES)
def test_cmdline_rules(run_con, tail, expected):
    proc = run_con.raw("--dump-args " + tail)
    assert proc.returncode == 0, proc.stderr
    lines = proc.stdout.decode("utf-8").splitlines()
    assert lines[0] == f"argc={2 + len(expected)}"
    got = [line.split("] ", 1)[1] if "] " in line else "" for line in lines[3:]]
    assert got == expected
