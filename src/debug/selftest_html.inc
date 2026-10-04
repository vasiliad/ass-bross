; =============================================================================
; debug/selftest_html.inc - самопроверка html_tokenize
; =============================================================================
proc selftest_html uses rbx rsi rdi
        mov     esi, 1
        
        lea     rcx, [st_arena]
        mov     rdx, 1 shl 20
        mov     r8d, ARENA_STEP_DOC
        call    arena_init
        test    eax, eax
        jz      .fail
        
        lea     rcx, [s_test_html]
        mov     rdx, s_test_html.len
        lea     r8, [st_arena]
        call    html_tokenize
        test    rax, rax
        jz      .fail
        
        ; Проверим первый токен: TAG_OPEN, 'html'
        mov     rbx, rax
        cmp     dword [rbx + HTML_TOKEN.Type], HTML_TOK_TAG_OPEN
        jne     .fail
        cmp     dword [rbx + HTML_TOKEN.Len], 4
        jne     .fail
        
        ; Второй токен: TEXT, ' '
        add     rbx, sizeof.HTML_TOKEN
        cmp     dword [rbx + HTML_TOKEN.Type], HTML_TOK_TEXT
        jne     .fail
        
        slice   s_st_arena_ok
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
