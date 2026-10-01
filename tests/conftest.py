"""Общая настройка тестов NanoWeb.

Исполняемый файл запускается напрямую на Windows и через Wine на Linux.
Если ни то ни другое недоступно, тесты, которым нужен запуск, пропускаются
(статические проверки PE выполняются всегда). Полный прогон — в CI на Windows.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
GOLDEN = Path(__file__).resolve().parent / "golden"


def pytest_addoption(parser):
    parser.addoption("--update-golden", action="store_true",
                     help="перезаписать эталоны в tests/golden вместо сравнения")


def _launcher():
    if sys.platform == "win32":
        return []
    wine = shutil.which("wine")
    if wine:
        return [wine]
    return None


LAUNCHER = _launcher()
needs_runtime = pytest.mark.skipif(LAUNCHER is None, reason="нет Windows или Wine для запуска .exe")
needs_windows = pytest.mark.skipif(sys.platform != "win32", reason="нужна настоящая Windows")


class Runner:
    def __init__(self, exe: Path):
        self.exe = exe

    def __call__(self, *args, cwd=None, timeout=30, appdata=None):
        env = dict(os.environ, WINEDEBUG="-all")
        if appdata is not None:
            env["APPDATA"] = str(appdata)
        proc = subprocess.run(LAUNCHER + [str(self.exe), *args], capture_output=True,
                              cwd=cwd, env=env, timeout=timeout)
        return proc

    def raw(self, command_tail: str, timeout=30):
        """Запуск с командной строкой «как есть» — для проверки правил разбора кавычек.
        Только Windows: под Wine строка всё равно проходит через разбор на стороне Linux."""
        env = dict(os.environ)
        return subprocess.run(f'"{self.exe}" {command_tail}', capture_output=True, env=env,
                              timeout=timeout)


@pytest.fixture(params=["nanoweb-con.exe", "nanoweb.exe"])
def exe_path(request):
    path = BUILD / request.param
    if not path.exists():
        pytest.fail(f"{path} не собран — выполните make / build.bat")
    return path


@pytest.fixture
def run(exe_path):
    return Runner(exe_path)


@pytest.fixture
def run_con():
    return Runner(BUILD / "nanoweb-con.exe")


@pytest.fixture
def golden(request):
    """Сравнить текст с tests/golden/<name>; с --update-golden — перезаписать эталон."""
    update = request.config.getoption("--update-golden")

    def check(name: str, actual: str):
        path = GOLDEN / name
        if update or not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(actual, encoding="utf-8", newline="\n")
            if not update:
                pytest.fail(f"эталон {name} создан — проверьте его и перезапустите тесты")
            return
        assert actual == path.read_text(encoding="utf-8"), f"расхождение с эталоном {name}"

    return check
