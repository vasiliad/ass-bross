; =============================================================================
; consts.inc — константы NanoWeb (флаги, коды, лимиты).
; Значения лимитов — docs/ARCHITECTURE.md, раздел 12.
; =============================================================================

NANOWEB_VERSION equ '0.0.1-m0'

; --- Сообщения окна -----------------------------------------------------------
WM_APP_BASE         = 0x8000
WM_NET              = WM_APP_BASE + 1

; --- Коды завершения процесса -------------------------------------------------
EXIT_OK             = 0
EXIT_USAGE          = 2
EXIT_ERROR          = 3
EXIT_NOT_FOUND      = 4

; --- Коды ошибок (last_error) -------------------------------------------------
ERR_NONE            = 0
ERR_OOM             = 1
ERR_IO              = 2
ERR_LIMIT           = 3
ERR_NOT_FOUND       = 4

; --- ARENA.Flags --------------------------------------------------------------
AF_NET_TARGET       = 1 shl 0

; --- DOM_NODE.Node_Type / Flags -----------------------------------------------
NT_ELEMENT          = 0
NT_TEXT             = 1
NT_COMMENT          = 2
NT_DOCUMENT         = 3

NF_WHITESPACE_ONLY  = 1 shl 0
NF_VOID             = 1 shl 1
NF_HIDDEN           = 1 shl 2
NF_HIDDEN_ATTR      = 1 shl 3
NF_HAS_ID           = 1 shl 4

; --- DOC_CTX ------------------------------------------------------------------
DS_FREE             = 0
DS_LOADING          = 1
DS_READY            = 2
DS_CLOSING          = 3

DF_SHOWN            = 1 shl 0
DF_STOPPED          = 1 shl 1
DF_CHARSET_RESTART  = 1 shl 2
DF_CHARSET_FIXED    = 1 shl 3

CC_HTML             = 0
CC_TEXT             = 1
CC_IMAGE            = 2
CC_INTERNAL         = 3

CS_UTF8             = 0
CS_1251             = 1
CS_1252             = 2
CS_KOI8R            = 3

; --- FETCH --------------------------------------------------------------------
FK_DOCUMENT         = 0
FK_STYLESHEET       = 1
FK_IMAGE            = 2

FS_FREE             = 0
FS_SENDING          = 1
FS_RECEIVING        = 2
FS_DONE             = 3
FS_ERROR            = 4
FS_CLOSING          = 5

FETCH_POOL_SIZE     = 48
DOC_POOL_SIZE       = 4

; --- VIS_RULE / SEL_PART ------------------------------------------------------
VE_HIDE             = 0
VE_SHOW             = 1
VRF_IMPORTANT       = 1 shl 0

SC_NONE             = 0
SC_DESCENDANT       = 1
SC_CHILD            = 2
SC_SAME             = 3

; --- LINE.Kind / FRAGMENT.Flags -----------------------------------------------
LK_TEXT             = 0
LK_HR               = 1
LK_IMAGE            = 2

FR_LINK             = 1 shl 0
FR_UNDERLINE        = 1 shl 1
FR_STRIKE           = 1 shl 2
FR_IMAGE            = 1 shl 3
FR_MUTED            = 1 shl 4
FR_MARKER           = 1 shl 5

; --- IMAGE --------------------------------------------------------------------
IS_PENDING          = 0
IS_LOADING          = 1
IS_LOADED           = 2
IS_DECODED          = 3
IS_ERROR            = 4
IS_SKIPPED          = 5

IMG_SIZE_KNOWN      = 1 shl 0
IMG_INLINE          = 1 shl 1
IMG_DATA_URI        = 1 shl 2

; --- Лимиты (ARCHITECTURE.md, раздел 12) --------------------------------------
LIM_RAW_BYTES       = 32 shl 20
LIM_SRC_BYTES       = 32 shl 20
LIM_DOM_DEPTH       = 512
LIM_DOM_NODES       = 2000000
LIM_ATTRS_PER_ELEM  = 256
LIM_STYLESHEETS     = 8
LIM_STYLESHEET_SIZE = 512 shl 10
LIM_CSS_SOURCES     = 256
LIM_CSS_IMPORTS     = 14
LIM_IMPORT_DEPTH    = 4
LIM_VIS_RULES       = 50000
LIM_SEL_PARTS       = 32
LIM_IMAGES          = 1000
LIM_IMAGE_FILE      = 16 shl 20
LIM_IMAGE_MPX       = 20000000
LIM_IMAGE_MPX_JPEG  = 100000000
LIM_IMAGE_SIDE      = 16384
LIM_DATA_URI        = 1 shl 20
LIM_SIMD_TAIL       = 32

; --- Арены: шаги фиксации -----------------------------------------------------
ARENA_STEP_DOC      = 64 shl 10
ARENA_STEP_RESOURCE = 4 shl 10
