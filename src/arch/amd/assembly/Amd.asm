option casemap:none

INCLUDE X86Entry.inc

EXTERN AmdVmExitHandler:PROC

HV_MAGIC_RAX EQU 031504F5453564E45h
HV_MAGIC_RCX EQU 0C0DEC0DE4E41454Ch
HV_MAGIC_RDX EQU 053544F504F4E4C59h
HV_MAGIC_R8  EQU 0A55A5AA5F00DCAFEh
HV_SLAT_R9   EQU 0534C4154464C5553h

VMCB_RIP_OFFSET EQU 0578h
VMCB_RSP_OFFSET EQU 05D8h
VMCB_CR4_OFFSET EQU 0548h
VMCB_CR3_OFFSET EQU 0550h
VMCB_CR0_OFFSET EQU 0558h
VMCB_DR7_OFFSET EQU 0560h
VMCB_DR6_OFFSET EQU 0568h
VMCB_CR2_OFFSET EQU 0640h

.code

AmdAsmLaunch PROC
    ; The fifth argument is the physical address of the host VMCB.
    mov r10, qword ptr [rsp + 40]
    mov r11, qword ptr [r9 + 16]
    mov qword ptr [r11 + 72], rsp
    lea rax, AmdLaunchFailed
    mov qword ptr [r11 + 80], rax
    mov qword ptr [rcx + VMCB_RSP_OFFSET], rsp
    lea rax, AmdGuestResume
    mov qword ptr [rcx + VMCB_RIP_OFFSET], rax

    ; Stable values live above the dedicated host stack pointer.
    mov qword ptr [r8], r9
    mov qword ptr [r8 + 8], rcx
    mov qword ptr [r8 + 16], rdx
    mov qword ptr [r8 + 24], r10

    mov rax, r10
    vmsave rax
    mov rsp, r8

AmdRunGuest:
    mov rax, qword ptr [rsp + 16]
    vmload rax
    mov rax, qword ptr [rsp + 16]
    stgi
    vmrun rax

    ; VMRUN restored the automatic host state.  Save the remaining guest
    ; state and restore the remaining host state before entering C.
    mov rax, qword ptr [rsp + 16]
    vmsave rax
    mov rax, qword ptr [rsp + 24]
    vmload rax

    X86_PUSH_GUEST_GPRS
    X86_SAVE_VOLATILE_XMM

    lea rcx, [rsp + X86_VOLATILE_XMM_SAVE_SIZE]
    mov rdx, qword ptr [rsp + X86_HANDLER_CPU_OFFSET]
    sub rsp, X86_HANDLER_CALL_FRAME_SIZE
    call AmdVmExitHandler
    add rsp, X86_HANDLER_CALL_FRAME_SIZE

    X86_RESTORE_VOLATILE_XMM
    cmp eax, 1
    je AmdShutdown

    X86_POP_GUEST_GPRS
    jmp AmdRunGuest

AmdShutdown:
    mov rdx, qword ptr [rsp + X86_GUEST_GPR_SAVE_SIZE]
    mov rdx, qword ptr [rdx + 16]
    mov r10, qword ptr [rdx + 72]
    mov r11, qword ptr [rdx + 80]

    ; VMEXIT restored launch-time host state. Reconstruct the current
    ; Windows state from the guest VMCB before devirtualizing this CPU.
    mov rax, qword ptr [rdx + 32]
    vmload rax
    mov rax, qword ptr [rdx]
    mov rcx, qword ptr [rax + VMCB_CR3_OFFSET]
    mov cr3, rcx
    mov rcx, qword ptr [rax + VMCB_CR4_OFFSET]
    mov cr4, rcx
    mov rcx, qword ptr [rax + VMCB_CR0_OFFSET]
    mov cr0, rcx
    mov rcx, qword ptr [rax + VMCB_CR2_OFFSET]
    mov cr2, rcx
    mov rcx, qword ptr [rax + VMCB_DR7_OFFSET]
    mov dr7, rcx
    mov rcx, qword ptr [rax + VMCB_DR6_OFFSET]
    mov dr6, rcx

    ; #VMEXIT clears GIF. EFER.SVME remains set until AmdStop resumes.
    stgi

    mov rbx, qword ptr [rsp + 24]
    mov rbp, qword ptr [rsp + 32]
    mov rsi, qword ptr [rsp + 40]
    mov rdi, qword ptr [rsp + 48]
    mov r12, qword ptr [rsp + 88]
    mov r13, qword ptr [rsp + 96]
    mov r14, qword ptr [rsp + 104]
    mov r15, qword ptr [rsp + 112]
    mov qword ptr [r10 - 8], r11
    lea rsp, [r10 - 8]
    ret

AmdGuestResume:
    xor eax, eax
    ret

AmdLaunchFailed:
    mov eax, 1
    ret
AmdAsmLaunch ENDP

AmdAsmStop PROC
    mov r9, rcx
    mov rax, HV_MAGIC_RAX
    mov rcx, HV_MAGIC_RCX
    mov rdx, HV_MAGIC_RDX
    mov r8,  HV_MAGIC_R8
    vmmcall
    ret
AmdAsmStop ENDP

AmdAsmSlatRendezvous PROC
    mov r9,  HV_SLAT_R9
    mov rax, HV_MAGIC_RAX
    mov rcx, HV_MAGIC_RCX
    mov rdx, HV_MAGIC_RDX
    mov r8,  HV_MAGIC_R8
    vmmcall
    ret
AmdAsmSlatRendezvous ENDP

AmdAsmReadEs PROC
    xor eax, eax
    mov ax, es
    ret
AmdAsmReadEs ENDP

AmdAsmReadCs PROC
    xor eax, eax
    mov ax, cs
    ret
AmdAsmReadCs ENDP

AmdAsmReadSs PROC
    xor eax, eax
    mov ax, ss
    ret
AmdAsmReadSs ENDP

AmdAsmReadDs PROC
    xor eax, eax
    mov ax, ds
    ret
AmdAsmReadDs ENDP

AmdAsmReadFs PROC
    xor eax, eax
    mov ax, fs
    ret
AmdAsmReadFs ENDP

AmdAsmReadGs PROC
    xor eax, eax
    mov ax, gs
    ret
AmdAsmReadGs ENDP

AmdAsmReadLdtr PROC
    xor eax, eax
    sldt ax
    ret
AmdAsmReadLdtr ENDP

AmdAsmReadTr PROC
    xor eax, eax
    str ax
    ret
AmdAsmReadTr ENDP

AmdAsmStoreGdtr PROC
    sgdt fword ptr [rcx]
    ret
AmdAsmStoreGdtr ENDP

AmdAsmStoreIdtr PROC
    sidt fword ptr [rcx]
    ret
AmdAsmStoreIdtr ENDP

END
