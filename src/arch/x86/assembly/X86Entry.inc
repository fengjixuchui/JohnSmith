; Shared x64 entry-frame helpers.
;
; The push order matches the guest-register structures consumed by the C
; handlers. XMM0-XMM5 are saved because the Windows x64 ABI permits a C
; function to overwrite them while a VM exit must preserve guest state.

IFNDEF X86_ENTRY_INC
X86_ENTRY_INC EQU 1

X86_GUEST_GPR_SAVE_SIZE       EQU 078h
X86_VOLATILE_XMM_SAVE_SIZE    EQU 060h
X86_HANDLER_CALL_FRAME_SIZE   EQU 028h
X86_HANDLER_CPU_OFFSET        EQU (X86_GUEST_GPR_SAVE_SIZE + X86_VOLATILE_XMM_SAVE_SIZE)

X86_PUSH_GUEST_GPRS MACRO
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax
ENDM

X86_POP_GUEST_GPRS MACRO
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
ENDM

X86_SAVE_VOLATILE_XMM MACRO
    sub rsp, X86_VOLATILE_XMM_SAVE_SIZE
    movdqu xmmword ptr [rsp + 00h], xmm0
    movdqu xmmword ptr [rsp + 10h], xmm1
    movdqu xmmword ptr [rsp + 20h], xmm2
    movdqu xmmword ptr [rsp + 30h], xmm3
    movdqu xmmword ptr [rsp + 40h], xmm4
    movdqu xmmword ptr [rsp + 50h], xmm5
ENDM

X86_RESTORE_VOLATILE_XMM MACRO
    movdqu xmm0, xmmword ptr [rsp + 00h]
    movdqu xmm1, xmmword ptr [rsp + 10h]
    movdqu xmm2, xmmword ptr [rsp + 20h]
    movdqu xmm3, xmmword ptr [rsp + 30h]
    movdqu xmm4, xmmword ptr [rsp + 40h]
    movdqu xmm5, xmmword ptr [rsp + 50h]
    add rsp, X86_VOLATILE_XMM_SAVE_SIZE
ENDM

ENDIF
