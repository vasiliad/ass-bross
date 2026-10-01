; =============================================================================
; debug/bench.inc — режим --bench (ARCHITECTURE.md, раздел 11).
; M0: только «noop» — проверка стенда замеров. Этапы parse/vis/layout/find
; добавляются вместе с соответствующими модулями.
; Формат вывода (разбирает tools/bench.py):
;   bench <имя> iterations=<N> ns=<всего наносекунд>
; =============================================================================

iglobal
  sdef s_bench_noop,  'bench noop iterations='
  sdef s_bench_ns,    ' ns='
endg

; -----------------------------------------------------------------------------
; bench_noop — пустой цикл из ecx итераций с замером QueryPerformanceCounter.
; -----------------------------------------------------------------------------
proc bench_noop uses rbx rsi rdi
        mov     ebx, ecx
        call    sys_qpc
        mov     rsi, rax
        mov     ecx, ebx
        test    ecx, ecx
        jz      .measured
  .loop:
        dec     ecx
        jnz     .loop
  .measured:
        call    sys_qpc
        sub     rax, rsi
        mov     rdi, rax                ; тики
        call    sys_qpf
        mov     rcx, rax
        mov     rax, rdi
        mov     rdx, 1000000000
        mul     rdx                     ; rdx:rax = тики × 10^9
        div     rcx                     ; rax = наносекунды
        mov     rdi, rax

        slice   s_bench_noop
        call    dbg_print_str
        mov     ecx, ebx
        call    dbg_print_int
        slice   s_bench_ns
        call    dbg_print_str
        mov     rcx, rdi
        call    dbg_print_int
        call    dbg_print_nl
        ret
endp
