; =============================================================================
; core/file.inc — высокоуровневые функции работы с файлами.
; =============================================================================

; -----------------------------------------------------------------------------
; file_load_arena — загружает файл целиком в арену.
;   rcx = путь (UTF-16, с нулём), rdx = ARENA
;   Результат: eax = прочитано байт или -1 (ошибка). Указатель арены смещается.
; -----------------------------------------------------------------------------
proc file_load_arena uses rbx rsi rdi
        mov     rbx, rcx                ; path
        mov     rsi, rdx                ; arena
        
        ; Узнаём размер файла
        invoke  CreateFileW, rbx, NW_GENERIC_READ, NW_FILE_SHARE_READ, 0, NW_OPEN_EXISTING, NW_FILE_ATTRIBUTE_NORMAL, 0
        cmp     rax, NW_INVALID_HANDLE
        je      .fail
        mov     rdi, rax
        
        locals
          fsize dq ?
        endl
        invoke  GetFileSizeEx, rdi, addr fsize
        invoke  CloseHandle, rdi
        test    eax, eax
        jz      .fail
        
        mov     rdi, [fsize]
        cmp     rdi, 32 * 1024 * 1024   ; Ограничение 32 МБ для локальных файлов
        ja      .fail
        
        ; Резервируем память в арене
        mov     rcx, rsi
        mov     edx, edi
        call    arena_ensure
        
        ; Читаем
        mov     rcx, rbx
        mov     rdx, [rsi + ARENA.Ptr]
        mov     r8d, edi
        call    sys_file_read
        
        cmp     eax, -1
        je      .fail
        
        mov     edi, eax
        mov     rcx, rsi
        mov     edx, edi
        call    arena_advance
        
        mov     eax, edi
        ret
        
  .fail:
        mov     rax, -1
        ret
endp
