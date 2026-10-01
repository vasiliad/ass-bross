# ИНСТРУМЕНТЫ РАЗРАБОТКИ
**Проект:** NanoWeb · FASM · Windows x64

## 1. Обязательные
| Инструмент | Назначение | Примечание |
|---|---|---|
| **FASM 1.73** (Windows-пакет) | Ассемблер, сразу выдаёт PE64 | Из пакета для Windows нужны макросы `INCLUDE/` (`win64a.inc`, `macro/struct.inc`, `macro/proc64.inc`, `macro/com64.inc` для `interface`/`comcall`) |
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
* Быстрый запуск: **Wine** (`wine build/nanoweb.exe --dump-dom tests/pages/basic.html`). GDI, WinHTTP и WIC под Wine работают (поведение сети и декодеров на Windows всё равно проверяется отдельно). Шрифтов Segoe UI и Consolas под Wine нет — поэтому эталоны раскладки снимаются с тестовым шрифтом `--font-mock`.
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
* **Golden-тесты:** `nanoweb.exe --dump-dom|--dump-hidden|--dump-layout --font-mock file.html` → сравнение с `tests/golden/*.txt`. Обновление эталонов: `pytest --update-golden` (изменения эталонов проверяются глазами в diff).
* **Корпус реальных страниц:** `tests/pages/` — сохранённые копии сайтов из тестового набора (с картинками), для каждой — эталоны `--dump-dom`, `--dump-hidden`, `--dump-layout`.
* **Потоковый разбор:** каждый golden-тест `--dump-dom` прогоняется и с `--chunk 1`, `--chunk 7`, `--chunk 4096` — результат обязан совпадать.
* **Сеть:** `tests/server/` — Python-сервер с ответами на крайние случаи (редиректы, редирект https→http, обрыв соединения, медленная отдача, большие и битые картинки, windows-1251 без `charset`, `text/plain`, PDF) и стресс-тест быстрой навигации.
* **Фаззинг** — с появления каждого разборщика (M1 — HTML, M2 — CSS, M3 — картинки): мутационный фаззер на Python в `tests/fuzz/`, найденные падения сохраняются как регрессионные тесты. WinAFL — по мере необходимости.

## 5. Измерения (KPI)
| Инструмент | Метрика |
|---|---|
| `nanoweb.exe --stats` | Время этапов (`QueryPerformanceCounter`), Private Bytes (`K32GetProcessMemoryInfo`) |
| `nanoweb.exe --bench parse\|vis\|layout\|find` | Скорость этапов на корпусе `tests/bench/` |
| **Process Explorer** | Private Bytes, Working Set, CPU в простое |
| **Windows Performance Analyzer** (ETW) | Профилирование горячих мест |

## 6. CI
GitHub Actions, раннер `windows-latest`:
1. Скачать FASM 1.73.
2. `build.bat` и `build.bat debug`.
3. Проверка заголовка PE: флаги `HIGH_ENTROPY_VA`, `DYNAMIC_BASE`, `NX_COMPAT`, наличие перемещений (Python-скрипт `tools/check_pe.py`).
4. `pytest` (golden-тесты с `--font-mock`, тесты порций, короткий прогон фаззера).
5. Опубликовать `nanoweb.exe` как артефакт сборки и проверить KPI размера (< 1 МБ).

## 7. Справочные материалы
* Спецификация HTML (раздел Parsing): https://html.spec.whatwg.org/multipage/parsing.html
* CSS Selectors (для правил видимости): https://www.w3.org/TR/selectors-3/
* WinHTTP: https://learn.microsoft.com/windows/win32/winhttp/winhttp-start-page
* WIC: https://learn.microsoft.com/windows/win32/wic/-wic-lh
* Документация FASM: https://flatassembler.net/docs.php
* Win32 API: https://learn.microsoft.com/windows/win32/api/
