# ИНСТРУМЕНТЫ РАЗРАБОТКИ
**Проект:** NanoWeb · FASM · Windows x64

## 1. Обязательные
| Инструмент | Назначение | Примечание |
|---|---|---|
| **FASM 1.73** (Windows-пакет) | Ассемблер, сразу выдаёт PE64 | Из пакета для Windows нужны макросы `INCLUDE/` (`win64a.inc`, `macro/struct.inc`, `macro/proc64.inc`) |
| **Git** | Контроль версий | |
| **Python 3.11+ и pytest** | Тестовый стенд (golden-тесты, тестовый HTTP-сервер) | |

Сборка на Windows:
```bat
build.bat
```
Под капотом это `fasm src\main.asm build\nanoweb.exe`, перед вызовом в переменную окружения `INCLUDE` записывается путь к макросам FASM.

## 2. Разработка на Linux
FASM для Linux собирает Windows-исполняемые файлы точно так же: формат результата задаётся директивой `format PE64` в исходнике. Нужно только скопировать каталог `INCLUDE/` из Windows-пакета FASM.

* Сборка: `make` (вызывает `fasm` и задаёт `INCLUDE`).
* Быстрый запуск: **Wine** (`wine build/nanoweb.exe --dump-dom tests/pages/basic.html`). GDI, Winsock и SChannel под Wine работают.
* Тесты: `make test` запускает pytest, который вызывает exe через Wine.
* Финальная проверка каждой вехи — на настоящей Windows (виртуальная машина или CI).

## 3. Отладка
| Инструмент | Для чего |
|---|---|
| **x64dbg** | Основной отладчик: пошаговое выполнение, регистры, память. Работает и под Wine |
| **WinDbg** | Разбор падений, дампы памяти |
| **DebugView** (Sysinternals) | Просмотр вывода `OutputDebugStringW` без отладчика |
| Листинг FASM (`fasm -s` + утилита `listing`) | Сопоставление адресов в отладчике с исходником (FASM не создаёт PDB) |

Отладочная сборка (`build.bat debug`) использует подсистему `console`: вывод `--dump-*` и `dbg_print_*` сразу виден в терминале. Релизная — `GUI`.

## 4. Тестирование
* **Golden-тесты:** `nanoweb.exe --dump-dom|--dump-style|--dump-layout file.html` → сравнение с `tests/golden/*.txt`. Обновление эталонов: `pytest --update-golden` (изменения эталонов проверяются глазами в diff).
* **html5lib-tests:** конвертер из их формата в наш дамп; отслеживается доля пройденных тестов для поддерживаемого подмножества.
* **Сеть:** `tests/server/` — Python-сервер с ответами на крайние случаи (chunked, редиректы, обрыв соединения, медленная отдача, windows-1251).
* **Устойчивость к мусору:** прогон токенизатора и CSS-парсера на случайных и испорченных файлах (простой мутационный фаззер на Python). Полноценный фаззинг (WinAFL) — после M2.

## 5. Измерения (KPI)
| Инструмент | Метрика |
|---|---|
| `nanoweb.exe --stats` | Время старта и этапов (`QueryPerformanceCounter`), Private Bytes (`GetProcessMemoryInfo`) |
| **Process Explorer** | Private Bytes, Working Set, CPU в простое |
| **hyperfine** | Время холодного старта по многим запускам |
| **Windows Performance Analyzer** (ETW) | Профилирование горячих мест |

## 6. CI
GitHub Actions, раннер `windows-latest`:
1. Скачать FASM 1.73.
2. `build.bat` и `build.bat debug`.
3. `pytest`.
4. Опубликовать `nanoweb.exe` как артефакт сборки и проверить KPI размера (< 1.5 МБ).

## 7. Справочные материалы
* Спецификация HTML (раздел Parsing): https://html.spec.whatwg.org/multipage/parsing.html
* CSS 2.1 (box model, visual formatting model): https://www.w3.org/TR/CSS21/
* html5lib-tests: https://github.com/html5lib/html5lib-tests
* Документация FASM: https://flatassembler.net/docs.php
* Win32 API: https://learn.microsoft.com/windows/win32/api/
