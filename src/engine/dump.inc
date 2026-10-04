; =============================================================================
; engine/dump.inc - утилиты для отладки и вывода DOM/токенов
; =============================================================================

uglobal
  dump_arena ARENA
endg

iglobal
  sdef s_tok_text,  'TEXT: '
  sdef s_tok_open,  'OPEN: '
  sdef s_tok_close, 'CLOSE: '
  sdef s_tok_comm,  'COMM: '
  sdef s_tok_eof,   'EOF', 10
  
  sdef s_err_read,  'nanoweb: cannot read file', 10
  sdef s_err_mem,   'nanoweb: memory allocation failed', 10
endg

; -----------------------------------------------------------------------------
; cmd_dump_tokens - загрузить файл и вывести его токены
; rcx = указатель на имя файла (UTF-8)
; rdx = длина имени файла
; Возвращает код возврата ОС (EXIT_OK / EXIT_ERROR)
; -----------------------------------------------------------------------------
proc cmd_dump_tokens uses rbx rsi rdi r12 r13
  locals
    wpath rw WPATH_MAX
  endl
        mov     rbx, rcx
        mov     r12, rdx
        
        ; Конвертируем имя файла в UTF-16
        invoke  MultiByteToWideChar, NW_CP_UTF8, 0, rbx, r12d, addr wpath, WPATH_MAX - 1
        test    eax, eax
        jz      .err_read
        movsxd rax, eax
        lea     rcx, [wpath]
        lea     rcx, [rcx + rax * 2]
        mov     word [rcx], 0           ; null terminator
        
        ; Инициализируем арену для файла и токенов (64 МБ)
        lea     rcx, [dump_arena]
        mov     rdx, 64 shl 20
        mov     r8d, ARENA_STEP_DOC
        call    arena_init
        test    eax, eax
        jz      .err_mem
        
        ; Читаем файл в арену
        lea     rcx, [wpath]
        lea     rdx, [dump_arena]
        call    file_load_arena
        test    rax, rax
        js      .err_read
        
        mov     rbx, rax                ; rbx = указатель на данные файла
        mov     r12, rdx                ; r12 = длина файла
        
        ; Парсим!
        mov     rcx, rbx
        mov     rdx, r12
        lea     r8, [dump_arena]
        call    html_tokenize
        test    rax, rax
        jz      .err_mem
        mov     rbx, rax                ; rbx = первый токен
        
  .loop:
        mov     eax, [rbx + HTML_TOKEN.Type]
        cmp     eax, HTML_TOK_EOF
        je      .print_eof
        
        cmp     eax, HTML_TOK_TEXT
        jne     @f
        slice   s_tok_text
        jmp     .print
  @@:   cmp     eax, HTML_TOK_TAG_OPEN
        jne     @f
        slice   s_tok_open
        jmp     .print
  @@:   cmp     eax, HTML_TOK_TAG_CLOSE
        jne     @f
        slice   s_tok_close
        jmp     .print
  @@:   cmp     eax, HTML_TOK_COMMENT
        jne     .next
        slice   s_tok_comm
        
  .print:
        call    dbg_print_str
        mov     rcx, [rbx + HTML_TOKEN.Ptr]
        mov     edx, [rbx + HTML_TOKEN.Len]
        call    dbg_print_str
        call    dbg_print_nl
        
  .next:
        add     rbx, sizeof.HTML_TOKEN
        jmp     .loop
        
  .print_eof:
        slice   s_tok_eof
        call    dbg_print_str
        mov     eax, EXIT_OK
        ret
        
  .err_read:
        slice   s_err_read
        call    sys_write_err
        mov     eax, EXIT_ERROR
        ret
        
  .err_mem:
        slice   s_err_mem
        call    sys_write_err
        mov     eax, EXIT_ERROR
        ret
endp

; -----------------------------------------------------------------------------
; dump_dom_node - рекурсивно выводит DOM-узел и его детей
; rcx = узел DOM_NODE
; rdx = уровень вложенности
; -----------------------------------------------------------------------------
proc dump_dom_node uses rbx rsi rdi r12 r13 r14
        mov     rbx, rcx
        mov     r12, rdx
        
        ; Выводим отступ (2 пробела на уровень)
        mov     r13, r12
  .indent:
        test    r13, r13
        jz      .print_node
        mov     eax, '  '
        call    dbg_print_char
        call    dbg_print_char
        dec     r13
        jmp     .indent
        
  .print_node:
        mov     eax, [rbx + DOM_NODE.nodeType]
        cmp     eax, DOM_TYPE_ELEMENT
        je      .elem
        cmp     eax, DOM_TYPE_TEXT
        je      .text
        cmp     eax, DOM_TYPE_COMMENT
        je      .comm
        jmp     .children
        
  .elem:
        mov     al, '<'
        call    dbg_print_char
        mov     rcx, [rbx + DOM_NODE.nodeNamePtr]
        mov     edx, [rbx + DOM_NODE.nodeNameLen]
        test    rcx, rcx
        jz      @f
        call    dbg_print_str
  @@:   mov     al, '>'
        call    dbg_print_char
        call    dbg_print_nl
        jmp     .children
        
  .text:
        slice   s_tok_text
        call    dbg_print_str
        mov     rcx, [rbx + DOM_NODE.nodeNamePtr]
        mov     edx, [rbx + DOM_NODE.nodeNameLen]
        call    dbg_print_str
        call    dbg_print_nl
        jmp     .children
        
  .comm:
        slice   s_tok_comm
        call    dbg_print_str
        mov     rcx, [rbx + DOM_NODE.nodeNamePtr]
        mov     edx, [rbx + DOM_NODE.nodeNameLen]
        call    dbg_print_str
        call    dbg_print_nl
        
  .children:
        mov     rsi, [rbx + DOM_NODE.nodeFirstChild]
  .child_loop:
        test    rsi, rsi
        jz      .done
        mov     rcx, rsi
        lea     rdx, [r12 + 1]
        call    dump_dom_node
        mov     rsi, [rsi + DOM_NODE.nodeNextSibling]
        jmp     .child_loop
        
  .done:
        ret
endp

; -----------------------------------------------------------------------------
; cmd_dump_dom - загрузить файл и вывести его DOM-дерево
; rcx = указатель на имя файла (UTF-8)
; rdx = длина имени файла
; Возвращает код возврата ОС (EXIT_OK / EXIT_ERROR)
; -----------------------------------------------------------------------------
proc cmd_dump_dom uses rbx rsi rdi r12 r13
  locals
    wpath rw WPATH_MAX
  endl
        mov     rbx, rcx
        mov     r12, rdx
        
        invoke  MultiByteToWideChar, NW_CP_UTF8, 0, rbx, r12d, addr wpath, WPATH_MAX - 1
        test    eax, eax
        jz      .err_read
        movsxd rax, eax
        lea     rcx, [wpath]
        lea     rcx, [rcx + rax * 2]
        mov     word [rcx], 0
        
        lea     rcx, [dump_arena]
        mov     rdx, 64 shl 20
        mov     r8d, ARENA_STEP_DOC
        call    arena_init
        test    eax, eax
        jz      .err_mem
        
        lea     rcx, [wpath]
        lea     rdx, [dump_arena]
        call    file_load_arena
        test    rax, rax
        js      .err_read
        
        mov     rcx, rax
        mov     rdx, rdx
        lea     r8, [dump_arena]
        call    html_tokenize
        test    rax, rax
        jz      .err_mem
        
        mov     rcx, rax
        lea     rdx, [dump_arena]
        call    dom_build
        test    rax, rax
        jz      .err_mem
        
        mov     rcx, rax
        xor     edx, edx
        call    dump_dom_node
        
        mov     eax, EXIT_OK
        ret
        
  .err_read:
        slice   s_err_read
        call    sys_write_err
        mov     eax, EXIT_ERROR
        ret
        
  .err_mem:
        slice   s_err_mem
        call    sys_write_err
        mov     eax, EXIT_ERROR
        ret
endp
