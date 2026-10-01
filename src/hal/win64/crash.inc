; =============================================================================
; hal/win64/crash.inc — журнал сбоев (ARCHITECTURE.md, раздел 12).
; Фильтр необработанных исключений пишет код, адрес и регистры в stderr и в
; %APPDATA%\NanoWeb\crash.log. Код фильтра не выделяет память и не вызывает COM:
; всё строится в статическом буфере.
; =============================================================================

NW_EXCEPTION_EXECUTE_HANDLER = 1
CRASH_BUF_MAX = 2048

; Смещения в EXCEPTION_RECORD и CONTEXT (x64)
ER_CODE     = 0x00
ER_ADDRESS  = 0x10
CTX_RAX     = 0x78

uglobal
  crash_buf    rb CRASH_BUF_MAX
  crash_wpath  rw WPATH_MAX
endg

iglobal
  sdef s_crash_head, 'NanoWeb crash: code='
  sdef s_crash_addr, ' address='
  ; Регистры в порядке поля CONTEXT начиная с Rax: имя (4 байта) на каждый qword.
  crash_reg_names db 'rax rcx rdx rbx rsp rbp rsi rdi r8  r9  r10 r11 r12 r13 r14 r15 rip '
  CRASH_REG_COUNT = 17
  w_crash_log  du 'crash.log', 0
endg

; -----------------------------------------------------------------------------
; crash_install — установить фильтр необработанных исключений.
; -----------------------------------------------------------------------------
proc crash_install
        invoke  SetUnhandledExceptionFilter, crash_filter
        ret
endp

; -----------------------------------------------------------------------------
; crash_filter — вызывается системой. rcx = EXCEPTION_POINTERS*.
;   Результат: EXCEPTION_EXECUTE_HANDLER — процесс завершается с кодом исключения.
; -----------------------------------------------------------------------------
proc crash_filter uses rbx rsi rdi r12 r13
        mov     rbx, [rcx]              ; EXCEPTION_RECORD*
        mov     r12, [rcx + 8]          ; CONTEXT*
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
        mov     [rdi], al
        inc     rdi
        cmp     r13d, CRASH_REG_COUNT
        jb      .reg
        mov     byte [rdi], 10
        inc     rdi
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
        jz      .done
        lea     rcx, [crash_wpath]
        lea     rdx, [crash_buf]
        mov     r8d, r13d
        call    sys_file_append
  .done:
        mov     eax, NW_EXCEPTION_EXECUTE_HANDLER
        ret
endp
