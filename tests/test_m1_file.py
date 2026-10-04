"""Тесты вехи M1: файлы (встроенная самопроверка --selftest file)."""
from conftest import needs_runtime


@needs_runtime
def test_selftest_file(run):
    proc = run("--selftest", "file")
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert proc.stdout.decode().strip() == "arena: ok"
