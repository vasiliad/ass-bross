; =============================================================================
; NanoWeb — сверхлёгкий браузер для чтения. Точка входа и сборка модулей.
;
; Сборка (одна единица трансляции, без линкера):
;   fasm -d CONSOLE=0 src/main.asm build/nanoweb.exe       ; релиз, подсистема GUI
;   fasm -d CONSOLE=1 src/main.asm build/nanoweb-con.exe   ; отладка, подсистема console
; После сборки tools/patch_pe.py выставляет флаги ASLR (ARCHITECTURE.md, раздел 12).
; =============================================================================

; База 0x140000000 — стандартная для 64-битных EXE: выше 4 ГБ, совместима с high-entropy ASLR.
match =1, CONSOLE { format PE64 NX console 6.0 at 0x140000000 }
match =0, CONSOLE { format PE64 NX GUI 6.0 at 0x140000000 }

entry start
stack 0x100000, 0x10000

include 'win64a.inc'
include 'include/macros.inc'
include 'include/structs.inc'
include 'include/consts.inc'
include 'include/proc.inc'

; =============================================================================
section '.text' code readable executable

start:
        sub     rsp, 8                  ; выравнивание стека на 16 для вызовов
        call    crash_install
        call    sys_init
        call    cmdline_parse
        call    main
        mov     ecx, eax
        call    sys_exit

; -----------------------------------------------------------------------------
; main — разбор режима запуска. Результат: eax = код завершения.
; -----------------------------------------------------------------------------
proc main uses rbx
        cmp     [cmdline_error], 0
        jne     .cmdline_bad
        cmp     [argc], 2
        jb      .usage_ok

        mov     ecx, 1
        lea     r8, [s_opt_version]
        mov     r9d, s_opt_version.len
        call    cmdline_is
        test    eax, eax
        jnz     .version

        mov     ecx, 1
        lea     r8, [s_opt_help]
        mov     r9d, s_opt_help.len
        call    cmdline_is
        test    eax, eax
        jnz     .usage_ok

        mov     ecx, 1
        lea     r8, [s_opt_crash]
        mov     r9d, s_opt_crash.len
        call    cmdline_is
        test    eax, eax
        jnz     .crash

        mov     ecx, 1
        lea     r8, [s_opt_get_setting]
        mov     r9d, s_opt_get_setting.len
        call    cmdline_is
        test    eax, eax
        jnz     .get_setting

        mov     ecx, 1
        lea     r8, [s_opt_dump_args]
        mov     r9d, s_opt_dump_args.len
        call    cmdline_is
        test    eax, eax
        jnz     .dump_args

        mov     ecx, 1
        lea     r8, [s_opt_bench]
        mov     r9d, s_opt_bench.len
        call    cmdline_is
        test    eax, eax
        jnz     .bench

        mov     ecx, 1
        lea     r8, [s_opt_selftest]
        mov     r9d, s_opt_selftest.len
        call    cmdline_is
        test    eax, eax
        jnz     .selftest

        slice   s_unknown
        call    sys_write_err
        jmp     .usage_err

  .cmdline_bad:
        slice   s_cmdline_bad
        call    sys_write_err
        mov     eax, EXIT_USAGE
        ret

        lea     r8, [s_opt_dump_tokens]
        mov     r9d, s_opt_dump_tokens.len
        call    cmdline_is
        test    eax, eax
        jnz     .dump_tokens

        lea     r8, [s_opt_dump_dom]
        mov     r9d, s_opt_dump_dom.len
        call    cmdline_is
        test    eax, eax
        jnz     .dump_dom


        lea     r8, [s_opt_extract]
        mov     r9d, s_opt_extract.len
        call    cmdline_is
        test    eax, eax
        jnz     .cmd_extract

  .cmd_extract:
        cmp     [argc], 6
        jne     .usage_err
        
        ; 2 = url, 3 = attr name, 4 = attr val, 5 = out file
        mov     ecx, 2
        call    cmdline_arg
        call    cmd_extract
        jmp     .dump_done

  .dump_dom:
        cmp     [argc], 3
        jne     .usage_err
        mov     ecx, 2
        call    cmdline_arg
        mov     rcx, rax
        call    cmd_dump_dom
        ret

  .dump_tokens:
        cmp     [argc], 3
        jne     .usage_err
        mov     ecx, 2
        call    cmdline_arg
        mov     rcx, rax
        call    cmd_dump_tokens
        ret

  .dump_args:
        ; --dump-args: argc и все аргументы, по одному в строке «[i] текст»
        slice   s_argc
        call    dbg_print_str
        mov     ecx, [argc]
        call    dbg_print_int
        call    dbg_print_nl
        xor     ebx, ebx
  .dump_next:
        cmp     ebx, [argc]
        jae     .dump_done
        slice   s_lbracket
        call    dbg_print_str
        mov     ecx, ebx
        call    dbg_print_int
        slice   s_rbracket
        call    dbg_print_str
        mov     ecx, ebx
        call    cmdline_arg
        mov     rcx, rax
        call    dbg_print_str
        call    dbg_print_nl
        inc     ebx
        jmp     .dump_next
  .dump_done:
        mov     eax, EXIT_OK
        ret

  .version:
        slice   s_version
        call    dbg_print_str
        mov     eax, EXIT_OK
        ret

  .usage_ok:
        ; Показывать GUI только если запустили без аргументов (argc == 1)
        cmp     [argc], 1
        jne     .print_usage
        match =0, CONSOLE {
            call    ui_show
            mov     eax, EXIT_OK
            ret
        }
  .print_usage:
        slice   s_usage
        call    dbg_print_str
        mov     eax, EXIT_OK
        ret

  .usage_err:
        slice   s_usage
        call    sys_write_err
        mov     eax, EXIT_USAGE
        ret

  .crash:
        ; намеренное обращение по нулевому адресу — проверка журнала сбоев
        xor     eax, eax
        mov     [rax], eax
        mov     eax, EXIT_ERROR
        ret

  .get_setting:
        ; --get-setting <секция> <ключ>
        cmp     [argc], 4
        jne     .usage_err
        call    settings_load
        mov     ecx, 3
        call    cmdline_arg
        mov     rbx, rax
        mov     r9d, edx                ; ключ
        push    r9
        push    rbx
        mov     ecx, 2
        call    cmdline_arg
        pop     r8
        pop     r9
        mov     rcx, rax
        call    settings_get            ; edx = длина значения
        test    rax, rax
        jz      .not_found
        mov     rcx, rax
        call    dbg_print_str
        call    dbg_print_nl
        mov     eax, EXIT_OK
        ret
  .not_found:
        mov     eax, EXIT_NOT_FOUND
        ret

  .bench:
        ; --bench noop [N]
        mov     ecx, 2
        lea     r8, [s_bench_name_noop]
        mov     r9d, s_bench_name_noop.len
        call    cmdline_is
        test    eax, eax
        jz      .usage_err
        mov     ebx, 1000000            ; итераций по умолчанию
        cmp     [argc], 4
        jb      .run_bench
        mov     ecx, 3
        call    cmdline_arg
        mov     rcx, rax
        call    parse_u32
        test    edx, edx
        jz      .usage_err
        mov     ebx, eax
  .run_bench:
        mov     ecx, ebx
        call    bench_noop
        mov     eax, EXIT_OK
        ret

  .selftest:
        cmp     [argc], 3
        jne     .usage_err
        
        mov     ecx, 2
        lea     r8, [s_st_name_arena]
        mov     r9d, s_st_name_arena.len
        call    cmdline_is
        test    eax, eax
        jnz     .st_arena
        
        mov     ecx, 2
        lea     r8, [s_st_name_file]
        mov     r9d, s_st_name_file.len
        call    cmdline_is
        test    eax, eax
        jnz     .st_file
        
        mov     ecx, 2
        lea     r8, [s_st_name_html]
        mov     r9d, s_st_name_html.len
        call    cmdline_is
        test    eax, eax
        jnz     .st_html
        
        jmp     .usage_err

  .st_arena:
        call    selftest_arena
        jmp     .st_ret
  .st_file:
        call    selftest_file
        jmp     .st_ret
  .st_html:
        call    selftest_html
  .st_ret:
        test    eax, eax
        mov     eax, EXIT_OK
        mov     ecx, EXIT_ERROR
        cmovz   eax, ecx
        ret
endp

; --- Модули -------------------------------------------------------------------
include 'core/str.inc'
include 'hal/win64/os.inc'
include 'hal/win64/crash.inc'
include 'core/cmdline.inc'
include 'core/settings.inc'
include 'debug/dbg_print.inc'
include 'hal/win64/mem.inc'
include 'core/arena.inc'
include 'core/file.inc'
include 'core/decode.inc'
include 'net/http.inc'
include 'engine/html.inc'
include 'engine/dom.inc'
include 'engine/extract.inc'
include 'engine/extract_cli.inc'
include 'ui.inc'
include 'engine/dump.inc'
include 'debug/bench.inc'
include 'debug/selftest.inc'
include 'debug/selftest_file.inc'
include 'debug/selftest_html.inc'

iglobal
  ; Код адресуется только RIP-относительно, и перемещений в нём нет. Но Windows не
  ; загружает образ с пустой секцией .reloc, а без неё невозможен ASLR. Поэтому одно
  ; перемещение заведено намеренно (проверяется tools/check_pe.py).
  align 8
  reloc_anchor dq start
  sdef s_version, 'NanoWeb ', NANOWEB_VERSION, 10
  sdef s_usage, \
      'Usage: nanoweb [options]', 10, \
      '  --version                     print version', 10, \
      '  --help                        this help', 10, \
      '  --get-setting <section> <key> print a value from nanoweb.ini', 10, \
      '  --bench noop [N]              measure an empty loop', 10, \
      '  --dump-args                   print parsed command-line arguments', 10, \
      '  --crash-test                  trigger a crash (crash log test)', 10, \
      '  --selftest arena              run built-in arena self-test', 10
  sdef s_unknown,          'nanoweb: unknown option', 10
  sdef s_opt_version,      '--version'
  sdef s_opt_help,         '--help'
  sdef s_opt_crash,        '--crash-test'
  sdef s_opt_get_setting,  '--get-setting'
  sdef s_opt_bench,        '--bench'
  sdef s_opt_dump_args,    '--dump-args'
  sdef s_opt_selftest,     '--selftest'
  sdef s_st_name_arena,    'arena'
  sdef s_st_name_file,      'file'
  sdef s_st_name_html,      'html'
  sdef s_opt_dump_tokens,    '--dump-tokens'
  sdef s_opt_dump_dom,       '--dump-dom'
  sdef s_opt_extract,        '--extract'
  sdef s_test_html,         '<html> <body>Hello</body></html>'
  sdef s_cmdline_bad,      'nanoweb: command line too long or too many arguments', 10
  sdef s_argc,             'argc='
  sdef s_lbracket,         '['
  sdef s_rbracket,         '] '
  sdef s_bench_name_noop,  'noop'
endg

; =============================================================================
section '.data' data readable writeable

IncludeIGlobals
align 16
IncludeUGlobals

; =============================================================================
section '.idata' import data readable writeable

library kernel32, 'KERNEL32.DLL', \
        wininet,  'WININET.DLL', \
        user32,   'USER32.DLL', \
        shell32,  'SHELL32.DLL'

import kernel32, \
       AddVectoredExceptionHandler, 'AddVectoredExceptionHandler', \
       CloseHandle,                 'CloseHandle', \
       CreateDirectoryW,            'CreateDirectoryW', \
       CreateFileW,                 'CreateFileW', \
       GetCommandLineW,             'GetCommandLineW', \
       GetCurrentProcess,           'GetCurrentProcess', \
       GetEnvironmentVariableW,     'GetEnvironmentVariableW', \
       GetFileAttributesW,          'GetFileAttributesW', \
       GetFileSizeEx,               'GetFileSizeEx', \
       GetModuleFileNameW,          'GetModuleFileNameW', \
       GetModuleHandleW,            'GetModuleHandleW', \
       GetStdHandle,                'GetStdHandle', \
       MultiByteToWideChar,         'MultiByteToWideChar', \
       OutputDebugStringW,          'OutputDebugStringW', \
       OutputDebugStringA,          'OutputDebugStringA', \
       QueryPerformanceCounter,     'QueryPerformanceCounter', \
       QueryPerformanceFrequency,   'QueryPerformanceFrequency', \
       ReadFile,                    'ReadFile', \
       SetConsoleOutputCP,          'SetConsoleOutputCP', \
       SetThreadStackGuarantee,     'SetThreadStackGuarantee', \
       TerminateProcess,            'TerminateProcess', \
       VirtualAlloc,                'VirtualAlloc', \
       VirtualFree,                 'VirtualFree', \
       WideCharToMultiByte,         'WideCharToMultiByte', \
       WriteFile,                   'WriteFile'

import wininet, \
       InternetOpenA,               'InternetOpenA', \
       InternetOpenUrlA,            'InternetOpenUrlA', \
       InternetReadFile,            'InternetReadFile', \
       InternetCloseHandle,         'InternetCloseHandle'

import user32, \
       CreateWindowExA,             'CreateWindowExA', \
       DefWindowProcA,              'DefWindowProcA', \
       DispatchMessageA,            'DispatchMessageA', \
       GetMessageA,                 'GetMessageA', \
       LoadCursorA,                 'LoadCursorA', \
       PostQuitMessage,             'PostQuitMessage', \
       RegisterClassExA,            'RegisterClassExA', \
       SendMessageA,                'SendMessageA', \
       TranslateMessage,            'TranslateMessage'

import shell32, \
       SHBrowseForFolderA,          'SHBrowseForFolderA', \
       SHGetPathFromIDListA,        'SHGetPathFromIDListA'

; =============================================================================
section '.rsrc' resource data readable

directory 24, manifests                 ; 24 = RT_MANIFEST

resource manifests, 1, 0, manifest

resdata manifest
  file 'res/nanoweb.manifest'
endres

; =============================================================================
section '.pdata' data readable

data 3
  NwEmitPData
end data

; =============================================================================
section '.reloc' fixups data readable discardable
