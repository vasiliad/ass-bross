; =============================================================================
; debug/dbg_print.inc — отладочный вывод: stdout + OutputDebugStringW.
; =============================================================================

uglobal
  dbg_numbuf  rb 32
endg

iglobal
  sdef s_newline, 10
endg

; -----------------------------------------------------------------------------
; dbg_print_str — вывести срез UTF-8. rcx = указатель, edx = длина.
; -----------------------------------------------------------------------------
proc dbg_print_str uses rbx rsi
        mov     rbx, rcx
        mov     esi, edx
        call    sys_write_out
        mov     rcx, rbx
        mov     edx, esi
        call    sys_debug_output
        ret
endp

; dbg_print_nl — перевод строки
proc dbg_print_nl
        slice   s_newline
        call    dbg_print_str
        ret
endp

; -----------------------------------------------------------------------------
; dbg_print_int — вывести знаковое десятичное число. rcx = число.
; -----------------------------------------------------------------------------
proc dbg_print_int uses rdi
        lea     rdi, [dbg_numbuf]
        mov     rax, rcx
        call    put_dec
        lea     rcx, [dbg_numbuf]
        mov     rdx, rdi
        sub     rdx, rcx
        call    dbg_print_str
        ret
endp

; -----------------------------------------------------------------------------
; dbg_print_hex — вывести число как 0x + 16 шестнадцатеричных цифр. rcx = число.
; -----------------------------------------------------------------------------
proc dbg_print_hex uses rdi
        lea     rdi, [dbg_numbuf]
        mov     word [rdi], '0x'
        add     rdi, 2
        mov     rax, rcx
        mov     ecx, 16
        call    put_hex
        lea     rcx, [dbg_numbuf]
        mov     edx, 18
        call    dbg_print_str
        ret
endp
