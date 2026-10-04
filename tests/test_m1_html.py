"""Тесты вехи M1: HTML токенизатор."""
from conftest import needs_runtime


@needs_runtime
def test_selftest_html(run):
    proc = run("--selftest", "html")
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert proc.stdout.decode().strip() == "arena: ok"
