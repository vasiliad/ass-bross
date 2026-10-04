; =============================================================================
; core/arena.inc — арены памяти (docs/MEMORY_MODEL.md, раздел 1).
; Резерв адресов целиком, фиксация шагами Commit_Step; адреса не перемещаются.
; Инвариант: память, выданная arena_alloc, всегда нулевая.
; =============================================================================

PAGE_SIZE = 4096

; -----------------------------------------------------------------------------
; arena_init — создать арену.
;   rcx = ARENA*, rdx = размер резерва, r8d = шаг фиксации (кратен 4 КБ, > 0)
;   Результат: rax = 1 / 0 (ошибка: неверные параметры или нет памяти)
; Резерв округляется вверх до страницы. Первый блок (Commit_Step) фиксируется сразу.
; -----------------------------------------------------------------------------
proc arena_init uses rbx rsi rdi
        mov     rbx, rcx
        mov     rsi, rdx
        mov     edi, r8d
        test    edi, edi
        jz      .fail
        test    edi, PAGE_SIZE - 1
        jnz     .fail
        ; резерв: округление вверх до страницы с проверкой переполнения
        mov     rax, -PAGE_SIZE
        cmp     rsi, rax
        ja      .fail
        add     rsi, PAGE_SIZE - 1
        and     rsi, -PAGE_SIZE
        cmp     rsi, rdi
        jb      .fail                   ; резерв меньше одного шага
        mov     rcx, rsi
        call    sys_mem_reserve
        test    rax, rax
        jz      .fail
        mov     [rbx + ARENA.Base], rax
        mov     [rbx + ARENA.Ptr], rax
        lea     rcx, [rax + rsi]
        mov     [rbx + ARENA.Reserve_End], rcx
        mov     [rbx + ARENA.Commit_Step], edi
        mov     [rbx + ARENA.Flags], 0
        mov     [rbx + ARENA.Commit_End], rax
        ; первый блок
        mov     rcx, rax
        mov     edx, edi
        call    sys_mem_commit
        test    eax, eax
        jz      .commit_failed
        mov     ecx, [rbx + ARENA.Commit_Step]
        add     [rbx + ARENA.Commit_End], rcx
        mov     eax, 1
        ret
  .commit_failed:
        mov     rcx, [rbx + ARENA.Base]
        call    sys_mem_release
        mov     rdi, rbx
        mov     ecx, sizeof.ARENA
        xor     eax, eax
        rep stosb
  .fail:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; arena_ensure — гарантировать n зафиксированных байт после Ptr.
;   rcx = ARENA*, rdx = n
;   Результат: rax = Ptr (память [Ptr, Ptr+n) доступна) или 0 (выход за резерв / нет памяти)
; Ptr не сдвигается: после записи вызывается arena_advance.
; -----------------------------------------------------------------------------
proc arena_ensure uses rbx rsi rdi
        mov     rbx, rcx
        mov     rsi, [rbx + ARENA.Ptr]
        mov     rax, [rbx + ARENA.Reserve_End]
        sub     rax, rsi                ; свободно в резерве
        cmp     rdx, rax
        ja      .fail                   ; n больше остатка резерва (в т.ч. огромные n)
        lea     rax, [rsi + rdx]        ; нужная граница, не больше Reserve_End
        mov     rcx, [rbx + ARENA.Commit_End]
        cmp     rax, rcx
        jbe     .ok
        ; докомитить целое число шагов: ceil((need - Commit_End) / Step) * Step
        sub     rax, rcx
        mov     r8d, [rbx + ARENA.Commit_Step]
        lea     rax, [rax + r8 - 1]
        xor     edx, edx
        div     r8
        mul     r8                      ; rax = размер фиксации
        ; не выходить за резерв (резерв кратен странице, но не обязательно шагу)
        mov     rdx, [rbx + ARENA.Reserve_End]
        sub     rdx, [rbx + ARENA.Commit_End]
        cmp     rax, rdx
        jbe     .size_ok
        mov     rax, rdx
  .size_ok:
        mov     rdi, rax                ; запомнить размер для сдвига Commit_End
        mov     rdx, rax
        mov     rcx, [rbx + ARENA.Commit_End]
        call    sys_mem_commit
        test    eax, eax
        jz      .fail
        add     [rbx + ARENA.Commit_End], rdi
  .ok:
        mov     rax, rsi
        ret
  .fail:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; arena_alloc — выделить n байт, выровненных на 8, обнулённых.
;   rcx = ARENA*, rdx = n
;   Результат: rax = указатель или 0
; -----------------------------------------------------------------------------
proc arena_alloc uses rbx rsi
        mov     rbx, rcx
        mov     rax, 0x7FFFFFFFFFFFFFF8
        cmp     rdx, rax
        ja      .fail
        lea     rsi, [rdx + 7]
        and     rsi, -8                 ; размер, кратный 8
        ; выровнять Ptr (после arena_advance с нечётным числом байт)
        mov     rax, [rbx + ARENA.Ptr]
        add     rax, 7
        and     rax, -8
        cmp     rax, [rbx + ARENA.Reserve_End]
        ja      .fail
        mov     [rbx + ARENA.Ptr], rax
        mov     rcx, rbx
        mov     rdx, rsi
        call    arena_ensure
        test    rax, rax
        jz      .fail
        add     [rbx + ARENA.Ptr], rsi
        ret
  .fail:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; arena_advance — сдвинуть Ptr после записи через arena_ensure.
;   rcx = ARENA*, rdx = сколько байт записано (не больше, чем было гарантировано)
; -----------------------------------------------------------------------------
proc arena_advance
        add     [rcx + ARENA.Ptr], rdx
        ret
endp

; -----------------------------------------------------------------------------
; arena_reset — вернуть арену в начальное состояние, сохранив инвариант обнулённости.
;   rcx = ARENA*
; Первый блок остаётся зафиксированным и обнуляется (AF_NET_TARGET — целиком,
; иначе — только использованная часть); всё выше первого блока декоммитится.
; -----------------------------------------------------------------------------
proc arena_reset uses rbx rsi rdi
        mov     rbx, rcx
        mov     rdi, [rbx + ARENA.Base]
        mov     eax, [rbx + ARENA.Commit_Step]
        lea     rsi, [rdi + rax]        ; граница первого блока
        mov     rcx, rax                ; по умолчанию — весь первый блок
        test    [rbx + ARENA.Flags], AF_NET_TARGET
        jnz     .zero
        mov     rcx, [rbx + ARENA.Ptr]
        cmp     rcx, rsi
        jbe     @f
        mov     rcx, rsi
    @@: sub     rcx, rdi                ; использованная часть первого блока
  .zero:
        xor     eax, eax
        rep stosb
        ; декоммит всего выше первого блока
        mov     rcx, [rbx + ARENA.Commit_End]
        cmp     rcx, rsi
        jbe     .done
        sub     rcx, rsi
        mov     rdx, rcx
        mov     rcx, rsi
        call    sys_mem_decommit
        mov     [rbx + ARENA.Commit_End], rsi
  .done:
        mov     rax, [rbx + ARENA.Base]
        mov     [rbx + ARENA.Ptr], rax
        ret
endp

; -----------------------------------------------------------------------------
; arena_release — вернуть весь диапазон ОС, обнулить заголовок.
;   rcx = ARENA*
; -----------------------------------------------------------------------------
proc arena_release uses rbx rdi
        mov     rbx, rcx
        mov     rcx, [rbx + ARENA.Base]
        test    rcx, rcx
        jz      .done
        call    sys_mem_release
  .done:
        mov     rdi, rbx
        mov     ecx, sizeof.ARENA
        xor     eax, eax
        rep stosb
        ret
endp
