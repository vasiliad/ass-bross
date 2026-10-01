; =============================================================================
; structs.inc — все структуры данных NanoWeb.
; Единственный источник истины по смещениям; документация — docs/MEMORY_MODEL.md.
; Размер каждой структуры проверяется при сборке (check_size в конце файла).
; =============================================================================

; Проверка размера структуры при сборке.
macro check_size name*, size* {
  if sizeof.#name <> size
    display 'structs.inc: size mismatch: ', `name, 13, 10
    err
  end if
}

; Проверка смещения поля при сборке.
macro check_offset field*, offset* {
  if field <> offset
    display 'structs.inc: offset mismatch: ', `field, 13, 10
    err
  end if
}

; --- 1.2. Заголовок арены ----------------------------------------------------
struct ARENA
  Base         dq ?
  Ptr          dq ?
  Commit_End   dq ?
  Reserve_End  dq ?
  Commit_Step  dd ?
  Flags        dd ?
ends

; --- 3. Контекст документа ---------------------------------------------------
struct DOC_CTX
  Raw             ARENA
  Src             ARENA
  Doc             ARENA
  Text            ARENA
  Layout          ARENA
  Line            ARENA
  Root            dq ?
  First_Image     dq ?
  First_Sheet     dq ?
  Rule_Buckets    dq ?
  Url_Ptr         dq ?
  Title_Ptr       dq ?
  Url_Len         dd ?
  Title_Len       dd ?
  Started_At      dq ?
  Last_Layout_At  dq ?
  Bytes_Received  dq ?
  In_Flight       dd ?
  State           dd ?
  Charset         dd ?
  Content_Class   dd ?
  Scroll_Y        dd ?
  Generation      dd ?
  Images_Total    dd ?
  Images_Done     dd ?
  Sheets_Pending  dd ?
  Vis_Width       dd ?
  Flags           dd ?
  Decoded_Upto    dd ?
  Tokenizer       dq ?
  Reserved        dq ?
ends

; --- 4. Узел DOM --------------------------------------------------------------
struct DOM_NODE
  Node_Type     dd ?
  Flags         dd ?
  Parent        dq ?
  First_Child   dq ?
  Last_Child    dq ?
  Next_Sibling  dq ?
  Tag_ID        dd ?
  Data_Len      dd ?
  Data_Ptr      dq ?
  First_Attr    dq ?
  Line_Index    dd ?
  Order         dd ?
ends

; --- 5. Атрибут ---------------------------------------------------------------
struct ATTR
  Next       dq ?
  Name_Ptr   dq ?
  Value_Ptr  dq ?
  Name_Len   dd ?
  Value_Len  dd ?
  Attr_ID    dd ?
  Reserved   dd ?
ends

; --- 6.1. Правило видимости ---------------------------------------------------
struct VIS_RULE
  Next         dq ?
  Selector     dq ?
  Order        dd ?
  Specificity  dd ?
  Effect       dd ?
  Flags        dd ?
  Media_Min_W  dd ?
  Media_Max_W  dd ?
ends

; --- 6.2. Часть селектора -----------------------------------------------------
struct SEL_PART
  Next        dq ?
  Tag_ID      dd ?
  Combinator  dd ?
  Id_Ptr      dq ?
  Id_Len      dd ?
  Class_Len   dd ?
  Class_Ptr   dq ?
ends

; --- 6.3. Таблица стилей ------------------------------------------------------
struct STYLESHEET
  Next          dq ?
  Fetch         dq ?
  Url_Ptr       dq ?
  Url_Len       dd ?
  State         dd ?
  Data          ARENA
  Source_Index  dd ?
  Media_Min_W   dd ?
  Media_Max_W   dd ?
  Import_Depth  dd ?
ends

; --- 7. Картинка --------------------------------------------------------------
struct IMAGE
  Next       dq ?
  Node       dq ?
  Url_Ptr    dq ?
  Url_Len    dd ?
  State      dd ?
  Data       ARENA
  Natural_W  dd ?
  Natural_H  dd ?
  Display_W  dd ?
  Display_H  dd ?
  Pixels     dq ?
  Fetch      dq ?
  Last_Used  dd ?
  Flags      dd ?
ends

; --- 8. Сетевой запрос --------------------------------------------------------
struct FETCH
  Request         dq ?
  Connect         dq ?
  Target          dq ?
  Owner           dq ?
  Doc             dq ?
  Kind            dd ?
  State           dd ?
  Seq             dd ?
  Generation      dd ?
  Http_Status     dd ?
  Content_Length  dd ?
  Bytes_Read      dd ?
  Win_Error       dd ?
  Secure_Flags    dd ?
  Closed          dd ?
  Last_Data_At    dq ?
  Read_Block      dd ?
  Reserved        dd ?
ends

; --- 9. Строка раскладки ------------------------------------------------------
struct LINE
  First_Fragment  dq ?
  Node            dq ?
  Y               dd ?
  Height          dd ?
  Baseline        dd ?
  X               dd ?
  Quote_Depth     dd ?
  Kind            dd ?
  Text_Start      dd ?
  Text_End        dd ?
ends

; --- 10. Фрагмент -------------------------------------------------------------
struct FRAGMENT
  Next      dq ?
  Data      dq ?
  Link      dq ?
  Text_Len  dd ?
  Font_ID   dd ?
  X         dd ?
  Width     dd ?
  Flags     dd ?
  Reserved  dd ?
ends

; --- 11. Запись истории -------------------------------------------------------
struct HISTORY_ENTRY
  Prev           dq ?
  Next           dq ?
  Url_Ptr        dq ?
  Title_Ptr      dq ?
  Url_Len        dd ?
  Title_Len      dd ?
  Anchor_Order   dd ?
  Scroll_Y       dd ?
  Cache          dq ?
  Content_Class  dd ?
  Reserved       dd ?
ends

; --- Проверки размеров (должны совпадать с docs/MEMORY_MODEL.md) -------------
check_size ARENA,          0x28
check_size DOC_CTX,        0x180
check_size DOM_NODE,       0x48
check_size ATTR,           0x28
check_size VIS_RULE,       0x28
check_size SEL_PART,       0x28
check_size STYLESHEET,     0x58
check_size IMAGE,          0x70
check_size FETCH,          0x60
check_size LINE,           0x30
check_size FRAGMENT,       0x30
check_size HISTORY_ENTRY,  0x40

; Выборочные смещения, на которые ссылается документация.
check_offset DOC_CTX.Root,           0xF0
check_offset DOC_CTX.Started_At,     0x128
check_offset DOC_CTX.In_Flight,      0x140
check_offset DOC_CTX.Tokenizer,      0x170
check_offset STYLESHEET.Data,        0x20
check_offset STYLESHEET.Source_Index, 0x48
check_offset IMAGE.Data,             0x20
check_offset IMAGE.Natural_W,        0x48
check_offset IMAGE.Pixels,           0x58
check_offset FETCH.Kind,             0x28
check_offset FETCH.Bytes_Read,       0x40
check_offset FETCH.Closed,           0x4C
check_offset FETCH.Last_Data_At,     0x50
check_offset LINE.Text_Start,        0x28
