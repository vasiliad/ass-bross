; =============================================================================
; engine/html.inc - быстрый HTML-токенизатор (лексер)
; =============================================================================

; Типы токенов
HTML_TOK_EOF        = 0
HTML_TOK_TEXT       = 1
HTML_TOK_TAG_OPEN   = 2
HTML_TOK_TAG_CLOSE  = 3
HTML_TOK_COMMENT    = 4

struct HTML_TOKEN
  Type   dd ?           ; HTML_TOK_*
  Len    dd ?           ; длина данных
  Ptr    dq ?           ; указатель на данные (имя тега, текст)
ends

; -----------------------------------------------------------------------------
; html_tokenize - разбирает HTML в массив токенов
; rcx = входной текст (UTF-8), rdx = длина текста
; r8 = арена для выделения токенов
; Возвращает rax = указатель на первый токен (или 0, если ошибка памяти)
; Последний токен всегда имеет Type = HTML_TOK_EOF
; -----------------------------------------------------------------------------
proc html_tokenize uses rbx rsi rdi r12 r13 r14 r15
  locals
    arena_ptr dq ?
    first_tok dq ?
    text_end  dq ?
  endl
        mov     [arena_ptr], r8
        mov     rsi, rcx
        lea     rdi, [rcx + rdx]
        mov     [text_end], rdi
        
        mov     [first_tok], 0
        mov     r14, 0          ; последний токен (для связывания? нет, просто массив)
        
        ; В этой реализации мы просто будем выделять память под каждый токен в арене
        ; последовательно. Массив токенов будет непрерывным.
        
        ; Сначала выделим "голову" массива токенов - чтобы можно было вернуть указатель.
        ; Но так как токены выделяются по одному, первый arena_alloc даст начало.
        mov     rcx, [arena_ptr]
        call    arena_ensure
        mov     [first_tok], rax
        
  .next_node:
        cmp     rsi, [text_end]
        jae     .eof
        
        cmp     byte [rsi], '<'
        je      .parse_tag
        
  .parse_text:
        ; Ищем следующий '<' или конец
        mov     r12, rsi        ; начало текста
  .text_loop:
        cmp     rsi, [text_end]
        jae     .text_done
        cmp     byte [rsi], '<'
        je      .text_done
        inc     rsi
        jmp     .text_loop
  .text_done:
        mov     r13, rsi
        sub     r13, r12        ; r13 = длина текста
        jz      .next_node      ; пустой текст
        
        ; Выделяем токен
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.HTML_TOKEN
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     r15, rax
        
        mov     dword [r15 + HTML_TOKEN.Type], HTML_TOK_TEXT
        mov     dword [r15 + HTML_TOKEN.Len], r13d
        mov     [r15 + HTML_TOKEN.Ptr], r12
        jmp     .next_node
        
  .parse_tag:
        inc     rsi             ; пропускаем '<'
        cmp     rsi, [text_end]
        jae     .eof
        
        cmp     byte [rsi], '/'
        je      .tag_close
        
        cmp     byte [rsi], '!'
        je      .comment_or_doctype
        
        ; Открывающий тег (игнорируем атрибуты пока, берем только имя)
        mov     r12, rsi
  .tag_name_loop:
        cmp     rsi, [text_end]
        jae     .tag_name_done
        mov     al, [rsi]
        cmp     al, ' '
        je      .tag_name_done
        cmp     al, '>'
        je      .tag_name_done
        cmp     al, '/'
        je      .tag_name_done
        inc     rsi
        jmp     .tag_name_loop
  .tag_name_done:
        mov     r13, rsi
        sub     r13, r12
        
        ; Пропускаем до '>'
  .skip_to_gt:
        cmp     rsi, [text_end]
        jae     .emit_open
        cmp     byte [rsi], '>'
        je      .got_gt
        inc     rsi
        jmp     .skip_to_gt
  .got_gt:
        inc     rsi
        
  .emit_open:
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.HTML_TOKEN
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     r15, rax
        mov     dword [r15 + HTML_TOKEN.Type], HTML_TOK_TAG_OPEN
        mov     dword [r15 + HTML_TOKEN.Len], r13d
        mov     [r15 + HTML_TOKEN.Ptr], r12
        jmp     .next_node

  .tag_close:
        inc     rsi             ; пропускаем '/'
        mov     r12, rsi
  .tag_close_loop:
        cmp     rsi, [text_end]
        jae     .tag_close_done
        mov     al, [rsi]
        cmp     al, '>'
        je      .tag_close_done
        cmp     al, ' '
        je      .tag_close_done
        inc     rsi
        jmp     .tag_close_loop
  .tag_close_done:
        mov     r13, rsi
        sub     r13, r12
        
  .skip_to_gt2:
        cmp     rsi, [text_end]
        jae     .emit_close
        cmp     byte [rsi], '>'
        je      .got_gt2
        inc     rsi
        jmp     .skip_to_gt2
  .got_gt2:
        inc     rsi
        
  .emit_close:
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.HTML_TOKEN
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     r15, rax
        mov     dword [r15 + HTML_TOKEN.Type], HTML_TOK_TAG_CLOSE
        mov     dword [r15 + HTML_TOKEN.Len], r13d
        mov     [r15 + HTML_TOKEN.Ptr], r12
        jmp     .next_node

  .comment_or_doctype:
        ; Простая эвристика: пропускаем всё до '>'
        inc     rsi
        mov     r12, rsi
  .comment_loop:
        cmp     rsi, [text_end]
        jae     .comment_done
        cmp     byte [rsi], '>'
        je      .comment_done
        inc     rsi
        jmp     .comment_loop
  .comment_done:
        mov     r13, rsi
        sub     r13, r12
        cmp     rsi, [text_end]
        jae     .emit_comment
        inc     rsi             ; пропускаем '>'
  .emit_comment:
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.HTML_TOKEN
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     r15, rax
        mov     dword [r15 + HTML_TOKEN.Type], HTML_TOK_COMMENT
        mov     dword [r15 + HTML_TOKEN.Len], r13d
        mov     [r15 + HTML_TOKEN.Ptr], r12
        jmp     .next_node

  .eof:
        mov     rcx, [arena_ptr]
        mov     edx, sizeof.HTML_TOKEN
        call    arena_alloc
        test    rax, rax
        jz      .oom
        mov     dword [rax + HTML_TOKEN.Type], HTML_TOK_EOF
        mov     dword [rax + HTML_TOKEN.Len], 0
        mov     qword [rax + HTML_TOKEN.Ptr], 0
        
        mov     rax, [first_tok]
        ret
        
  .oom:
        xor     eax, eax
        ret
endp
