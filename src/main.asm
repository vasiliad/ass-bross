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

        slice   s_unknown
        call    sys_write_err
        jmp     .usage_err

  .cmdline_bad:
        slice   s_cmdline_bad
        call    sys_write_err
        mov     eax, EXIT_USAGE
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
endp

; --- Модули -------------------------------------------------------------------
include 'core/str.inc'
include 'hal/win64/os.inc'
include 'hal/win64/crash.inc'
include 'core/cmdline.inc'
include 'core/settings.inc'
include 'debug/dbg_print.inc'
include 'debug/bench.inc'

iglobal
  sdef s_version, 'NanoWeb ', NANOWEB_VERSION, 10
  sdef s_usage, \
      'Usage: nanoweb [options]', 10, \
      '  --version                     print version', 10, \
      '  --help                        this help', 10, \
      '  --get-setting <section> <key> print a value from nanoweb.ini', 10, \
      '  --bench noop [N]              measure an empty loop', 10, \
      '  --dump-args                   print parsed command-line arguments', 10, \
      '  --crash-test                  trigger a crash (crash log test)', 10
  sdef s_unknown,          'nanoweb: unknown option', 10
  sdef s_opt_version,      '--version'
  sdef s_opt_help,         '--help'
  sdef s_opt_crash,        '--crash-test'
  sdef s_opt_get_setting,  '--get-setting'
  sdef s_opt_bench,        '--bench'
  sdef s_opt_dump_args,    '--dump-args'
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

library kernel32, 'KERNEL32.DLL'

import kernel32, \
       AddVectoredExceptionHandler, 'AddVectoredExceptionHandler', \
       CloseHandle,                 'CloseHandle', \
       CreateDirectoryW,            'CreateDirectoryW', \
       CreateFileW,                 'CreateFileW', \
       GetCommandLineW,             'GetCommandLineW', \
       GetCurrentProcess,           'GetCurrentProcess', \
       GetEnvironmentVariableW,     'GetEnvironmentVariableW', \
       GetFileAttributesW,          'GetFileAttributesW', \
       GetModuleFileNameW,          'GetModuleFileNameW', \
       GetModuleHandleW,            'GetModuleHandleW', \
       GetStdHandle,                'GetStdHandle', \
       MultiByteToWideChar,         'MultiByteToWideChar', \
       OutputDebugStringW,          'OutputDebugStringW', \
       QueryPerformanceCounter,     'QueryPerformanceCounter', \
       QueryPerformanceFrequency,   'QueryPerformanceFrequency', \
       ReadFile,                    'ReadFile', \
       SetConsoleOutputCP,          'SetConsoleOutputCP', \
       SetThreadStackGuarantee,     'SetThreadStackGuarantee', \
       TerminateProcess,            'TerminateProcess', \
       WideCharToMultiByte,         'WideCharToMultiByte', \
       WriteFile,                   'WriteFile'

; =============================================================================
section '.rsrc' resource data readable

directory 24, manifests                 ; 24 = RT_MANIFEST

resource manifests, 1, 0, manifest

resdata manifest
  file 'res/nanoweb.manifest'
endres

; =============================================================================
section '.reloc' fixups data readable discardable
