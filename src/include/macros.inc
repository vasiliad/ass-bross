; =============================================================================
; macros.inc — общие макросы NanoWeb.
; =============================================================================

; --- Сбор данных модулей в секцию данных ---------------------------------------
; Модули объявляют данные рядом со своим кодом:
;     iglobal                     uglobal
;       msg db 'text'               buf rb 256
;     endg                        endg
; main.asm выводит их в секцию данных макросами IncludeIGlobals / IncludeUGlobals.
; Неинициализированные данные (uglobal) идут в конец секции и не занимают места в файле.

macro iglobal {
  IGlobals equ IGlobals,
  macro __IGlobalBlock { }

macro uglobal {
  UGlobals equ UGlobals,
  macro __UGlobalBlock { }

endg fix }

macro IncludeIGlobals {
  macro IGlobals dummy,[n] \{ __IGlobalBlock
    purge __IGlobalBlock \}
  match I, IGlobals \{ I \} }

macro IncludeUGlobals {
  macro UGlobals dummy,[n] \{
    \common
      \local begin, size
      begin = $
      virtual at $
    \forward
      __UGlobalBlock
      purge __UGlobalBlock
    \common
      size = $ - begin
      end virtual
      rb size
  \}
  match U, UGlobals \{ U \} }

; IGlobals/UGlobals намеренно не определяются заранее: первое `IGlobals equ IGlobals,`
; должно дать буквальный текст «IGlobals,», который затем вызывается как макрос.

; --- Строковые константы ---------------------------------------------------------
; sdef name, 'text', 10  →  name db 'text', 10  и  name.len = длина в байтах.
macro sdef name*, [text] {
  common
    name db text
    name#.len = $ - name
}

; Передать срез строки-константы в rcx/edx: slice s_name
macro slice name* {
  lea  rcx, [name]
  mov  edx, name#.len
}
