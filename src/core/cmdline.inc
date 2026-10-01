; =============================================================================
; core/cmdline.inc — разбор командной строки в массив аргументов UTF-8.
; Правила как у CommandLineToArgvW: пробелы разделяют аргументы, кавычки
; группируют, 2n обратных слэшей перед кавычкой → n слэшей и кавычка-разделитель,
; 2n+1 → n слэшей и буквальная кавычка, "" внутри кавычек → буквальная кавычка.
; =============================================================================

ARGV_MAX = 32

uglobal
  argc       dd ?
  cmdline_error dd ?                ; 1 — командная строка слишком длинная или аргументов больше ARGV_MAX
  argv_ptr   rq ARGV_MAX
  argv_len   rd ARGV_MAX
  argv_buf   rb CMDLINE_MAX
endg

; -----------------------------------------------------------------------------
; cmdline_parse — заполнить argc / argv_ptr / argv_len.
; -----------------------------------------------------------------------------
proc cmdline_parse uses rbx rsi rdi r12 r13 r14
        mov     [cmdline_error], 0
        call    sys_cmdline_utf8
        test    edx, edx
        jnz     @f
        mov     [cmdline_error], 1      ; не поместилась в буфер (в строке всегда есть хотя бы имя exe)
  @@:
        mov     rsi, rax                ; вход (с нулём в конце)
        lea     rdi, [argv_buf]         ; выход: аргументы подряд
        lea     r14, [argv_ptr]         ; базы массивов (RIP-относительно, без абсолютных адресов)
        lea     r13, [argv_len]
        xor     ebx, ebx                ; число аргументов
  .skip_space:
        mov     al, [rsi]
        cmp     al, ' '
        je      .skip1
        cmp     al, 9
        je      .skip1
        test    al, al
        jz      .end
        cmp     ebx, ARGV_MAX
        jb      @f
        mov     [cmdline_error], 1      ; лишние аргументы не отбрасываются молча
        jmp     .end
  @@:
        ; начало нового аргумента
        mov     [r14 + rbx*8], rdi
        xor     r12d, r12d              ; внутри кавычек?
  .char:
        mov     al, [rsi]
        test    al, al
        jz      .arg_end
        cmp     al, '"'
        je      .quote
        cmp     al, '\'
        je      .backslash
        test    r12d, r12d
        jnz     .literal
        cmp     al, ' '
        je      .arg_end
        cmp     al, 9
        je      .arg_end
  .literal:
        mov     [rdi], al
        inc     rdi
        inc     rsi
        jmp     .char
  .quote:
        test    r12d, r12d
        jz      .toggle
        cmp     byte [rsi + 1], '"'
        jne     .toggle
        mov     byte [rdi], '"'         ; "" внутри кавычек
        inc     rdi
        add     rsi, 2
        jmp     .char
  .toggle:
        xor     r12d, 1
        inc     rsi
        jmp     .char
  .backslash:
        ; считаем подряд идущие обратные слэши
        xor     ecx, ecx
  .count:
        cmp     byte [rsi + rcx], '\'
        jne     .counted
        inc     ecx
        jmp     .count
  .counted:
        cmp     byte [rsi + rcx], '"'
        je      .bs_quote
        ; без кавычки после — слэши буквальные
        add     rsi, rcx
        mov     al, '\'
        rep     stosb
        jmp     .char
  .bs_quote:
        add     rsi, rcx                ; rsi → кавычка
        mov     edx, ecx
        shr     ecx, 1
        mov     al, '\'
        rep     stosb                   ; n слэшей
        test    edx, 1
        jz      .char                   ; чётное: кавычка обработается как разделитель
        mov     byte [rdi], '"'         ; нечётное: буквальная кавычка
        inc     rdi
        inc     rsi
        jmp     .char
  .arg_end:
        mov     rax, rdi
        sub     rax, [r14 + rbx*8]
        mov     [r13 + rbx*4], eax
        inc     ebx
        jmp     .skip_space
  .skip1:
        inc     rsi
        jmp     .skip_space
  .end:
        mov     [argc], ebx
        ret
endp

; -----------------------------------------------------------------------------
; cmdline_arg — срез аргумента по номеру. ecx = номер.
;   Результат: rax = указатель, edx = длина; rax = 0, если аргумента нет.
; -----------------------------------------------------------------------------
proc cmdline_arg
        cmp     ecx, [argc]
        jae     .none
        mov     ecx, ecx
        lea     rax, [argv_len]
        mov     edx, [rax + rcx*4]
        lea     rax, [argv_ptr]
        mov     rax, [rax + rcx*8]
        ret
  .none:
        xor     eax, eax
        xor     edx, edx
        ret
endp

; -----------------------------------------------------------------------------
; cmdline_is — eax = 1, если аргумент номер ecx равен срезу r8/r9d.
; -----------------------------------------------------------------------------
proc cmdline_is uses rbx rsi
        mov     rbx, r8
        mov     esi, r9d
        call    cmdline_arg
        test    rax, rax
        jz      .no
        mov     rcx, rax
        mov     r8, rbx
        mov     r9d, esi
        call    str_eq
        ret
  .no:
        xor     eax, eax
        ret
endp
