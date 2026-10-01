; =============================================================================
; core/str.inc — операции со срезами строк (ptr + len) и форматирование чисел.
; Все функции — leaf, без вызовов WinAPI.
; =============================================================================

; -----------------------------------------------------------------------------
; str_eq — побайтное сравнение срезов (leaf).
;   rcx/edx = срез A, r8/r9d = срез B. Результат: eax = 1, если равны.
;   Портит: rax, rcx, rdx, r8, r9, r10.
; -----------------------------------------------------------------------------
str_eq:                                 ; leaf
        cmp     edx, r9d
        jne     .no
  .loop:
        test    edx, edx
        jz      .yes
        mov     al, [rcx]
        cmp     al, [r8]
        jne     .no
        inc     rcx
        inc     r8
        dec     edx
        jmp     .loop
  .yes:
        mov     eax, 1
        ret
  .no:
        xor     eax, eax
        ret

; -----------------------------------------------------------------------------
; str_ieq — сравнение срезов без учёта регистра ASCII (leaf).
;   Аргументы и результат как у str_eq. Портит: rax, rcx, rdx, r8, r9, r10.
; -----------------------------------------------------------------------------
str_ieq:                                ; leaf
        cmp     edx, r9d
        jne     .no
  .loop:
        test    edx, edx
        jz      .yes
        movzx   eax, byte [rcx]
        movzx   r10d, byte [r8]
        lea     r9d, [eax - 'A']
        cmp     r9d, 'Z' - 'A'
        ja      @f
        or      eax, 0x20
  @@:
        lea     r9d, [r10d - 'A']
        cmp     r9d, 'Z' - 'A'
        ja      @f
        or      r10d, 0x20
  @@:
        cmp     eax, r10d
        jne     .no
        inc     rcx
        inc     r8
        dec     edx
        jmp     .loop
  .yes:
        mov     eax, 1
        ret
  .no:
        xor     eax, eax
        ret

; -----------------------------------------------------------------------------
; parse_u32 — десятичное число без знака из среза, с насыщением (leaf).
;   rcx/edx = срез. Результат: eax = значение (не больше 0xFFFFFFFF),
;   edx = 1, если срез целиком состоит из цифр и не пуст, иначе 0.
;   Портит: rax, rcx, rdx, r8, r9.
; -----------------------------------------------------------------------------
parse_u32:                              ; leaf
        xor     eax, eax
        test    edx, edx
        jz      .bad
        mov     r9d, edx
  .loop:
        movzx   r8d, byte [rcx]
        sub     r8d, '0'
        cmp     r8d, 9
        ja      .bad
        imul    rax, rax, 10
        add     rax, r8
        mov     r8, rax
        shr     r8, 32
        jz      @f
        mov     eax, 0xFFFFFFFF         ; насыщение (старшая половина обнуляется записью в eax)
  @@:
        inc     rcx
        dec     r9d
        jnz     .loop
        mov     edx, 1
        ret
  .bad:
        xor     edx, edx
        ret

; -----------------------------------------------------------------------------
; put_bytes — дописать срез в буфер (leaf).
;   rdi = курсор буфера, rsi = данные, ecx = длина. После: rdi сдвинут.
;   Портит: rcx, rsi, rdi.
; -----------------------------------------------------------------------------
put_bytes:                              ; leaf
        rep     movsb
        ret

; -----------------------------------------------------------------------------
; put_hex — шестнадцатеричная запись числа фиксированной ширины (leaf).
;   rdi = курсор, rax = число, ecx = цифр (1..16). После: rdi сдвинут.
;   Портит: rax, rcx, rdx, rdi.
; -----------------------------------------------------------------------------
put_hex:                                ; leaf
        lea     rdi, [rdi + rcx]
        push    rdi
  .loop:
        dec     rdi
        mov     edx, eax
        and     edx, 0x0F
        add     dl, '0'
        cmp     dl, '9'
        jbe     @f
        add     dl, 'A' - '9' - 1       ; 10..15 → 'A'..'F'
  @@:
        mov     [rdi], dl
        shr     rax, 4
        dec     ecx
        jnz     .loop
        pop     rdi
        ret

; -----------------------------------------------------------------------------
; put_dec — десятичная запись знакового числа (leaf).
;   rdi = курсор, rax = число. После: rdi сдвинут.
;   Портит: rax, rcx, rdx, r8, rdi.
; -----------------------------------------------------------------------------
put_dec:                                ; leaf
        test    rax, rax
        jns     .positive
        mov     byte [rdi], '-'
        inc     rdi
        neg     rax                     ; для INT64_MIN даёт то же число — обрабатывается как беззнаковое
  .positive:
        mov     r8, rdi                 ; начало цифр
        mov     ecx, 10
  .digit:
        xor     edx, edx
        div     rcx
        add     dl, '0'
        mov     [rdi], dl
        inc     rdi
        test    rax, rax
        jnz     .digit
        ; цифры записаны в обратном порядке — разворачиваем
        lea     rcx, [rdi - 1]
  .reverse:
        cmp     r8, rcx
        jae     .done
        mov     al, [r8]
        mov     dl, [rcx]
        mov     [r8], dl
        mov     [rcx], al
        inc     r8
        dec     rcx
        jmp     .reverse
  .done:
        ret
