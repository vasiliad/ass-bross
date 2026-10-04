; =============================================================================
; include/proc.inc — пролог/эпилог процедур с таблицей раскрутки стека x64.
;
; Кадр такой же, как у стандартного prologuedef из proc64.inc FASM (push rbp;
; mov rbp,rsp; sub rsp,N; push регистры uses), поэтому поведение кода не меняется.
; Добавлено: после тела каждой процедуры лежит UNWIND_INFO (прямо в .text — данным
; нужен лишь RVA), а в таблицу .pdata попадает запись RUNTIME_FUNCTION.
;
; Коды раскрутки (в порядке убывания смещения в прологе):
;   push uses-регистров (в обратном порядке) → sub rsp,N → mov rbp,rsp → push rbp.
;
; Процедуры, которые FASM отбрасывает (`if used`), в .pdata не попадают: запись
; охраняется `if defined`. Побочный эффект: ссылка из .pdata считается использованием,
; поэтому неиспользуемые процедуры остаются в образе.
; Записи накапливаются в порядке, обратном адресам; tools/patch_pe.py сортирует их.
; =============================================================================

; Номера регистров в кодах раскрутки (UWOP_PUSH_NONVOL)
nw_ureg_rax = 0
nw_ureg_rcx = 1
nw_ureg_rdx = 2
nw_ureg_rbx = 3
nw_ureg_rsp = 4
nw_ureg_rbp = 5
nw_ureg_rsi = 6
nw_ureg_rdi = 7
nw_ureg_r8  = 8
nw_ureg_r9  = 9
nw_ureg_r10 = 10
nw_ureg_r11 = 11
nw_ureg_r12 = 12
nw_ureg_r13 = 13
nw_ureg_r14 = 14
nw_ureg_r15 = 15

; --- Накопление записей .pdata (приём iglobal: список-эквивалент и стек макросов) ----
macro nw_pdata_add name* {
  NwPData equ NwPData,
  macro __NwPDataBlock \{
    if defined name#.__uw
      dd rva name, rva name#.__end, rva name#.__uw
    end if
  \}
}

macro NwEmitPData {
  macro NwPData dummy,[n] \{ __NwPDataBlock
    purge __NwPDataBlock \}
  match P, NwPData \{ P \} }

; --- Пролог: как prologuedef, плюс метки для UNWIND_INFO ---------------------------
macro nw_prologue procname,flag,parmbytes,localbytes,reglist
{ local loc,fill,counter
  loc = (localbytes+15) and (not 15)
  parmbase@proc equ rbp+16
  localbase@proc equ rbp-loc
  push rbp
  procname#.__u1:
  mov rbp,rsp
  procname#.__u2:
  if loc+fill
   sub rsp,loc+fill
  end if
  procname#.__u3:
  counter = 0
  irps reg, reglist \{ push reg
                       procname#.__pu_\#reg:
                       counter = counter+1 \}
  procname#.__uend:
  procname#.__n = counter
  procname#.__a = loc+fill
  fill = 8*(counter and 1)
}

macro nw_epilogue procname,flag,parmbytes,localbytes,reglist
{ irps reg, reglist \{ reverse pop reg \}
  leave
  retn }

; --- Закрытие: UNWIND_INFO после тела процедуры и регистрация в .pdata -------------
macro nw_close procname,flag,parmbytes,localbytes,reglist
{ procname#.__end:
  align 4
  procname#.__uw:
  if procname#.__a = 0
    procname#.__s = 0
  else if procname#.__a <= 128
    procname#.__s = 1
  else if procname#.__a <= 0x7FFF8
    procname#.__s = 2
  else
    procname#.__s = 3
  end if
  db 1                                  ; Version = 1, Flags = 0
  db procname#.__uend - procname        ; размер пролога
  db procname#.__n + procname#.__s + 2  ; число слотов кодов
  db 5                                  ; регистр кадра rbp, смещение 0
  irps reg, reglist \{ reverse db procname#.__pu_\#reg - procname, nw_ureg_\#reg shl 4 \}
  if procname#.__s = 1
    db procname#.__u3 - procname, 2 + (((procname#.__a - 8) / 8) shl 4)   ; ALLOC_SMALL
  else if procname#.__s = 2
    db procname#.__u3 - procname, 1                                       ; ALLOC_LARGE (16 бит)
    dw procname#.__a / 8
  else if procname#.__s = 3
    db procname#.__u3 - procname, 0x11                                    ; ALLOC_LARGE (32 бита)
    dd procname#.__a
  end if
  db procname#.__u2 - procname, 3                                         ; SET_FPREG
  db procname#.__u1 - procname, 5 shl 4                                   ; PUSH_NONVOL rbp
  if (procname#.__n + procname#.__s + 2) and 1
    dw 0                                ; массив кодов выравнивается до чётного числа слотов
  end if
  nw_pdata_add procname
}

prologue@proc equ nw_prologue
epilogue@proc equ nw_epilogue
close@proc equ nw_close
