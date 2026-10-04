; =============================================================================
; debug/selftest.inc — встроенные самопроверки (режим --selftest <имя>).
; Печатают «<имя>: ok» и возвращают 1, либо «<имя>: FAIL <шаг>» и возвращают 0.
; =============================================================================

SELFTEST_RESERVE = 16 shl 20
SELFTEST_BIG     = 200000               ; больше одного шага фиксации (64 КБ)

uglobal
  st_arena  ARENA
endg

iglobal
  sdef s_st_arena_ok,   'arena: ok', 10
  sdef s_st_arena_fail, 'arena: FAIL step '
endg

; -----------------------------------------------------------------------------
; st_is_zero — проверить, что память нулевая. rcx = указатель, rdx = длина.
; Результат: eax = 1 (все нули) / 0.
; -----------------------------------------------------------------------------
proc st_is_zero
  .next:
        test    rdx, rdx
        jz      .yes
        cmp     byte [rcx], 0
        jne     .no
        inc     rcx
        dec     rdx
        jmp     .next
  .yes:
        mov     eax, 1
        ret
  .no:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; st_fill — залить память байтом 0xAA. rcx = указатель, rdx = длина.
; -----------------------------------------------------------------------------
proc st_fill uses rdi
        mov     rdi, rcx
        mov     rcx, rdx
        mov     al, 0xAA
        rep stosb
        ret
endp

; -----------------------------------------------------------------------------
; selftest_arena — проверка арен: init, alloc (выравнивание, нули, рост фиксации),
; ensure/advance, отказ при выходе за резерв, reset (инвариант нулей), release.
; Результат: eax = 1 / 0.
; -----------------------------------------------------------------------------
proc selftest_arena uses rbx rsi
        mov     esi, 1                  ; номер шага для сообщения об ошибке

        lea     rcx, [st_arena]
        mov     rdx, SELFTEST_RESERVE
        mov     r8d, ARENA_STEP_DOC
        call    arena_init
        test    eax, eax
        jz      .fail

        ; 2: выделение 100 байт — не 0, выровнено на 8, нулевое
        inc     esi
        lea     rcx, [st_arena]
        mov     edx, 100
        call    arena_alloc
        test    rax, rax
        jz      .fail
        mov     rbx, rax
        test    bl, 7
        jnz     .fail
        mov     rcx, rbx
        mov     edx, 100
        call    st_is_zero
        test    eax, eax
        jz      .fail
        mov     rcx, rbx
        mov     edx, 100
        call    st_fill

        ; 3: большое выделение (рост фиксации), нулевое, последний байт доступен
        inc     esi
        lea     rcx, [st_arena]
        mov     edx, SELFTEST_BIG
        call    arena_alloc
        test    rax, rax
        jz      .fail
        mov     rbx, rax
        mov     rcx, rbx
        mov     edx, SELFTEST_BIG
        call    st_is_zero
        test    eax, eax
        jz      .fail
        mov     rcx, rbx
        mov     edx, SELFTEST_BIG
        call    st_fill

        ; 4: ensure возвращает Ptr, не сдвигая его; advance сдвигает;
        ;    следующий alloc выровнен на 8 даже после нечётного advance
        inc     esi
        lea     rcx, [st_arena]
        mov     edx, 64
        call    arena_ensure
        cmp     rax, [st_arena + ARENA.Ptr]
        jne     .fail
        lea     rcx, [st_arena]
        mov     edx, 3
        call    arena_advance
        lea     rcx, [st_arena]
        mov     edx, 8
        call    arena_alloc
        test    rax, rax
        jz      .fail
        test    al, 7
        jnz     .fail

        ; 5: выход за резерв — отказ, а не сбой
        inc     esi
        lea     rcx, [st_arena]
        mov     rdx, SELFTEST_RESERVE
        call    arena_alloc
        test    rax, rax
        jnz     .fail
        lea     rcx, [st_arena]
        mov     rdx, -1
        call    arena_alloc
        test    rax, rax
        jnz     .fail

        ; 6: reset — Ptr = Base, повторные выделения снова нулевые
        inc     esi
        lea     rcx, [st_arena]
        call    arena_reset
        mov     rax, [st_arena + ARENA.Ptr]
        cmp     rax, [st_arena + ARENA.Base]
        jne     .fail
        lea     rcx, [st_arena]
        mov     edx, 100
        call    arena_alloc
        cmp     rax, [st_arena + ARENA.Base]
        jne     .fail
        mov     rcx, rax
        mov     edx, 100
        call    st_is_zero
        test    eax, eax
        jz      .fail
        lea     rcx, [st_arena]
        mov     edx, SELFTEST_BIG
        call    arena_alloc
        test    rax, rax
        jz      .fail
        mov     rcx, rax
        mov     edx, SELFTEST_BIG
        call    st_is_zero
        test    eax, eax
        jz      .fail

        ; 7: release обнуляет заголовок
        inc     esi
        lea     rcx, [st_arena]
        call    arena_release
        cmp     [st_arena + ARENA.Base], 0
        jne     .fail

        ; 8: неверные параметры init отвергаются
        inc     esi
        lea     rcx, [st_arena]
        mov     rdx, SELFTEST_RESERVE
        xor     r8d, r8d
        call    arena_init
        test    eax, eax
        jnz     .fail
        lea     rcx, [st_arena]
        mov     rdx, SELFTEST_RESERVE
        mov     r8d, 1000               ; не кратно странице
        call    arena_init
        test    eax, eax
        jnz     .fail

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
