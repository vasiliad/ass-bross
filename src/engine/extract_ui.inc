proc ui_task_extract uses rbx rsi rdi r12 r13 r14 r15
  locals
    l_url  dq ?
    l_an   dq ?
    l_av   dq ?
    l_fol  dq ?
    
    html_ptr  dq ?
    html_len  dq ?
    dom_root  dq ?
    ext_ptr   dq ?
    ext_len   dq ?
    
    hFile     dq ?
    bytes_w   dd ?
    wpath     dq 512 dup (?)
    arena_in  dq 6 dup (?)
    arena_out dq 6 dup (?)
  endl
        mov     [l_url], rcx
        mov     [l_an], rdx
        mov     [l_av], r8
        mov     [l_fol], r9
        
        lea     rcx, [arena_in]
        mov     rdx, 1024*1024*10
        mov     r8d, 65536
        call    arena_init
        lea     rcx, [arena_out]
        mov     rdx, 1024*1024*10
        mov     r8d, 65536
        call    arena_init

        lea     rdx, [arena_in]
        lea     rcx, [url_u]
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
        lea     rdx, [attr_n_u]
        mov     r8, [l_an]
        lea     r9, [attr_v_u]
        push    [l_av]
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
        
        ; Combine folder and filename
        ; folder_w has the folder path in WCHAR. We just append "\\result.txt" to it.
        ; But wait, folder_w is globally available!
        lea     rsi, [szPath]
        lea     rdi, [wpath]
  .copy_folder:
        lodsw
        test    ax, ax
        jz      .append_slash
        stosw
        jmp     .copy_folder
  .append_slash:
        mov     ax, '\'
        stosw
        mov     ax, 'r'
        stosw
        mov     ax, 'e'
        stosw
        mov     ax, 's'
        stosw
        mov     ax, 'u'
        stosw
        mov     ax, 'l'
        stosw
        mov     ax, 't'
        stosw
        mov     ax, '.'
        stosw
        mov     ax, 't'
        stosw
        mov     ax, 'x'
        stosw
        mov     ax, 't'
        stosw
        xor     ax, ax
        stosw
        
        ; CreateFileW
        lea     rcx, [wpath]
        mov     edx, 0x40000000
        xor     r8d, r8d
        xor     r9d, r9d
        push    0
        push    0x80
        push    2
        sub     rsp, 32
        call    [CreateFileW]
        add     rsp, 56
        cmp     rax, -1
        je      .err
        mov     [hFile], rax
        
        ; WriteFile
        mov     rcx, [hFile]
        mov     rdx, [ext_ptr]
        mov     r8, [ext_len]
        lea     r9, [bytes_w]
        push    0
        sub     rsp, 32
        call    [WriteFile]
        add     rsp, 40
        
        ; CloseHandle
        mov     rcx, [hFile]
        sub     rsp, 32
        call    [CloseHandle]
        add     rsp, 32
        
        mov     eax, EXIT_OK
        ret
  .err:
        mov     eax, EXIT_ERROR
        ret
endp

; -----------------------------------------------------------------------------
proc ui_task_browse uses rbx rsi rdi r12 r13 r14 r15
  locals
    hwnd_browser dq ?
    html_ptr  dq ?
    html_len  dq ?
    dom_root  dq ?
    ext_ptr   dq ?
    ext_len   dq ?
    
    arena_in  dq 6 dup (?)
    arena_out dq 6 dup (?)
  endl
        mov     [hwnd_browser], rdx
        
        lea     rcx, [arena_in]
        mov     rdx, 1024*1024*10
        mov     r8d, 65536
        call    arena_init
        lea     rcx, [arena_out]
        mov     rdx, 1024*1024*10
        mov     r8d, 65536
        call    arena_init

        lea     rdx, [arena_in]
        lea     rcx, [url_u]
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
        
        lea     rcx, [arena_in]
        xor     edx, edx
        call    arena_alloc
        mov     r12, rax
        
        mov     rcx, NW_CP_UTF8
        xor     edx, edx
        mov     r8, [ext_ptr]
        mov     r9, [ext_len]
        push    0
        push    0
        push    1024*1024
        push    r12
        sub     rsp, 32
        call    [MultiByteToWideChar]
        add     rsp, 64
        
        mov     rcx, [hwnd_browser]
        mov     r8, r12
        mov     rdx, 0x000C
        sub     rsp, 32
        call    [SendMessageW]
        add     rsp, 32
        
        mov     eax, 0
        ret
  .err:
        mov     eax, 1
        ret
endp
