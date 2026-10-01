; =============================================================================
; hal/win64/crash.inc — журнал сбоев (ARCHITECTURE.md, раздел 12).
;
; Почему векторный обработчик, а не SetUnhandledExceptionFilter: на x64 система
; доходит до фильтра необработанных исключений, только раскрутив стек по таблице
; .pdata, а FASM её не создаёт — без неё раскрутка ломается на первом же кадре
; и фильтр не вызывается (проверено в CI на Windows). Векторный обработчик
; вызывается до раскрутки. Чтобы не реагировать на исключения, которые системные
; DLL штатно обрабатывают сами, учитываются только фатальные коды и только
; адреса внутри образа nanoweb.exe.
;
; Отчёт: код, адрес и регистры — в stderr и в %APPDATA%\NanoWeb\crash.log.
; Код обработчика не выделяет память и не вызывает COM; затем процесс
; завершается с кодом исключения.
; =============================================================================

NW_EXCEPTION_CONTINUE_SEARCH = 0
CRASH_BUF_MAX       = 2048
CRASH_STACK_RESERVE = 0x8000            ; запас стека для обработчика при переполнении стека

; Смещения в EXCEPTION_RECORD и CONTEXT (x64), в заголовке PE
ER_CODE           = 0x00
ER_ADDRESS        = 0x10
CTX_RAX           = 0x78
PE_LFANEW         = 0x3C
PE_SIZE_OF_IMAGE  = 24 + 56

uglobal
  crash_buf     rb CRASH_BUF_MAX
  crash_wpath   rw WPATH_MAX
  image_base    dq ?
  image_end     dq ?
  crash_active  dd ?
  crash_guard   dd ?
endg

iglobal
  sdef s_crash_head, 'NanoWeb crash: code='
  sdef s_crash_addr, ' address='
  ; Регистры в порядке полей CONTEXT начиная с Rax: имя (4 байта) на каждый qword.
  crash_reg_names db 'rax rcx rdx rbx rsp rbp rsi rdi r8  r9  r10 r11 r12 r13 r14 r15 rip '
  CRASH_REG_COUNT = 17
  ; Фатальные коды исключений
  crash_codes dd 0xC0000005, \          ; ACCESS_VIOLATION
                 0xC000001D, \          ; ILLEGAL_INSTRUCTION
                 0xC0000096, \          ; PRIV_INSTRUCTION
                 0xC0000094, \          ; INT_DIVIDE_BY_ZERO
                 0xC0000095, \          ; INT_OVERFLOW
                 0xC00000FD, \          ; STACK_OVERFLOW
                 0xC0000409, \          ; STACK_BUFFER_OVERRUN
                 0x80000002             ; DATATYPE_MISALIGNMENT
  CRASH_CODE_COUNT = 8
  w_crash_log  du 'crash.log', 0
endg

; -----------------------------------------------------------------------------
; crash_install — запомнить границы образа, зарезервировать стек для обработчика
; и установить векторный обработчик первым в цепочке.
; -----------------------------------------------------------------------------
proc crash_install
        invoke  GetModuleHandleW, 0
        mov     [image_base], rax
        mov     ecx, [rax + PE_LFANEW]
        mov     ecx, [rax + rcx + PE_SIZE_OF_IMAGE]
        add     rax, rcx
        mov     [image_end], rax
        mov     [crash_guard], CRASH_STACK_RESERVE
        invoke  SetThreadStackGuarantee, crash_guard
        invoke  AddVectoredExceptionHandler, 1, crash_handler
        ret
endp

; -----------------------------------------------------------------------------
; crash_handler — вызывается системой до раскрутки стека. rcx = EXCEPTION_POINTERS*.
;   Нефатальные исключения и исключения вне образа пропускаются (CONTINUE_SEARCH).
; -----------------------------------------------------------------------------
proc crash_handler uses rbx rsi rdi r12 r13
        mov     rbx, [rcx]              ; EXCEPTION_RECORD*
        mov     r12, [rcx + 8]          ; CONTEXT*

        ; адрес внутри nanoweb.exe?
        mov     rax, [rbx + ER_ADDRESS]
        cmp     rax, [image_base]
        jb      .pass
        cmp     rax, [image_end]
        jae     .pass
        ; фатальный код?
        mov     eax, [rbx + ER_CODE]
        lea     rsi, [crash_codes]
        mov     ecx, CRASH_CODE_COUNT
  .code:
        cmp     eax, [rsi]
        je      .fatal
        add     rsi, 4
        dec     ecx
        jnz     .code
  .pass:
        mov     eax, NW_EXCEPTION_CONTINUE_SEARCH
        ret

  .fatal:
        ; повторный сбой внутри самого обработчика — завершиться без отчёта
        mov     eax, 1
        xchg    eax, [crash_active]
        test    eax, eax
        jnz     .terminate

        lea     rdi, [crash_buf]
        lea     rsi, [s_crash_head]
        mov     ecx, s_crash_head.len
        call    put_bytes
        mov     eax, [rbx + ER_CODE]
        mov     ecx, 8
        call    put_hex
        lea     rsi, [s_crash_addr]
        mov     ecx, s_crash_addr.len
        call    put_bytes
        mov     rax, [rbx + ER_ADDRESS]
        mov     ecx, 16
        call    put_hex
        mov     byte [rdi], 10
        inc     rdi

        ; регистры: по 4 в строке
        xor     r13d, r13d
  .reg:
        lea     rsi, [crash_reg_names]
        lea     rsi, [rsi + r13*4]
        mov     ecx, 4
        call    put_bytes
  .trim:
        cmp     byte [rdi - 1], ' '     ; имена дополнены пробелами до 4 символов
        jne     .trimmed
        dec     rdi
        jmp     .trim
  .trimmed:
        mov     byte [rdi], '='
        inc     rdi
        mov     rax, [r12 + CTX_RAX + r13*8]
        mov     ecx, 16
        call    put_hex
        inc     r13d
        mov     al, ' '
        test    r13d, 3
        jnz     @f
        mov     al, 10
  @@:
        cmp     r13d, CRASH_REG_COUNT
        jne     @f
        mov     al, 10                  ; последняя строка
  @@:
        mov     [rdi], al
        inc     rdi
        cmp     r13d, CRASH_REG_COUNT
        jb      .reg
        ; TODO(M4): URL текущей страницы из cur_doc

        lea     rsi, [crash_buf]
        sub     rdi, rsi
        mov     r13d, edi               ; длина отчёта

        mov     rcx, rsi
        mov     edx, r13d
        call    sys_write_err

        lea     rcx, [crash_wpath]
        lea     rdx, [w_crash_log]
        call    sys_appdata_path
        test    eax, eax
        jz      .terminate
        lea     rcx, [crash_wpath]
        lea     rdx, [crash_buf]
        mov     r8d, r13d
        call    sys_file_append
  .terminate:
        mov     ecx, [rbx + ER_CODE]
        call    sys_exit                ; не возвращается
        ret
endp
