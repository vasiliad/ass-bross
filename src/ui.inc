; =============================================================================
; ui.inc - базовый графический интерфейс для настроек
; =============================================================================

WS_OVERLAPPEDWINDOW = 0x00CF0000
WS_VISIBLE          = 0x10000000
WS_CHILD            = 0x40000000
CBS_DROPDOWNLIST    = 0x0003
BS_PUSHBUTTON       = 0x0000
WM_COMMAND          = 0x0111
WM_DESTROY          = 0x0002
CB_ADDSTRING        = 0x0143
CB_SETCURSEL        = 0x014E
BIF_RETURNONLYFSDIRS= 0x0001
BIF_USENEWUI        = 0x0040

IDC_ARROW           = 32512




iglobal
  s_ui_class      db 'NanoWebUI', 0
  s_ui_title      db 'NanoWeb Export Settings', 0
  s_ui_combo      db 'COMBOBOX', 0
  s_ui_btn        db 'BUTTON', 0
  s_ui_btn_text   db 'Select Folder...', 0
  
  s_fmt_txt       db 'TXT Format', 0
  s_fmt_json      db 'JSON Format', 0
  s_fmt_csv       db 'CSV Format', 0
endg

uglobal
  hwndMain    dq ?
  hCombo      dq ?
  hButton     dq ?
  szPath      rb 260
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
        cmp     eax, 102        ; ID кнопки
        jne     .done
        
        ; Открываем выбор папки
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
        invoke  SHBrowseForFolderA, rcx
        test    rax, rax
        jz      .done
        
        invoke  SHGetPathFromIDListA, rax, addr szPath
        
        ; Выводим в дебаг
        invoke  OutputDebugStringA, addr szPath
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
; ui_show - создает и показывает окно
; -----------------------------------------------------------------------------
proc ui_show uses rbx rsi rdi
  locals
    wc  WNDCLASSEX
    msg MSG
  endl
        ; Заполняем WNDCLASSEX нулями
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
        
        ; Создаем главное окно
        invoke  GetModuleHandleW, 0
        mov     r10, rax
        
        sub     rsp, 32
        push    0
        push    r10
        push    0
        push    0
        push    200
        push    400
        push    100
        push    100
        mov     r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE
        lea     r8, [s_ui_title]
        lea     rdx, [s_ui_class]
        xor     ecx, ecx
        invoke  CreateWindowExA, rcx, rdx, r8, r9
        add     rsp, 32+64
        mov     [hwndMain], rax
        
        ; ComboBox
        sub     rsp, 32
        push    0
        push    r10 ; hInstance
        push    101 ; ID
        mov     rax, [hwndMain]
        push    rax ; hWndParent
        push    100
        push    150
        push    20
        push    20
        mov     r9d, WS_CHILD or WS_VISIBLE or CBS_DROPDOWNLIST
        xor     r8d, r8d
        lea     rdx, [s_ui_combo]
        xor     ecx, ecx
        invoke  CreateWindowExA, rcx, rdx, r8, r9
        add     rsp, 32+64
        mov     [hCombo], rax
        
        invoke  SendMessageA, [hCombo], CB_ADDSTRING, 0, addr s_fmt_txt
        invoke  SendMessageA, [hCombo], CB_ADDSTRING, 0, addr s_fmt_json
        invoke  SendMessageA, [hCombo], CB_ADDSTRING, 0, addr s_fmt_csv
        invoke  SendMessageA, [hCombo], CB_SETCURSEL, 0, 0
        
        ; Кнопка
        sub     rsp, 32
        push    0
        push    r10
        push    102
        mov     rax, [hwndMain]
        push    rax
        push    30
        push    150
        push    20
        push    190
        mov     r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
        lea     r8, [s_ui_btn_text]
        lea     rdx, [s_ui_btn]
        xor     ecx, ecx
        invoke  CreateWindowExA, rcx, rdx, r8, r9
        add     rsp, 32+64
        mov     [hButton], rax
        
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
