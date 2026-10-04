; =============================================================================
; net/http.inc - базовая загрузка данных по HTTP/HTTPS (через WinINet)
; =============================================================================

INTERNET_OPEN_TYPE_PRECONFIG = 0
INTERNET_FLAG_RELOAD         = 0x80000000
INTERNET_FLAG_SECURE         = 0x00800000
INTERNET_FLAG_NO_CACHE_WRITE = 0x04000000

iglobal
  sdef s_http_ua, 'nanoweb/0.1'
endg

; -----------------------------------------------------------------------------
; http_get_arena - скачивает данные по URL в указанную арену
; rcx = указатель на URL (UTF-8/ASCII null-terminated)
; rdx = указатель на ARENA
; Возвращает rax = указатель на начало скачанных данных в арене, rdx = длина
; Возвращает rax = 0 в случае ошибки
; -----------------------------------------------------------------------------
proc http_get_arena uses rbx rsi rdi r12 r13
  locals
    hInet   dq ?
    hUrl    dq ?
    arena   dq ?
    bytes   dd ?
    out_ptr dq ?
    out_len dq ?
  endl
        mov     r12, rcx        ; url
        mov     [arena], rdx
        mov     [out_len], 0
        
        ; Выделяем пустой блок 0 байт чтобы узнать стартовый адрес
        mov     rcx, [arena]
        xor     edx, edx
        call    arena_alloc
        mov     [out_ptr], rax
        
        ; InternetOpenA
        invoke  InternetOpenA, addr s_http_ua, 0, 0, 0, 0
        test    rax, rax
        jz      .err
        mov     [hInet], rax
        
        ; InternetOpenUrlA
        mov     eax, 0x84000000
        invoke  InternetOpenUrlA, [hInet], r12, 0, 0, eax, 0
        test    rax, rax
        jz      .err_inet
        mov     [hUrl], rax
        
  .read_loop:
        ; Просим аллокатор гарантировать наличие 64 КБ
        mov     rcx, [arena]
        mov     edx, 65536
        call    arena_ensure
        
        ; Получаем текущий указатель аллокации
        mov     rcx, [arena]
        mov     rbx, [rcx + ARENA.Ptr]
        
        ; Читаем в этот буфер
        mov     rcx, [hUrl]
        mov     rdx, rbx
        mov     r8d, 65536
        lea     r9, [bytes]
        invoke  InternetReadFile, rcx, rdx, r8, r9
        test    eax, eax
        jz      .err_url
        
        mov     eax, [bytes]
        test    eax, eax
        jz      .done           ; конец файла
        
        ; Фиксируем прочитанные байты в арене
        mov     rcx, [arena]
        mov     edx, eax
        call    arena_alloc
        
        ; Увеличиваем общую длину
        mov     eax, [bytes]
        add     [out_len], rax
        jmp     .read_loop
        
  .done:
        invoke  InternetCloseHandle, [hUrl]
        invoke  InternetCloseHandle, [hInet]
        mov     rax, [out_ptr]
        mov     rdx, [out_len]
        ret
        
  .err_url:
        invoke  InternetCloseHandle, [hUrl]
  .err_inet:
        invoke  InternetCloseHandle, [hInet]
  .err:
        xor     eax, eax
        xor     edx, edx
        ret
endp
