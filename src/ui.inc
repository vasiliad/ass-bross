; =============================================================================
; ui.inc - графический интерфейс (универсальный экстрактор)
; =============================================================================

WS_OVERLAPPEDWINDOW = 0x00CF0000
WS_VISIBLE          = 0x10000000
WS_CHILD            = 0x40000000
WS_BORDER           = 0x00800000
ES_AUTOHSCROLL      = 0x00000080
CBS_DROPDOWNLIST    = 0x0003
BS_PUSHBUTTON       = 0x0000
WM_COMMAND          = 0x0111
WM_DESTROY          = 0x0002
WM_SETTEXT          = 0x000C
CB_ADDSTRING        = 0x0143
CB_SETCURSEL        = 0x014E
BIF_RETURNONLYFSDIRS= 0x0001
BIF_USENEWUI        = 0x0040

IDC_ARROW           = 32512

iglobal
  msg_no_folder du 'Please select an output folder first!',0
  msg_success   du 'Extraction completed successfully! Saved to result.txt',0
  msg_err       du 'Extraction failed! Check URL or network connection.',0
  msg_title_ok  du 'Success',0
  msg_title_err du 'Error',0
  s_ui_class      db 'NanoWebUI', 0
  s_ui_title      db 'NanoWeb Universal Extractor', 0
  
  s_cls_combo     db 'COMBOBOX', 0
  s_cls_btn       db 'BUTTON', 0
  s_cls_static    db 'STATIC', 0
  s_cls_edit      db 'EDIT', 0
  
  s_lbl_url       db 'Target URL:', 0
  s_lbl_attr_n    db 'Attribute Name:', 0
  s_lbl_attr_v    db 'Attribute Value:', 0
  
  s_btn_folder    db 'Select Output Folder...', 0
  s_btn_run       db 'RUN EXTRACTION', 0
  
  s_fmt_txt       db 'TXT Format', 0
  s_fmt_json      db 'JSON Format', 0
  s_fmt_csv       db 'CSV Format', 0
  
  s_def_url       db 'https://example.com', 0
  s_def_attr_n    db 'data-qa', 0
  s_def_attr_v    db 'content', 0
endg

uglobal
  hwndMain    dq ?
  hEditUrl    dq ?
  hEditAttrN  dq ?
  hEditAttrV  dq ?
  hComboFmt   dq ?
  hBtnFolder  dq ?
  hLblFolder  dq ?
  hBtnRun     dq ?
    szPath      rb 1024
  
  url_w       rb 1024
  attr_n_w    rb 1024
  attr_v_w    rb 1024
  folder_w    rb 1024
  
  url_u       rb 4096
  attr_n_u    rb 4096
  attr_v_u    rb 4096
  folder_u    rb 4096
  
  ui_hFile    dq ?
endg

; -----------------------------------------------------------------------------
; ui_window_proc - обработчик сообщений
; -----------------------------------------------------------------------------
proc ui_window_proc hwnd, wmsg, wparam, lparam
  locals
    bi BROWSEINFO
  endl
        mov     [hwnd], rcx
        mov     [wmsg], rdx
        mov     [wparam], r8
        mov     [lparam], r9
        
        cmp     rdx, WM_COMMAND
        je      .wm_command
        cmp     rdx, WM_DESTROY
        je      .wm_destroy
        
  .def:
        invoke  DefWindowProcA, rcx, rdx, r8, r9
        ret
        
  .wm_command:
        mov     eax, r8d
        and     eax, 0xFFFF
        
        cmp     eax, 105        ; ID кнопки выбора папки
        je      .cmd_folder
        cmp     eax, 107        ; ID кнопки RUN
        je      .cmd_run
        jmp     .done
        
  .cmd_folder:
        lea     rcx, [bi]
        mov     rdx, sizeof.BROWSEINFO
        xor     eax, eax
        mov     rdi, rcx
        mov     rcx, rdx
        rep     stosb
        
        mov     rax, [hwnd]
        mov     [bi.hwndOwner], rax
        mov     [bi.ulFlags], BIF_RETURNONLYFSDIRS or BIF_USENEWUI
        
        lea     rcx, [bi]
        invoke  SHBrowseForFolderW, rcx
        test    rax, rax
        jz      .done
        
        invoke  SHGetPathFromIDListW, rax, addr szPath
        
        ; Обновляем текст метки папки
        invoke  SendMessageW, [hLblFolder], WM_SETTEXT, 0, addr szPath
        jmp     .done
        
  .cmd_run:
        mov     rcx, [hEditUrl]
        lea     rdx, [url_w]
        mov     r8d, 512
        sub     rsp, 32
        call    [GetWindowTextW]
        add     rsp, 32
        
        mov     rcx, [hEditAttrN]
        lea     rdx, [attr_n_w]
        mov     r8d, 512
        sub     rsp, 32
        call    [GetWindowTextW]
        add     rsp, 32
        
        mov     rcx, [hEditAttrV]
        lea     rdx, [attr_v_w]
        mov     r8d, 512
        sub     rsp, 32
        call    [GetWindowTextW]
        add     rsp, 32
        
        mov     rcx, [hLblFolder]
        lea     rdx, [szPath]
        mov     r8d, 512
        sub     rsp, 32
        call    [GetWindowTextW]
        add     rsp, 32
        
        ; Convert URL
        mov     rcx, NW_CP_UTF8
        xor     edx, edx
        lea     r8, [url_w]
        mov     r9d, -1
        lea     rax, [url_u]
        push    0
        push    0
        push    4096
        push    rax
        sub     rsp, 32
        call    [WideCharToMultiByte]
        add     rsp, 64
        mov     r12, rax
        dec     r12
        
        ; Convert Attr Name
        mov     rcx, NW_CP_UTF8
        xor     edx, edx
        lea     r8, [attr_n_w]
        mov     r9d, -1
        lea     rax, [attr_n_u]
        push    0
        push    0
        push    4096
        push    rax
        sub     rsp, 32
        call    [WideCharToMultiByte]
        add     rsp, 64
        mov     r13, rax
        dec     r13
        
        ; Convert Attr Value
        mov     rcx, NW_CP_UTF8
        xor     edx, edx
        lea     r8, [attr_v_w]
        mov     r9d, -1
        lea     rax, [attr_v_u]
        push    0
        push    0
        push    4096
        push    rax
        sub     rsp, 32
        call    [WideCharToMultiByte]
        add     rsp, 64
        mov     r14, rax
        dec     r14
        
        ; Convert Folder
        mov     rcx, NW_CP_UTF8
        xor     edx, edx
        lea     r8, [szPath]
        mov     r9d, -1
        lea     rax, [folder_u]
        push    0
        push    0
        push    4096
        push    rax
        sub     rsp, 32
        call    [WideCharToMultiByte]
        add     rsp, 64
        mov     r15, rax
        dec     r15
        
        ; Check if szPath is empty
        lea     rax, [szPath]
        cmp     word [rax], 0
        jne     .do_extract
        
        ; Show error if empty
        mov     rcx, [hwnd]
        lea     rdx, [msg_no_folder]
        lea     r8, [msg_title_err]
        mov     r9d, 0x10 ; MB_ICONERROR
        sub     rsp, 32
        call    [MessageBoxW]
        add     rsp, 32
        jmp     .done
        
  .do_extract:
        ; Call engine
        mov     rcx, r12
        mov     rdx, r13
        mov     r8, r14
        mov     r9, r15
        call    ui_task_extract
        
        cmp     eax, 0 ; EXIT_OK
        jne     .show_err
        
        mov     rcx, [hwnd]
        lea     rdx, [msg_success]
        lea     r8, [msg_title_ok]
        mov     r9d, 0x40 ; MB_ICONINFORMATION
        sub     rsp, 32
        call    [MessageBoxW]
        add     rsp, 32
        jmp     .done
        
  .show_err:
        mov     rcx, [hwnd]
        lea     rdx, [msg_err]
        lea     r8, [msg_title_err]
        mov     r9d, 0x10 ; MB_ICONERROR
        sub     rsp, 32
        call    [MessageBoxW]
        add     rsp, 32
        jmp     .done
        
  .wm_destroy:
        invoke  PostQuitMessage, 0
        xor     eax, eax
        ret
        
  .done:
        xor     eax, eax
        ret
endp

; -----------------------------------------------------------------------------
; ui_show - создает и показывает главное окно
; -----------------------------------------------------------------------------
proc ui_show uses rbx rsi rdi r12 r13
  locals
    wc  WNDCLASSEX
    msg MSG
  endl
        lea     rdi, [wc]
        mov     ecx, sizeof.WNDCLASSEX
        xor     eax, eax
        rep     stosb
        
        mov     [wc.cbSize], sizeof.WNDCLASSEX
        lea     rax, [ui_window_proc]
        mov     [wc.lpfnWndProc], rax
        
        invoke  GetModuleHandleW, 0
        mov     [wc.hInstance], rax
        
        invoke  LoadCursorA, 0, IDC_ARROW
        mov     [wc.hCursor], rax
        
        mov     [wc.hbrBackground], 5 ; COLOR_WINDOW
        lea     rax, [s_ui_class]
        mov     [wc.lpszClassName], rax
        
        lea     rcx, [wc]
        invoke  RegisterClassExA, rcx
        
        invoke  GetModuleHandleW, 0
        mov     r13, rax ; hInstance
        
        ; Главное окно
        sub     rsp, 32
        push    0
        push    r13
        push    0
        push    0
        push    350      ; height
        push    500      ; width
        push    100      ; y
        push    100      ; x
        mov     r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE
        lea     r8, [s_ui_title]
        lea     rdx, [s_ui_class]
        xor     ecx, ecx
        invoke  CreateWindowExA, rcx, rdx, r8, r9
        add     rsp, 32+64
        mov     [hwndMain], rax
        mov     r12, rax ; hwndMain
        
        ; URL Label & Edit
        invoke  CreateWindowExA, 0, addr s_cls_static, addr s_lbl_url, WS_CHILD or WS_VISIBLE, \
                20, 20, 100, 20, r12, 0, r13, 0
        invoke  CreateWindowExA, 0, addr s_cls_edit, addr s_def_url, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_AUTOHSCROLL, \
                130, 20, 330, 24, r12, 101, r13, 0
        mov     [hEditUrl], rax

        ; Attr Name Label & Edit
        invoke  CreateWindowExA, 0, addr s_cls_static, addr s_lbl_attr_n, WS_CHILD or WS_VISIBLE, \
                20, 60, 100, 20, r12, 0, r13, 0
        invoke  CreateWindowExA, 0, addr s_cls_edit, addr s_def_attr_n, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_AUTOHSCROLL, \
                130, 60, 100, 24, r12, 102, r13, 0
        mov     [hEditAttrN], rax

        ; Attr Value Label & Edit
        invoke  CreateWindowExA, 0, addr s_cls_static, addr s_lbl_attr_v, WS_CHILD or WS_VISIBLE, \
                240, 60, 100, 20, r12, 0, r13, 0
        invoke  CreateWindowExA, 0, addr s_cls_edit, addr s_def_attr_v, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_AUTOHSCROLL, \
                350, 60, 110, 24, r12, 103, r13, 0
        mov     [hEditAttrV], rax

        ; Format Combo
        invoke  CreateWindowExA, 0, addr s_cls_combo, 0, WS_CHILD or WS_VISIBLE or CBS_DROPDOWNLIST, \
                20, 110, 150, 100, r12, 104, r13, 0
        mov     [hComboFmt], rax
        invoke  SendMessageA, [hComboFmt], CB_ADDSTRING, 0, addr s_fmt_txt
        invoke  SendMessageA, [hComboFmt], CB_ADDSTRING, 0, addr s_fmt_json
        invoke  SendMessageA, [hComboFmt], CB_ADDSTRING, 0, addr s_fmt_csv
        invoke  SendMessageA, [hComboFmt], CB_SETCURSEL, 0, 0

        ; Folder Button & Label
        invoke  CreateWindowExA, 0, addr s_cls_btn, addr s_btn_folder, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
                190, 110, 270, 24, r12, 105, r13, 0
        mov     [hBtnFolder], rax
        invoke  CreateWindowExA, 0, addr s_cls_static, 0, WS_CHILD or WS_VISIBLE, \
                20, 150, 440, 40, r12, 106, r13, 0
        mov     [hLblFolder], rax

        ; RUN Button
        invoke  CreateWindowExA, 0, addr s_cls_btn, addr s_btn_run, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
                20, 210, 440, 40, r12, 107, r13, 0
        mov     [hBtnRun], rax
        
  .msg_loop:
        lea     rcx, [msg]
        invoke  GetMessageA, rcx, 0, 0, 0
        test    eax, eax
        jle     .msg_done
        
        lea     rcx, [msg]
        invoke  TranslateMessage, rcx
        lea     rcx, [msg]
        invoke  DispatchMessageA, rcx
        jmp     .msg_loop
        
  .msg_done:
        ret
endp
