; =============================================================================
; engine/extract_cli.inc - CLI-обертка для извлечения текста
; =============================================================================

proc cmd_extract uses rbx rsi rdi r12 r13 r14 r15
  locals
    url_ptr   dq ?
    attr_n_p  dq ?
    attr_n_l  dq ?
    attr_v_p  dq ?
    attr_v_l  dq ?
    out_file  dq ?
    
    html_ptr  dq ?
    html_len  dq ?
    dom_root  dq ?
    ext_ptr   dq ?
    ext_len   dq ?
    
    bytes_w   dd ?
    hFile     dq ?
    wpath     db 1024 dup (?)
    
    arena_in  db 48 dup (?)
    arena_out db 48 dup (?)
  endl
        mov     ecx, 2
        call    cmdline_arg
        mov     [url_ptr], rax
        mov     ecx, 3
        call    cmdline_arg
        mov     [attr_n_p], rax
        mov     [attr_n_l], rdx
        mov     ecx, 4
        call    cmdline_arg
        mov     [attr_v_p], rax
        mov     [attr_v_l], rdx
        mov     ecx, 5
        call    cmdline_arg
        mov     [out_file], rax

        lea     rcx, [arena_in]
        mov     rdx, 1024*1024*10
        mov     r8d, 65536
        call    arena_init
        lea     rcx, [arena_out]
        mov     rdx, 1024*1024*10
        mov     r8d, 65536
        call    arena_init

        lea     rdx, [arena_in]
        mov     rcx, [url_ptr]
        call    http_get_arena
        test    rax, rax
        jz      .err
        mov     [html_ptr], rax
        mov     [html_len], rdx
        
        mov     rcx, rax
        mov     rdx, [html_len]
        lea     r8, [arena_in]
        call    html_tokenize
        test    rax, rax
        jz      .err
        
        mov     rcx, rax
        lea     rdx, [arena_in]
        call    dom_build
        test    rax, rax
        jz      .err
        mov     [dom_root], rax
        
        mov     rcx, [dom_root]
        mov     rdx, [attr_n_p]
        mov     r8, [attr_n_l]
        mov     r9, [attr_v_p]
        push    [attr_v_l]
        sub     rsp, 32
        call    dom_find_by_attr
        add     rsp, 40
        test    rax, rax
        jz      .err
        
        mov     rcx, rax
        lea     rdx, [arena_out]
        mov     r8, 1024*1024*10
        xor     r9d, r9d
        call    dom_extract_text
        mov     [ext_len], rax
        
        lea     rcx, [arena_out]
        xor     edx, edx
        call    arena_alloc
        mov     [ext_ptr], rax
        
        ; Пишем в файл
        mov     rcx, NW_CP_UTF8
        xor     edx, edx
        mov     r8, [out_file]
        mov     r9d, -1
        lea     rax, [wpath]
        push    512
        push    rax
        sub     rsp, 32
        call [MultiByteToWideChar]
        add     rsp, 48
        
        lea     rcx, [wpath]
        mov     edx, 0x40000000 ; GENERIC_WRITE
        xor     r8d, r8d
        xor     r9d, r9d
        push    0
        push    0x80 ; FILE_ATTRIBUTE_NORMAL
        push    2    ; CREATE_ALWAYS
        sub     rsp, 32
        invoke  CreateFileW, rcx, rdx, r8, r9
        add     rsp, 56
        cmp     rax, -1
        je      .err
        mov     [hFile], rax
        
        mov     rcx, [hFile]
        mov     rdx, [ext_ptr]
        mov     r8, [ext_len]
        lea     r9, [bytes_w]
        push    0
        sub     rsp, 32
        invoke  WriteFile, rcx, rdx, r8, r9, 0
        add     rsp, 40
        
        invoke  CloseHandle, [hFile]
        
        mov     eax, EXIT_OK
        ret
  .err:
        mov     eax, EXIT_ERROR
        ret
endp
