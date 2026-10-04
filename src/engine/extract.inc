; =============================================================================
; engine/extract.inc - поиск узлов и извлечение текста
; =============================================================================

; -----------------------------------------------------------------------------
; dom_find_by_attr - рекурсивный поиск первого узла с нужным атрибутом
; rcx = корневой узел
; rdx = имя атрибута (срез: rdx=ptr, r8=len)
; r9  = значение атрибута (срез: r9=ptr, [rsp+40]=len)
; Возвращает rax = узел DOM_NODE или 0
; -----------------------------------------------------------------------------
proc dom_find_by_attr uses rbx rsi rdi r12 r13 r14 r15
  locals
    attr_name_ptr dq ?
    attr_name_len dq ?
    attr_val_ptr  dq ?
    attr_val_len  dq ?
  endl
        mov     [attr_name_ptr], rdx
        mov     [attr_name_len], r8
        mov     [attr_val_ptr], r9
        mov     rax, [rsp + 80]         ; [rsp+40] + locals + regs
        mov     [attr_val_len], rax
        
        test    rcx, rcx
        jz      .not_found
        
        mov     rbx, rcx
        
        cmp     dword [rbx + DOM_NODE.nodeType], DOM_TYPE_ELEMENT
        jne     .check_children
        
        mov     rsi, [rbx + DOM_NODE.Token]
        test    rsi, rsi
        jz      .check_children
        
        ; Ищем в токенах (rsi указывает на TAG_OPEN)
        add     rsi, sizeof.HTML_TOKEN
  .attr_loop:
        mov     eax, [rsi + HTML_TOKEN.Type]
        cmp     eax, HTML_TOK_ATTR
        jne     .check_children         ; кончились атрибуты
        
        ; Сравниваем имя
        mov     rcx, [rsi + HTML_TOKEN.Ptr]
        mov     edx, [rsi + HTML_TOKEN.Len]
        mov     r8, [attr_name_ptr]
        mov     r9d, dword [attr_name_len]
        call    str_ieq
        test    eax, eax
        jz      .next_attr
        
        ; Имя совпало. Значение - это следующий токен TEXT (хак из html.inc)
        mov     r14, rsi
        add     r14, sizeof.HTML_TOKEN
        cmp     dword [r14 + HTML_TOKEN.Type], HTML_TOK_TEXT
        jne     .next_attr
        
        mov     rcx, [r14 + HTML_TOKEN.Ptr]
        mov     edx, [r14 + HTML_TOKEN.Len]
        mov     r8, [attr_val_ptr]
        mov     r9d, dword [attr_val_len]
        call    str_ieq
        test    eax, eax
        jnz     .found
        
  .next_attr:
        add     rsi, sizeof.HTML_TOKEN
        ; Если мы пропустили значение (TEXT), нужно прыгнуть еще раз
        cmp     dword [rsi + HTML_TOKEN.Type], HTML_TOK_TEXT
        jne     .attr_loop
        add     rsi, sizeof.HTML_TOKEN
        jmp     .attr_loop
        
  .found:
        mov     rax, rbx
        ret
        
  .check_children:
        mov     rbx, [rbx + DOM_NODE.nodeFirstChild]
  .child_loop:
        test    rbx, rbx
        jz      .not_found
        
        mov     rcx, rbx
        mov     rdx, [attr_name_ptr]
        mov     r8, [attr_name_len]
        mov     r9, [attr_val_ptr]
        push    [attr_val_len]
        sub     rsp, 32
        call    dom_find_by_attr
        add     rsp, 40
        test    rax, rax
        jnz     .done
        
        mov     rbx, [rbx + DOM_NODE.nodeNextSibling]
        jmp     .child_loop
        
  .not_found:
        xor     eax, eax
  .done:
        ret
endp

; -----------------------------------------------------------------------------
; dom_extract_text - собирает текст из DOM-дерева в буфер
; rcx = корневой узел
; rdx = указатель на буфер вывода
; r8  = максимальный размер буфера
; r9  = текущая позиция записи (смещение)
; Возвращает rax = новая позиция записи
; -----------------------------------------------------------------------------
proc dom_extract_text uses rbx rsi rdi r12 r13 r14
        mov     rbx, rcx
        mov     r12, rdx
        mov     r13, r8
        mov     r14, r9
        
        test    rbx, rbx
        jz      .done
        
        mov     eax, [rbx + DOM_NODE.nodeType]
        cmp     eax, DOM_TYPE_TEXT
        je      .text
        
        ; Игнорируем script, style
        mov     rcx, [rbx + DOM_NODE.nodeNamePtr]
        mov     edx, [rbx + DOM_NODE.nodeNameLen]
        test    rcx, rcx
        jz      .children
        
        ; Очень простой пропуск "script"
        cmp     edx, 6
        jne     @f
        mov     al, [rcx]
        or      al, 0x20
        cmp     al, 's'
        je      .done           ; грубо, но для примера сойдет
  @@:
        
  .children:
        mov     rbx, [rbx + DOM_NODE.nodeFirstChild]
  .child_loop:
        test    rbx, rbx
        jz      .block_done
        mov     rcx, rbx
        mov     rdx, r12
        mov     r8, r13
        mov     r9, r14
        call    dom_extract_text
        mov     r14, rax
        mov     rbx, [rbx + DOM_NODE.nodeNextSibling]
        jmp     .child_loop
        
  .block_done:
        ; Можно добавить перевод строки после div/p/br
        mov     rax, r14
        ret
        
  .text:
        mov     rsi, [rbx + DOM_NODE.nodeNamePtr]
        mov     ecx, [rbx + DOM_NODE.nodeNameLen]
        test    ecx, ecx
        jz      .done
        
  .copy:
        cmp     r14, r13
        jae     .done
        mov     al, [rsi]
        mov     [r12 + r14], al
        inc     r14
        inc     rsi
        dec     ecx
        jnz     .copy
        
        ; Добавим пробел после текста
        cmp     r14, r13
        jae     .done
        mov     byte [r12 + r14], ' '
        inc     r14
        
  .done:
        mov     rax, r14
        ret
endp
