; =============================================================================
; core/settings.inc — чтение nanoweb.ini (ARCHITECTURE.md, раздел 9).
; Файл ищется рядом с nanoweb.exe (переносной режим), иначе в
; %APPDATA%\NanoWeb\. Формат: [секция], ключ = значение, комментарии ; и #.
; Имена секций и ключей — без учёта регистра ASCII.
; =============================================================================

INI_MAX = 64 shl 10

uglobal
  ini_buf    rb INI_MAX
  ini_len    dd ?
             dd ?
  ini_wpath  rw WPATH_MAX
endg

iglobal
  w_ini_name  du 'nanoweb.ini', 0
endg

; -----------------------------------------------------------------------------
; settings_load — найти и прочитать nanoweb.ini. Нет файла — пустые настройки.
; -----------------------------------------------------------------------------
proc settings_load
        mov     [ini_len], 0
        lea     rcx, [ini_wpath]
        lea     rdx, [w_ini_name]
        call    sys_exe_dir_path
        test    eax, eax
        jz      .appdata
        lea     rcx, [ini_wpath]
        call    sys_file_exists
        test    eax, eax
        jnz     .read
  .appdata:
        lea     rcx, [ini_wpath]
        lea     rdx, [w_ini_name]
        call    sys_appdata_path
        test    eax, eax
        jz      .done
  .read:
        lea     rcx, [ini_wpath]
        lea     rdx, [ini_buf]
        mov     r8d, INI_MAX
        call    sys_file_read
        test    rax, rax
        js      .done
        mov     [ini_len], eax
  .done:
        ret
endp

; -----------------------------------------------------------------------------
; ini_trim — убрать пробелы и табуляции по краям среза (leaf).
;   rsi = начало, rcx = конец (не включительно). После: rsi..rcx без пробелов.
; -----------------------------------------------------------------------------
ini_trim:                               ; leaf
  .left:
        cmp     rsi, rcx
        jae     .done
        cmp     byte [rsi], ' '
        je      .l1
        cmp     byte [rsi], 9
        jne     .right
  .l1:
        inc     rsi
        jmp     .left
  .right:
        cmp     rcx, rsi
        jbe     .done
        cmp     byte [rcx - 1], ' '
        je      .r1
        cmp     byte [rcx - 1], 9
        jne     .done
  .r1:
        dec     rcx
        jmp     .right
  .done:
        ret

; -----------------------------------------------------------------------------
; settings_get — значение ключа.
;   rcx/edx = секция, r8/r9d = ключ.
;   Результат: rax = указатель на значение (0, если не найдено), edx = длина.
; -----------------------------------------------------------------------------
proc settings_get uses rbx rsi rdi r12 r13 r14 r15
  locals
    sec_ptr  dq ?
    key_ptr  dq ?
    sec_len  dd ?
    key_len  dd ?
    in_sec   dd ?
    line_end dq ?
  endl
        mov     [sec_ptr], rcx
        mov     [sec_len], edx
        mov     [key_ptr], r8
        mov     [key_len], r9d
        mov     [in_sec], 0
        lea     rbx, [ini_buf]          ; текущая позиция
        mov     r15d, [ini_len]
        lea     r15, [rbx + r15]        ; конец данных
        ; BOM UTF-8
        mov     eax, [ini_len]
        cmp     eax, 3
        jb      .line
        cmp     word [rbx], 0xBBEF
        jne     .line
        cmp     byte [rbx + 2], 0xBF
        jne     .line
        add     rbx, 3
  .line:
        cmp     rbx, r15
        jae     .not_found
        ; конец строки
        mov     rdi, rbx
  .eol:
        cmp     rdi, r15
        jae     .got_eol
        cmp     byte [rdi], 10
        je      .got_eol
        inc     rdi
        jmp     .eol
  .got_eol:
        mov     [line_end], rdi
        mov     rsi, rbx
        mov     rcx, rdi
        cmp     rcx, rsi
        jbe     .next
        cmp     byte [rcx - 1], 13      ; CRLF
        jne     @f
        dec     rcx
  @@:
        call    ini_trim
        cmp     rsi, rcx
        jae     .next                   ; пустая строка
        mov     al, [rsi]
        cmp     al, ';'
        je      .next
        cmp     al, '#'
        je      .next
        cmp     al, '['
        je      .section
        ; ключ = значение
        cmp     [in_sec], 0
        je      .next
        mov     r12, rsi                ; начало ключа
        mov     r13, rcx                ; конец строки
        mov     rdi, rsi
  .find_eq:
        cmp     rdi, r13
        jae     .next                   ; нет '='
        cmp     byte [rdi], '='
        je      .got_eq
        inc     rdi
        jmp     .find_eq
  .got_eq:
        mov     r14, rdi                ; позиция '='
        mov     rsi, r12
        mov     rcx, r14
        call    ini_trim
        mov     rdx, rcx
        sub     rdx, rsi
        mov     rcx, rsi
        mov     r8, [key_ptr]
        mov     r9d, [key_len]
        call    str_ieq
        test    eax, eax
        jz      .next
        lea     rsi, [r14 + 1]
        mov     rcx, r13
        call    ini_trim
        mov     rax, rsi
        mov     rdx, rcx
        sub     rdx, rsi
        ret
  .section:
        inc     rsi
        mov     rdi, rsi
  .find_close:
        cmp     rdi, rcx
        jae     .bad_section
        cmp     byte [rdi], ']'
        je      .got_close
        inc     rdi
        jmp     .find_close
  .got_close:
        mov     rcx, rdi
        call    ini_trim
        mov     rdx, rcx
        sub     rdx, rsi
        mov     rcx, rsi
        mov     r8, [sec_ptr]
        mov     r9d, [sec_len]
        call    str_ieq
        mov     [in_sec], eax
        jmp     .next
  .bad_section:
        mov     [in_sec], 0
  .next:
        mov     rbx, [line_end]
        inc     rbx
        jmp     .line
  .not_found:
        xor     eax, eax
        xor     edx, edx
        ret
endp
