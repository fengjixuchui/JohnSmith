option casemap:none

; Owned cold target for end-to-end hook validation. The first 21 bytes must
; remain compatible with the conservative decoder in HookTrampoline.c; RET
; starts immediately after that copied window.

HOOK_PROBE_PREFIX_SIZE EQU 3
HOOK_PROBE_PATCH_SIZE  EQU 21

PUBLIC JohnSmithHookProbeTarget

_HOOKPROBE SEGMENT ALIGN(4096) READ EXECUTE NOPAGE ALIAS(".hprobe") 'CODE'

JohnSmithHookProbeTarget PROC
    DB      048h, 089h, 0C8h            ; mov rax, rcx
    REPT (HOOK_PROBE_PATCH_SIZE - HOOK_PROBE_PREFIX_SIZE)
        nop
    ENDM
    ret
JohnSmithHookProbeTarget ENDP

_HOOKPROBE ENDS

END
