; =============================================================================
; engine/dom.inc - построитель DOM-дерева
; =============================================================================

DOM_TYPE_ELEMENT = 1
DOM_TYPE_TEXT    = 2
DOM_TYPE_COMMENT = 3

struct DOM_NODE
  nodeType dd ?
  nodeNameLen dd ?
  nodeNamePtr dq ?
  nodeParent dq ?
  nodeFirstChild dq ?
  nodeLastChild dq ?
  nodeNextSibling dq ?
  Token       dq ?
ends

; -----------------------------------------------------------------------------
; dom_build - строит DOM-дерево из массива токенов
; rcx = указатель на первый токен (HTML_TOKEN)
; rdx = указатель на арену для узлов DOM
; Возвращает rax = указатель на корневой фиктивный узел "Document"
; -----------------------------------------------------------------------------
proc dom_build uses rbx rsi rdi r12 r13 r14 r15
  locals
    arena_ptr dq ?
    tokens    dq ?
    root      dq ?
    current   dq ?
  endl
        mov     [tokens], rcx
        mov     [arena_ptr], rdx
        
        ; Выделяем корневой узел
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.DOM_NODE
        call    arena_alloc
        test    rax, rax
        jz      .oom
        
        mov     [root], rax
        mov     [current], rax
        
        mov     dword [rax + DOM_NODE.nodeType], DOM_TYPE_ELEMENT
        mov     dword [rax + DOM_NODE.nodeNameLen], 0
        mov     qword [rax + DOM_NODE.nodeNamePtr], 0
        mov     qword [rax + DOM_NODE.nodeParent], 0
        mov     qword [rax + DOM_NODE.nodeFirstChild], 0
        mov     qword [rax + DOM_NODE.nodeLastChild], 0
        mov     qword [rax + DOM_NODE.nodeNextSibling], 0
        
        mov     rsi, [tokens]   ; текущий токен
        
  .loop:
        mov     eax, [rsi + HTML_TOKEN.Type]
        cmp     eax, HTML_TOK_EOF
        je      .done
        
        cmp     eax, HTML_TOK_TEXT
        je      .handle_text
        
        cmp     eax, HTML_TOK_TAG_OPEN
        je      .handle_open
        
        cmp     eax, HTML_TOK_TAG_CLOSE
        je      .handle_close
        
        cmp     eax, HTML_TOK_COMMENT
        je      .handle_comment

        cmp     eax, HTML_TOK_ATTR
        je      .handle_attr
        
        jmp     .next_tok


  .handle_attr:
        ; Пока просто игнорируем атрибуты в самом DOM дереве
        ; (они лежат в массиве токенов сразу после TAG_OPEN).
        jmp     .next_tok

  .handle_open:
        ; Проверим, не самозакрывающийся ли это тег (!DOCTYPE, meta, img, br, hr, input)
        ; Пока для упрощения все теги считаем контейнерами, кроме тех, что закрываются сразу.
        ; Но у нас очень тупой парсер - он ничего не знает о самозакрывающихся.
        ; Если в HTML нет закрывающего тега (например <meta>), он останется открытым до конца.
        ; В реальном движке тут нужен список "void elements".
        
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.DOM_NODE
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     r12, rax                ; новый узел
        
        mov     dword [r12 + DOM_NODE.nodeType], DOM_TYPE_ELEMENT
        mov     ecx, [rsi + HTML_TOKEN.Len]
        mov     dword [r12 + DOM_NODE.nodeNameLen], ecx
        mov     rcx, [rsi + HTML_TOKEN.Ptr]
        mov     [r12 + DOM_NODE.nodeNamePtr], rcx
        mov     qword [r12 + DOM_NODE.nodeFirstChild], 0
        mov     qword [r12 + DOM_NODE.nodeLastChild], 0
        mov     qword [r12 + DOM_NODE.nodeNextSibling], 0
        
        mov     r13, [current]
        mov     [r12 + DOM_NODE.nodeParent], r13
        
        ; Добавляем в конец списка детей current
        mov     r14, [r13 + DOM_NODE.nodeLastChild]
        test    r14, r14
        jnz     .add_sibling_open
        ; Это первый ребенок
        mov     [r13 + DOM_NODE.nodeFirstChild], r12
        mov     [r13 + DOM_NODE.nodeLastChild], r12
        jmp     .update_current
  .add_sibling_open:
        mov     [r14 + DOM_NODE.nodeNextSibling], r12
        mov     [r13 + DOM_NODE.nodeLastChild], r12
        
  .update_current:
        ; void elements
        ; Сверим имя с "meta", "link", "img", "br", "hr", "input", "!DOCTYPE"
        mov     rcx, [rsi + HTML_TOKEN.Ptr]
        mov     edx, [rsi + HTML_TOKEN.Len]
        
        ; Если начинается с '!', это точно void (например !DOCTYPE)
        cmp     edx, 1
        jb      .push_current
        cmp     byte [rcx], '!'
        je      .next_tok
        
        ; Для супер-микро-движка пока сделаем жестко зашитые проверки.
        ; (В идеале нужен хеш-сет).
        ; Пропустим это, будем надеяться, что структура вменяемая.
        
  .push_current:
        mov     [current], r12
        jmp     .next_tok

  .handle_close:
        ; Поднимаемся наверх, ища открытый тег с таким же именем
        mov     r13, [current]
        mov     rcx, [rsi + HTML_TOKEN.Ptr]
        mov     edx, [rsi + HTML_TOKEN.Len]
  .cl_loop:
        cmp     r13, [root]
        je      .next_tok               ; дошли до корня, игнорируем
        
        ; Сравниваем имя
        cmp     edx, [r13 + DOM_NODE.nodeNameLen]
        jne     .cl_up
        
        mov     r8, [r13 + DOM_NODE.nodeNamePtr]
        mov     r9d, edx
        ; Сохраним нужные регистры перед вызовом str_ieq
        push    rcx
        push    rdx
        push    r13
        call    str_ieq
        pop     r13
        pop     rdx
        pop     rcx
        test    eax, eax
        jnz     .cl_match
        
  .cl_up:
        mov     r13, [r13 + DOM_NODE.nodeParent]
        jmp     .cl_loop
        
  .cl_match:
        ; Нашли совпадающий открытый тег. Закрываем его (текущим становится его родитель)
        mov     r13, [r13 + DOM_NODE.nodeParent]
        mov     [current], r13
        jmp     .next_tok

  .handle_text:
        mov     r8d, DOM_TYPE_TEXT
        jmp     .add_leaf
        
  .handle_comment:
        mov     r8d, DOM_TYPE_COMMENT
        
  .add_leaf:
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.DOM_NODE
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     r12, rax
        
        mov     dword [r12 + DOM_NODE.nodeType], r8d
        mov     ecx, [rsi + HTML_TOKEN.Len]
        mov     dword [r12 + DOM_NODE.nodeNameLen], ecx
        mov     rcx, [rsi + HTML_TOKEN.Ptr]
        mov     [r12 + DOM_NODE.nodeNamePtr], rcx
        mov     qword [r12 + DOM_NODE.nodeFirstChild], 0
        mov     qword [r12 + DOM_NODE.nodeLastChild], 0
        mov     qword [r12 + DOM_NODE.nodeNextSibling], 0
        
        mov     r13, [current]
        mov     [r12 + DOM_NODE.nodeParent], r13
        
        mov     r14, [r13 + DOM_NODE.nodeLastChild]
        test    r14, r14
        jnz     .add_sibling_leaf
        mov     [r13 + DOM_NODE.nodeFirstChild], r12
        mov     [r13 + DOM_NODE.nodeLastChild], r12
        jmp     .next_tok
  .add_sibling_leaf:
        mov     [r14 + DOM_NODE.nodeNextSibling], r12
        mov     [r13 + DOM_NODE.nodeLastChild], r12

  .next_tok:
        add     rsi, sizeof.HTML_TOKEN
        jmp     .loop

  .done:
        mov     rax, [root]
        ret
        
  .oom:
        xor     eax, eax
        ret
endp
