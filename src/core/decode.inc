; =============================================================================
; core/decode.inc - декодирование текста и валидация UTF-8
; =============================================================================

; -----------------------------------------------------------------------------
; decode_utf8_validate - проверка, является ли текст валидным UTF-8.
; rcx = указатель на буфер, rdx = размер буфера
; Возвращает eax = 1 (валидный UTF-8 или ASCII), 0 (невалидный)
; -----------------------------------------------------------------------------
proc decode_utf8_validate uses rbx rsi rdi
        test    rdx, rdx
        jz      .valid

        mov     rsi, rcx
        lea     rdi, [rcx + rdx]        ; конец буфера
  .loop:
        cmp     rsi, rdi
        jae     .valid
        
        movzx   eax, byte [rsi]
        inc     rsi
        
        test    al, 0x80
        jz      .loop                   ; ASCII (0xxxxxxx)
        
        ; 110xxxxx 10xxxxxx
        cmp     al, 0xC2
        jb      .invalid                ; C0, C1 - overlong (или 80-BF - orphan continuation)
        cmp     al, 0xE0
        jb      .seq2
        
        ; 1110xxxx 10xxxxxx 10xxxxxx
        cmp     al, 0xF0
        jb      .seq3
        
        ; 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        cmp     al, 0xF5
        jb      .seq4
        
        jmp     .invalid                ; F5-FF невалидны
        
  .seq2:
        cmp     rsi, rdi
        jae     .invalid
        movzx   ebx, byte [rsi]
        inc     rsi
        and     bl, 0xC0
        cmp     bl, 0x80
        jne     .invalid
        jmp     .loop
        
  .seq3:
        mov     r8b, al                 ; сохраняем первый байт
        cmp     rsi, rdi
        jae     .invalid
        movzx   ebx, byte [rsi]
        inc     rsi
        
        ; Проверка на overlong и суррогаты
        cmp     r8b, 0xE0
        jne     .seq3_not_E0
        cmp     bl, 0xA0
        jb      .invalid
        jmp     .seq3_cont
  .seq3_not_E0:
        cmp     r8b, 0xED
        jne     .seq3_cont
        cmp     bl, 0x9F
        ja      .invalid
  .seq3_cont:
        and     bl, 0xC0
        cmp     bl, 0x80
        jne     .invalid
        
        cmp     rsi, rdi
        jae     .invalid
        movzx   ebx, byte [rsi]
        inc     rsi
        and     bl, 0xC0
        cmp     bl, 0x80
        jne     .invalid
        jmp     .loop
        
  .seq4:
        mov     r8b, al                 ; первый байт
        cmp     rsi, rdi
        jae     .invalid
        movzx   ebx, byte [rsi]
        inc     rsi
        
        cmp     r8b, 0xF0
        jne     .seq4_not_F0
        cmp     bl, 0x90
        jb      .invalid
        jmp     .seq4_cont
  .seq4_not_F0:
        cmp     r8b, 0xF4
        jne     .seq4_cont
        cmp     bl, 0x8F
        ja      .invalid
  .seq4_cont:
        and     bl, 0xC0
        cmp     bl, 0x80
        jne     .invalid
        
        cmp     rsi, rdi
        jae     .invalid
        movzx   ebx, byte [rsi]
        inc     rsi
        and     bl, 0xC0
        cmp     bl, 0x80
        jne     .invalid
        
        cmp     rsi, rdi
        jae     .invalid
        movzx   ebx, byte [rsi]
        inc     rsi
        and     bl, 0xC0
        cmp     bl, 0x80
        jne     .invalid
        jmp     .loop
        
  .valid:
        mov     eax, 1
        ret
  .invalid:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; decode_win1251_to_utf8 - конвертирует windows-1251 в UTF-8.
; rcx = входной буфер, rdx = длина входа
; r8 = выходной буфер (должен быть в 2-3 раза больше rdx)
; Возвращает rax = количество записанных байт (длина UTF-8)
; -----------------------------------------------------------------------------
proc decode_win1251_to_utf8 uses rbx rsi rdi
        mov     rsi, rcx
        lea     rcx, [rsi + rdx]        ; конец входа
        mov     rdi, r8
        
  .loop:
        cmp     rsi, rcx
        jae     .done
        
        movzx   eax, byte [rsi]
        inc     rsi
        
        test    al, 0x80
        jz      .ascii
        
        ; Таблица или простой маппинг для кириллицы (0xC0-0xFF -> U+0410-U+044F)
        cmp     al, 0xC0
        jb      .table                  ; 80-BF требует таблицы
        
        ; C0-FF: А-Я, а-я. C0 = U+0410. U+0410 в UTF-8: D0 90
        ; Для C0-CF: D0 90-9F
        ; Для D0-DF: D0 A0-AF
        ; Для E0-EF: D0 B0-BF
        ; Для F0-FF: D1 80-8F
        
        cmp     al, 0xF0
        jae     .p4
        
        ; C0-EF -> D0 (90-BF)
        mov     byte [rdi], 0xD0
        inc     rdi
        sub     al, 0xC0
        add     al, 0x90
        mov     byte [rdi], al
        inc     rdi
        jmp     .loop
        
  .p4:
        ; F0-FF -> D1 (80-8F)
        mov     byte [rdi], 0xD1
        inc     rdi
        sub     al, 0xF0
        add     al, 0x80
        mov     byte [rdi], al
        inc     rdi
        jmp     .loop
        
  .table:
        ; Ё, ё, и спецсимволы
        cmp     al, 0xA8
        jne     @f
        ; Ё = U+0401 -> D0 81
        mov     byte [rdi], 0xD0
        mov     byte [rdi+1], 0x81
        add     rdi, 2
        jmp     .loop
  @@:
        cmp     al, 0xB8
        jne     @f
        ; ё = U+0451 -> D1 91
        mov     byte [rdi], 0xD1
        mov     byte [rdi+1], 0x91
        add     rdi, 2
        jmp     .loop
  @@:
        ; Для остальных (не очень важных) просто заменяем на '?'
        ; В идеале нужна полная таблица 80-BF, но для базового веба хватит '?' 
        ; либо можно встроить таблицу на 64 символа.
        mov     byte [rdi], '?'
        inc     rdi
        jmp     .loop

  .ascii:
        mov     byte [rdi], al
        inc     rdi
        jmp     .loop
        
  .done:
        mov     rax, rdi
        sub     rax, r8                 ; длина записанного
        ret
endp
