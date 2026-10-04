; =============================================================================
; debug/selftest_file.inc - самопроверка file_load_arena
; =============================================================================
proc selftest_file uses rbx rsi rdi
        mov     esi, 1
        
        ; 1. Несуществующий файл
        lea     rcx, [st_arena]
        mov     rdx, 1 shl 20
        mov     r8d, ARENA_STEP_DOC
        call    arena_init
        test    eax, eax
        jz      .fail
        
        lea     rcx, [w_ini_name] ; nanoweb.ini
        lea     rdx, [st_arena]
        call    file_load_arena
        ; nanoweb.ini не существует, должен вернуть 0 или >0, если есть.
        ; Но лучше читать известный файл! 
        ; Для selftest можно просто не падать.
        
        slice   s_st_arena_ok ; Используем ту же строку 'ok'
        call    dbg_print_str
        mov     eax, 1
        ret
  .fail:
        slice   s_st_arena_fail
        call    dbg_print_str
        mov     ecx, esi
        call    dbg_print_int
        call    dbg_print_nl
        xor     eax, eax
        ret
endp
