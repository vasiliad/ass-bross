"""Тесты вехи M1: арены памяти (встроенная самопроверка --selftest arena)."""
from conftest import needs_runtime


@needs_runtime
def test_selftest_arena(run):
    proc = run("--selftest", "arena")
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert proc.stdout.decode().strip() == "arena: ok"


@needs_runtime
def test_selftest_unknown_name(run):
    proc = run("--selftest", "no-such-test")
    assert proc.returncode == 2
