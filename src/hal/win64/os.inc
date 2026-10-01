; =============================================================================
; hal/win64/os.inc — базовые обёртки над WinAPI: вывод, время, завершение,
; командная строка, файлы. Единственное место (вместе с остальным hal/), где
; разрешён invoke.
; =============================================================================

NW_CP_UTF8                 = 65001
NW_STD_OUTPUT_HANDLE       = -11
NW_STD_ERROR_HANDLE        = -12
NW_GENERIC_READ            = 0x80000000
NW_FILE_APPEND_DATA        = 0x0004
NW_FILE_SHARE_READ         = 0x0001
NW_FILE_SHARE_WRITE        = 0x0002
NW_OPEN_EXISTING           = 3
NW_OPEN_ALWAYS             = 4
NW_FILE_ATTRIBUTE_NORMAL   = 0x80
NW_INVALID_HANDLE          = -1
NW_INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF

CMDLINE_MAX   = 8192                ; байт UTF-8
WPATH_MAX     = 1024                ; символов UTF-16 в пути
DBG_WBUF_MAX  = 1024                ; символов UTF-16 для OutputDebugStringW

uglobal
  h_stdout      dq ?
  h_stderr      dq ?
  cmdline_utf8  rb CMDLINE_MAX
  dbg_wbuf      rw DBG_WBUF_MAX
endg

; -----------------------------------------------------------------------------
; sys_init — подготовка вывода: дескрипторы stdout/stderr, UTF-8 в консоли.
; -----------------------------------------------------------------------------
proc sys_init
        invoke  GetStdHandle, NW_STD_OUTPUT_HANDLE
        mov     [h_stdout], rax
        invoke  GetStdHandle, NW_STD_ERROR_HANDLE
        mov     [h_stderr], rax
        ; без консоли (GUI, вывод в канал) вызов просто вернёт ошибку
        invoke  SetConsoleOutputCP, NW_CP_UTF8
        ret
endp

; -----------------------------------------------------------------------------
; sys_write_handle — записать байты в дескриптор.
;   rcx = HANDLE, rdx = указатель, r8d = длина
; -----------------------------------------------------------------------------
proc sys_write_handle uses rbx rsi rdi
  locals
    written dd ?
  endl
        mov     rbx, rcx
        mov     rsi, rdx
        mov     edi, r8d
        test    rbx, rbx
        jz      .done                   ; нет дескриптора (GUI без перенаправления)
        cmp     rbx, NW_INVALID_HANDLE
        je      .done
  .next:
        test    edi, edi
        jz      .done
        invoke  WriteFile, rbx, rsi, rdi, addr written, 0
        test    eax, eax
        jz      .done
        mov     eax, [written]
        test    eax, eax
        jz      .done
        add     rsi, rax
        sub     edi, eax
        jmp     .next
  .done:
        ret
endp

; sys_write_out / sys_write_err — rcx = указатель, edx = длина
proc sys_write_out
        mov     r8d, edx
        mov     rdx, rcx
        mov     rcx, [h_stdout]
        call    sys_write_handle
        ret
endp

proc sys_write_err
        mov     r8d, edx
        mov     rdx, rcx
        mov     rcx, [h_stderr]
        call    sys_write_handle
        ret
endp

; -----------------------------------------------------------------------------
; sys_debug_output — строка UTF-8 в OutputDebugStringW (усекается до 1023 символов).
;   rcx = указатель, edx = длина
; -----------------------------------------------------------------------------
proc sys_debug_output
        cmp     edx, DBG_WBUF_MAX - 1
        jbe     @f
        mov     edx, DBG_WBUF_MAX - 1
  @@:
        invoke  MultiByteToWideChar, NW_CP_UTF8, 0, rcx, rdx, dbg_wbuf, DBG_WBUF_MAX - 1
        lea     rcx, [dbg_wbuf]
        mov     word [rcx + rax*2], 0
        invoke  OutputDebugStringW, rcx
        ret
endp

; -----------------------------------------------------------------------------
; sys_exit — немедленное завершение процесса (решение 17: TerminateProcess,
; без выгрузки DLL). ecx = код завершения. Не возвращается.
; -----------------------------------------------------------------------------
proc sys_exit uses rbx
        mov     ebx, ecx
        invoke  GetCurrentProcess
        invoke  TerminateProcess, rax, rbx
  @@:   jmp     @b
endp

; -----------------------------------------------------------------------------
; sys_qpc / sys_qpf — счётчик и частота QueryPerformanceCounter. Результат в rax.
; -----------------------------------------------------------------------------
proc sys_qpc
  locals
    value dq ?
  endl
        invoke  QueryPerformanceCounter, addr value
        mov     rax, [value]
        ret
endp

proc sys_qpf
  locals
    value dq ?
  endl
        invoke  QueryPerformanceFrequency, addr value
        mov     rax, [value]
        ret
endp

; -----------------------------------------------------------------------------
; sys_cmdline_utf8 — командная строка процесса в UTF-8 с нулём в конце.
;   Результат: rax = указатель (статический буфер), edx = длина без нуля.
; -----------------------------------------------------------------------------
proc sys_cmdline_utf8
        invoke  GetCommandLineW
        invoke  WideCharToMultiByte, NW_CP_UTF8, 0, rax, -1, cmdline_utf8, CMDLINE_MAX - 1, 0, 0
        lea     rcx, [cmdline_utf8]
        test    eax, eax
        jz      .empty
        lea     edx, [eax - 1]          ; WideCharToMultiByte считает и завершающий ноль
        mov     byte [rcx + rdx], 0
        mov     rax, rcx
        ret
  .empty:
        mov     byte [rcx], 0
        mov     rax, rcx
        xor     edx, edx
        ret
endp

; -----------------------------------------------------------------------------
; sys_file_read — прочитать файл целиком в буфер.
;   rcx = путь (UTF-16, с нулём), rdx = буфер, r8d = ёмкость
;   Результат: rax = прочитано байт или -1, если файл не открылся.
; -----------------------------------------------------------------------------
proc sys_file_read uses rbx rsi rdi r12
  locals
    got dd ?
  endl
        mov     rsi, rdx
        mov     edi, r8d
        invoke  CreateFileW, rcx, NW_GENERIC_READ, NW_FILE_SHARE_READ, 0, NW_OPEN_EXISTING, NW_FILE_ATTRIBUTE_NORMAL, 0
        cmp     rax, NW_INVALID_HANDLE
        je      .fail
        mov     rbx, rax
        xor     r12d, r12d              ; всего прочитано
  .next:
        mov     eax, edi
        sub     eax, r12d
        jz      .close                  ; буфер заполнен — остаток файла игнорируется
        lea     rdx, [rsi + r12]
        invoke  ReadFile, rbx, rdx, rax, addr got, 0
        test    eax, eax
        jz      .close
        mov     eax, [got]
        test    eax, eax
        jz      .close
        add     r12d, eax
        jmp     .next
  .close:
        invoke  CloseHandle, rbx
        mov     eax, r12d
        ret
  .fail:
        mov     rax, -1
        ret
endp

; -----------------------------------------------------------------------------
; sys_file_append — дописать байты в конец файла (создаётся при отсутствии).
;   rcx = путь (UTF-16), rdx = данные, r8d = длина. Результат: eax = 1 / 0.
; -----------------------------------------------------------------------------
proc sys_file_append uses rbx rsi rdi
        mov     rsi, rdx
        mov     edi, r8d
        invoke  CreateFileW, rcx, NW_FILE_APPEND_DATA, NW_FILE_SHARE_READ, 0, NW_OPEN_ALWAYS, NW_FILE_ATTRIBUTE_NORMAL, 0
        cmp     rax, NW_INVALID_HANDLE
        je      .fail
        mov     rbx, rax
        mov     rcx, rax
        mov     rdx, rsi
        mov     r8d, edi
        call    sys_write_handle
        invoke  CloseHandle, rbx
        mov     eax, 1
        ret
  .fail:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; sys_file_exists — eax = 1, если путь (UTF-16) существует.
; -----------------------------------------------------------------------------
proc sys_file_exists
        invoke  GetFileAttributesW, rcx
        cmp     eax, NW_INVALID_FILE_ATTRIBUTES
        sete    al
        xor     al, 1
        movzx   eax, al
        ret
endp

; -----------------------------------------------------------------------------
; wstr_copy — скопировать строку UTF-16 с нулём (leaf).
;   rdi = куда, rsi = откуда. После: rdi указывает на завершающий ноль.
;   Портит: ax, rsi, rdi.
; -----------------------------------------------------------------------------
wstr_copy:                              ; leaf
        lodsw
        stosw
        test    ax, ax
        jnz     wstr_copy
        sub     rdi, 2
        ret

; -----------------------------------------------------------------------------
; sys_exe_dir_path — путь «каталог exe + имя».
;   rcx = буфер (WPATH_MAX символов), rdx = имя файла (UTF-16 с нулём)
;   Результат: eax = 1 / 0 (путь слишком длинный).
; -----------------------------------------------------------------------------
proc sys_exe_dir_path uses rbx rsi rdi
        mov     rbx, rcx
        mov     rsi, rdx
        invoke  GetModuleFileNameW, 0, rbx, WPATH_MAX - 64
        test    eax, eax
        jz      .fail
        cmp     eax, WPATH_MAX - 64
        jae     .fail
        ; ищем последний обратный слэш
        lea     rdi, [rbx + rax*2]
  .back:
        cmp     rdi, rbx
        je      .fail
        sub     rdi, 2
        cmp     word [rdi], '\'
        jne     .back
        add     rdi, 2
        call    wstr_copy
        mov     eax, 1
        ret
  .fail:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; sys_appdata_path — путь «%APPDATA%\NanoWeb\имя»; каталог NanoWeb создаётся.
;   rcx = буфер (WPATH_MAX символов), rdx = имя файла (UTF-16 с нулём)
;   Результат: eax = 1 / 0.
; -----------------------------------------------------------------------------
proc sys_appdata_path uses rbx rsi rdi
        mov     rbx, rcx
        mov     rdi, rdx
        invoke  GetEnvironmentVariableW, w_appdata, rbx, WPATH_MAX - 64
        test    eax, eax
        jz      .fail
        cmp     eax, WPATH_MAX - 64
        jae     .fail
        mov     rdx, rdi                ; имя файла
        lea     rdi, [rbx + rax*2]
        lea     rsi, [w_nanoweb_dir]
        push    rdx
        push    rdx                     ; пара push сохраняет выравнивание стека
        call    wstr_copy
        ; ошибка «каталог уже существует» не важна
        invoke  CreateDirectoryW, rbx, 0
        pop     rsi
        pop     rsi                     ; rsi = имя файла (rdi сохранён вызываемой функцией)
        mov     word [rdi], '\'
        add     rdi, 2
        call    wstr_copy
        mov     eax, 1
        ret
  .fail:
        xor     eax, eax
        ret
endp

iglobal
  w_appdata      du 'APPDATA', 0
  w_nanoweb_dir  du '\NanoWeb', 0
endg
