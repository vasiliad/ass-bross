# СОГЛАШЕНИЯ ПО КОДУ
**Проект:** NanoWeb · FASM · Windows x64

## 1. Структура репозитория
```text
nanoweb/
├── src/
│   ├── main.asm              ; format PE64 NX GUI 6.0, точка входа, импорт, манифест, fixups, include модулей
│   ├── include/
│   │   ├── structs.inc       ; все struct из MEMORY_MODEL.md
│   │   └── consts.inc        ; TAG_*, ATTR_*, ROLE_*, CS_*, лимиты (LIM_*), коды ошибок, WM_NET
│   ├── hal/win64/            ; mem.inc, net.inc (WinHTTP), gui.inc, font.inc, image.inc (WIC), file.inc, os.inc
│   ├── core/                 ; arena.inc, eventloop.inc, url.inc, encoding.inc, settings.inc, history.inc
│   ├── engine/               ; html_tokenizer.inc, dom.inc, visibility.inc, images.inc, reader_layout.inc
│   ├── paint/                ; painter.inc
│   ├── ui/                   ; addressbar.inc, statusbar.inc, hittest.inc, selection.inc, find.inc, search.inc
│   ├── debug/                ; dbg_print.inc, dump_dom.inc, dump_layout.inc, stats.inc, crash.inc, font_mock.inc
│   └── data/                 ; tags.inc (теги и роли), attrs.inc, entities.inc, fonts.inc, casefold.inc
├── tests/
│   ├── pages/                ; корпус реальных страниц (с картинками)
│   ├── fuzz/                 ; мутационный фаззер и найденные падения
│   ├── golden/               ; эталонный вывод --dump-*
│   ├── bench/                ; корпус для замеров скорости
│   ├── server/               ; локальный HTTP-сервер для тестов сети
│   └── test_*.py             ; pytest
├── docs/
├── build.bat
└── Makefile
```
Сборка — одна единица трансляции: `main.asm` подключает все модули через `include`. Линкер не нужен.

## 2. Соглашение о вызовах: Microsoft x64
Действует **для всех функций**, включая внутренние.

| | Регистры |
|---|---|
| Аргументы 1–4 | `rcx`, `rdx`, `r8`, `r9` (дальше — стек) |
| Результат | `rax` |
| Изменяемые (volatile) | `rax`, `rcx`, `rdx`, `r8`–`r11`, `xmm0`–`xmm5` |
| Сохраняемые (non-volatile) | `rbx`, `rbp`, `rdi`, `rsi`, `r12`–`r15`, `xmm6`–`xmm15` |

* Перед `call` стек выровнен на 16 байт и выделено 32 байта shadow space.
* Функции оформляются макросом `proc` (`proc name uses rbx rsi, arg1, arg2`), вызовы — `fastcall` / `invoke` (для WinAPI). Макросы берут на себя пролог, shadow space и выравнивание.
* Листовые функции во внутренних горячих циклах (токенизатор, заливка прямоугольников) могут писаться без `proc`, но обязаны соблюдать сохранность non-volatile регистров. Такие функции помечаются комментарием `; leaf`.

## 3. Ошибки
* Функции, возвращающие указатель: `rax = 0` — ошибка.
* Функции-действия: `rax = 1` — успех, `rax = 0` — ошибка. Подробный код — в глобальной переменной `last_error` (константы `ERR_*`).
* Движок не завершает процесс при ошибке разметки или сети: любые входные данные должны приводить к отображаемому результату или странице ошибки.

## 4. Структуры
```asm
struct DOM_NODE
  Node_Type    dd ?
  Flags        dd ?
  Parent       dq ?
  First_Child  dq ?
  Last_Child   dq ?
  Next_Sibling dq ?
  Tag_ID       dd ?
  Data_Len     dd ?
  Data_Ptr     dq ?
  First_Attr   dq ?
  Line_Index   dd ?
  Order        dd ?
ends

  mov  rax, [rbx + DOM_NODE.Last_Child]
  mov  edx, sizeof.DOM_NODE
```
* Только именованные смещения, никаких числовых.
* После изменения структуры обновить `docs/MEMORY_MODEL.md` и проверить размер утверждением на этапе сборки:
  `if sizeof.DOM_NODE <> 0x48` / `display 'DOM_NODE size mismatch'` / `err` / `end if`.

## 5. Именование
| Сущность | Стиль | Пример |
|---|---|---|
| Функции | `модуль_действие` | `arena_alloc`, `html_tokenize`, `layout_block` |
| Функции HAL | `sys_модуль_действие` | `sys_mem_reserve`, `sys_net_connect` |
| Структуры | `UPPER_SNAKE` | `DOM_NODE`, `LINE` |
| Константы | `ПРЕФИКС_ИМЯ` | `TAG_DIV`, `ATTR_HREF`, `ERR_OOM` |
| Глобальные переменные | `lower_snake` | `fetch_pool`, `cur_doc` |
| Локальные метки | `.имя` | `.next_char` |

## 6. Правило слоёв
* `invoke` WinAPI разрешён **только** в `src/hal/`.
* `engine/` не зависит от `paint/` и `ui/`; `core/` не зависит от `engine/`.
* Единственный код вне главного потока — колбэк WinHTTP в `hal/win64/net.inc`: он пишет только поля `Bytes_Read`, `Win_Error`, `Secure_Flags` своего слота `FETCH` (и `State = Закрыт` через `lock xchg` при `HANDLE_CLOSING`) и вызывает `PostMessageW`. Данные движка (DOM, арены, раскладку) он не трогает.
* COM-вызовы WIC — через макросы `interface` / `comcall` из пакета FASM (вызов по таблице виртуальных методов); `CoInitializeEx(COINIT_APARTMENTTHREADED)` в главном потоке при первой картинке.
* Строки — срезы `ptr + len` (см. `MEMORY_MODEL.md`, раздел 2), UTF-8 внутри движка.

## 7. Безопасность кода
* **Без рекурсии** в обходах DOM, селекторов и раскладки — явные стеки с проверкой переполнения.
* Каждое чтение входных данных проверяет границу среза; числа из входа — с насыщением; произведения размеров проверяются до выделения памяти.
* Все пределы — именованные константы `LIM_*` в `consts.inc` (таблица — `ARCHITECTURE.md`, раздел 12).
* Новый разборщик входит в репозиторий вместе с фаззинг-тестом.
* Журнал сбоев — только через `SetUnhandledExceptionFilter`; код фильтра не выделяет память и не вызывает COM.

## 8. Комментарии
* Каждая функция начинается с шапки: назначение, аргументы, результат, изменяемые регистры (если отличаются от стандарта).
* В теле комментируется смысл, а не инструкция (`; пропускаем пробелы перед именем атрибута`, а не `; inc rsi`).
