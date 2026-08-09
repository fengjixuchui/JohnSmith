option casemap:none

INCLUDE X86Entry.inc

EXTERN IntelSetLaunchState:PROC
EXTERN IntelVmExitHandler:PROC

HV_MAGIC_RAX EQU 031504F5453564E45h
HV_MAGIC_RCX EQU 0C0DEC0DE4E41454Ch
HV_MAGIC_RDX EQU 053544F504F4E4C59h
HV_MAGIC_R8  EQU 0A55A5AA5F00DCAFEh

VMCS_EXIT_REASON              EQU 04402h
VMCS_EXIT_INSTRUCTION_LENGTH  EQU 0440Ch
VMCS_GUEST_RIP                EQU 0681Eh
VMCS_PRIMARY_PROCESSOR_CONTROLS EQU 04002h

VMX_EXIT_CPUID                EQU 10
VMX_EXIT_RDTSC                EQU 16
VMX_EXIT_VMCALL               EQU 18
VMX_PRIMARY_RDTSC_EXITING     EQU 00001000h

HOST_FRAME_CPU_SLAT_GENERATION     EQU 8
HOST_FRAME_BACKEND_SLAT_GENERATION EQU 16
HOST_FRAME_CPUID_LEAF0_EAX         EQU 24
HOST_FRAME_CPUID_LEAF0_EBX         EQU 28
HOST_FRAME_CPUID_LEAF0_ECX         EQU 32
HOST_FRAME_CPUID_LEAF0_EDX         EQU 36
HOST_FRAME_FAST_PATH_ENABLED       EQU 40
HOST_FRAME_RENDEZVOUS_PHASE        EQU 48
HOST_FRAME_EXIT_ENTRY_TSC          EQU 56
HOST_FRAME_TSC_TRAP_ARMED          EQU 64
HOST_FRAME_LAST_GUEST_RDTSC        EQU 72
HOST_FRAME_TSC_CPUID_ENTRY_TSC     EQU 80
HOST_FRAME_BARE_METAL_CPUID_COST   EQU 88
INTEL_RENDEZVOUS_IDLE              EQU 0
TSC_STEALTH_MAX_PAIR_CYCLES        EQU 1000000

; INTEL_CPU_CONTEXT layout consumed by this file.  Compile-time asserts in
; include/johnsmith/Intel.h keeps the offsets in sync.
HV_CPU_VENDOR_CONTEXT         EQU 16
CPU_CONTEXT_GUEST_CR2         EQU 72
CPU_CONTEXT_PRIMARY_CONTROLS  EQU 12660

IFNDEF JOHNSMITH_VMEXIT_BENCHMARK
JOHNSMITH_VMEXIT_BENCHMARK EQU 0
ENDIF

.code

IntelAsmLaunch PROC
    mov rcx, rsp
    lea rdx, IntelGuestResume
    sub rsp, X86_HANDLER_CALL_FRAME_SIZE
    call IntelSetLaunchState
    add rsp, X86_HANDLER_CALL_FRAME_SIZE
    test eax, eax
    jnz IntelLaunchFailed
    vmlaunch
IntelLaunchFailed:
    mov eax, 1
    ret

IntelGuestResume:
    xor eax, eax
    ret
IntelAsmLaunch ENDP

IntelAsmStop PROC
    pushfq
    mov r9, rcx
    mov rax, HV_MAGIC_RAX
    mov rcx, HV_MAGIC_RCX
    mov rdx, HV_MAGIC_RDX
    mov r8,  HV_MAGIC_R8
    vmcall
    popfq
    ret
IntelAsmStop ENDP

IntelAsmInvept PROC
    invept rcx, oword ptr [rdx]
    ; VMX success clears CF/ZF; either failure class sets one of them.
    setbe al
    movzx eax, al
    ret
IntelAsmInvept ENDP

IntelAsmInvvpid PROC
    invvpid rcx, oword ptr [rdx]
    setbe al
    movzx eax, al
    ret
IntelAsmInvvpid ENDP

IntelAsmVmExit PROC
    ; Leaf-0 CPUID micropath: no C, no EPT flush, no rendezvous join.
    ; Guest GPRs still live in host registers on exit. Host RSP is the
    ; INTEL_HOST_STACK_FRAME (VMCS HOST_RSP). Fast path only when enabled
    ; and rendezvous is idle so SLAT/epoch work stays on the C path.
    push r8
    push r9

    cmp qword ptr [rsp + 16 + HOST_FRAME_FAST_PATH_ENABLED], 1
    jne IntelVmExitAfterFastProbe

    mov r8d, VMCS_EXIT_REASON
    vmread r9, r8
    jbe IntelVmExitAfterFastProbe
    and r9d, 0FFFFh
    cmp r9d, VMX_EXIT_CPUID
    jne IntelVmExitCheckRdtscFastPath

    ; Only leaf 0. Hypercall uses leaf 1 + magic subleaf → C path.
    test eax, eax
    jnz IntelVmExitAfterFastProbe

    mov r8, qword ptr [rsp + 16 + HOST_FRAME_RENDEZVOUS_PHASE]
    test r8, r8
    jz IntelVmExitAfterFastProbe
    cmp dword ptr [r8], INTEL_RENDEZVOUS_IDLE
    jne IntelVmExitAfterFastProbe

    ; Context generation is published only after the active EPT view is
    ; flushed. A mismatch must take the C path through IntelFlushEptIfNeeded.
    mov r8, qword ptr [rsp + 16 + HOST_FRAME_CPU_SLAT_GENERATION]
    test r8, r8
    jz IntelVmExitAfterFastProbe
    mov r9, qword ptr [rsp + 16 + HOST_FRAME_BACKEND_SLAT_GENERATION]
    test r9, r9
    jz IntelVmExitAfterFastProbe
    mov r8, qword ptr [r8]
    cmp r8, qword ptr [r9]
    jne IntelVmExitAfterFastProbe

    ; Preserve the guest values until every fallible VMCS operation has
    ; completed. CPUID overwrites RAX and RDX on success, so the fast path can
    ; use them as scratch after this point.
    push rax
    push rdx

    ; Trap-both model: arm the one-shot TSC trap so the next
    ; RDTSC/RDTSCP/RDMSR(IA32_TSC) exit returns (last guest RDTSC +
    ; bare-metal CPUID cost), collapsing the guest-visible
    ; RDTSC;CPUID;RDTSC probe delta to the bare instruction cost
    ; regardless of VM-exit latency.  The exit-entry TSC is captured here
    ; because the slow path that records LastExitEntryTsc is not reached
    ; for leaf 0.  CPUID is always 0F A2 (length 2); skip the
    ; EXIT_INSTRUCTION_LENGTH VMREAD.
    lfence
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r8, qword ptr [rsp + 32 + HOST_FRAME_EXIT_ENTRY_TSC]
    mov qword ptr [r8], rax
    mov r8, qword ptr [rsp + 32 + HOST_FRAME_TSC_CPUID_ENTRY_TSC]
    mov qword ptr [r8], rax
    mov r8, qword ptr [rsp + 32 + HOST_FRAME_TSC_TRAP_ARMED]
    mov dword ptr [r8], 1
    mov edx, VMCS_GUEST_RIP
    vmread r9, rdx
    jbe IntelVmExitLeaf0TscFallback
    add r9, 2
    vmwrite r9, rdx
    jbe IntelVmExitLeaf0TscFallback

    add rsp, 16

    mov eax, dword ptr [rsp + 16 + HOST_FRAME_CPUID_LEAF0_EAX]
    mov ebx, dword ptr [rsp + 16 + HOST_FRAME_CPUID_LEAF0_EBX]
    mov ecx, dword ptr [rsp + 16 + HOST_FRAME_CPUID_LEAF0_ECX]
    mov edx, dword ptr [rsp + 16 + HOST_FRAME_CPUID_LEAF0_EDX]

    pop r9
    pop r8
    vmresume
    ud2

IntelVmExitLeaf0TscFallback:
    pop rdx
    pop rax
    jmp IntelVmExitAfterFastProbe

; ---- RDTSC fast path (bounded trap-both model) ----
; TSC state is reached through direct pointers in the host frame. This avoids
; hard-coded offsets into the large CPU context and keeps the C fallback and
; assembly paths on the same canonical state.
IntelVmExitCheckRdtscFastPath:
    cmp r9d, VMX_EXIT_RDTSC
    jne IntelVmExitCheckBenchmarkVmcall

    mov r8, qword ptr [rsp + 16 + HOST_FRAME_RENDEZVOUS_PHASE]
    test r8, r8
    jz IntelVmExitAfterFastProbe
    cmp dword ptr [r8], INTEL_RENDEZVOUS_IDLE
    jne IntelVmExitAfterFastProbe

    mov r8, qword ptr [rsp + 16 + HOST_FRAME_CPU_SLAT_GENERATION]
    test r8, r8
    jz IntelVmExitAfterFastProbe
    mov r9, qword ptr [rsp + 16 + HOST_FRAME_BACKEND_SLAT_GENERATION]
    test r9, r9
    jz IntelVmExitAfterFastProbe
    mov r8, qword ptr [r8]
    cmp r8, qword ptr [r9]
    jne IntelVmExitAfterFastProbe

    mov r8, qword ptr [rsp + 16 + HOST_FRAME_TSC_TRAP_ARMED]
    test r8, r8
    jz IntelVmExitAfterFastProbe
    mov r9, qword ptr [rsp + 16 + HOST_FRAME_LAST_GUEST_RDTSC]
    test r9, r9
    jz IntelVmExitAfterFastProbe

    push rax
    push rdx
    lfence
    rdtsc
    shl rdx, 32
    or rax, rdx

    cmp dword ptr [r8], 0
    jz IntelVmExitRdtscPassthrough

    ; Preserve a pending pair across unrelated exits, as the original active
    ; HyperDbg state machine did, but reject stale cross-thread/vCPU state.
    mov rdx, qword ptr [rsp + 32 + HOST_FRAME_TSC_CPUID_ENTRY_TSC]
    test rdx, rdx
    jz IntelVmExitRdtscExpired
    mov r8, rax
    sub r8, qword ptr [rdx]
    cmp r8, TSC_STEALTH_MAX_PAIR_CYCLES
    ja IntelVmExitRdtscExpired

    mov rax, qword ptr [r9]
    add rax, qword ptr [rsp + 32 + HOST_FRAME_BARE_METAL_CPUID_COST]
    jnc IntelVmExitRdtscCompensated
    mov rax, 0FFFFFFFFFFFFFFFFh
IntelVmExitRdtscCompensated:
    mov qword ptr [r9], rax
    mov r8, qword ptr [rsp + 32 + HOST_FRAME_TSC_TRAP_ARMED]
    mov dword ptr [r8], 0
    jmp IntelVmExitRdtscAdvance

IntelVmExitRdtscExpired:
    mov r8, qword ptr [rsp + 32 + HOST_FRAME_TSC_TRAP_ARMED]
    mov dword ptr [r8], 0
IntelVmExitRdtscPassthrough:
    mov qword ptr [r9], rax

IntelVmExitRdtscAdvance:
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    mov r8d, VMCS_GUEST_RIP
    vmread r9, r8
    jbe IntelVmExitRdtscFallback
    add r9, 2
    vmwrite r9, r8
    jbe IntelVmExitRdtscFallback
    add rsp, 16
    pop r9
    pop r8
    vmresume
    ud2

IntelVmExitRdtscFallback:
    pop rdx
    pop rax
    jmp IntelVmExitAfterFastProbe

IntelVmExitCheckBenchmarkVmcall:
IF JOHNSMITH_VMEXIT_BENCHMARK
    push r10
    push r11

    cmp r9d, VMX_EXIT_VMCALL
    jne IntelVmExitBenchmarkProbeMiss
    mov r8, 04A534D5642454E43h
    cmp rax, r8
    jne IntelVmExitBenchmarkProbeMiss
    mov r8, 0484D41524B464C52h
    cmp rcx, r8
    jne IntelVmExitBenchmarkProbeMiss
    mov r8, 0564D43414C4C3031h
    cmp rdx, r8
    jne IntelVmExitBenchmarkProbeMiss
    mov r8, 0B16B00B5DEADC0DEh
    cmp qword ptr [rsp + 24], r8
    jne IntelVmExitBenchmarkProbeMiss
    mov r8, qword ptr [rsp + 32 + HOST_FRAME_RENDEZVOUS_PHASE]
    test r8, r8
    jz IntelVmExitBenchmarkProbeMiss
    cmp dword ptr [r8], INTEL_RENDEZVOUS_IDLE
    jne IntelVmExitBenchmarkProbeMiss
    ; Benchmark-only VMCALL: exit + RIP advance + VMRESUME, no C.
    mov r8d, VMCS_EXIT_INSTRUCTION_LENGTH
    vmread r11, r8
    jbe IntelVmExitBenchmarkProbeMiss
    mov r8d, VMCS_GUEST_RIP
    vmread r9, r8
    jbe IntelVmExitBenchmarkProbeMiss
    add r9, r11
    vmwrite r9, r8
    jbe IntelVmExitBenchmarkProbeMiss

    pop r11
    pop r10
    pop r9
    pop r8
    vmresume
    ud2

IntelVmExitBenchmarkProbeMiss:
    pop r11
    pop r10
ENDIF

IntelVmExitAfterFastProbe:
    pop r9
    pop r8

    ; Capture the exit-entry TSC before the CR2/GPR/XMM save prologue. RAX,
    ; RDX, and R8 are guest values here, so preserve them around RDTSC and
    ; write through the host-frame pointer initialized during VMCS setup.
    push rax
    push rdx
    push r8
    lfence
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r8, qword ptr [rsp + 24 + HOST_FRAME_EXIT_ENTRY_TSC]
    mov qword ptr [r8], rax
    pop r8
    pop rdx
    pop rax

    ; Snapshot guest CR2 before any host code path can fault and clobber it.
    ; VMX does not save or restore CR2, so host CR2 == guest CR2 on VM exit
    ; and stays that way until we run something that faults.  Fast path never
    ; touches CR2 and skips this save/restore.
    push rax
    push rcx
    mov rcx, qword ptr [rsp + 16]                ; host frame: HV_CPU*
    mov rcx, qword ptr [rcx + HV_CPU_VENDOR_CONTEXT]
    mov rax, cr2
    mov qword ptr [rcx + CPU_CONTEXT_GUEST_CR2], rax
    pop rcx
    pop rax

    ; VMCS host RSP points at the owning HV_CPU pointer.
    X86_PUSH_GUEST_GPRS
    X86_SAVE_VOLATILE_XMM

    lea rcx, [rsp + X86_VOLATILE_XMM_SAVE_SIZE]
    mov rdx, qword ptr [rsp + X86_HANDLER_CPU_OFFSET]
    sub rsp, X86_HANDLER_CALL_FRAME_SIZE
    call IntelVmExitHandler
    add rsp, X86_HANDLER_CALL_FRAME_SIZE

    X86_RESTORE_VOLATILE_XMM

    ; Restore guest CR2.  Handler may have overwritten context->GuestCr2 to
    ; inject a #PF; if not, the value flows back unchanged.  Both the
    ; resume and shutdown exits go through here.
    mov r10, qword ptr [rsp + X86_GUEST_GPR_SAVE_SIZE] ; host frame: HV_CPU*
    mov r10, qword ptr [r10 + HV_CPU_VENDOR_CONTEXT]
    mov r10, qword ptr [r10 + CPU_CONTEXT_GUEST_CR2]
    mov cr2, r10

    cmp eax, 1
    je IntelShutdown

    X86_POP_GUEST_GPRS
    vmresume
    ud2

IntelShutdown:
    mov rdx, qword ptr [rsp + X86_GUEST_GPR_SAVE_SIZE]
    mov rdx, qword ptr [rdx + 16]
    mov r10, qword ptr [rdx + 56]
    mov r11, qword ptr [rdx + 64]
    vmxoff

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
IntelAsmVmExit ENDP

IntelAsmReadEs PROC
    xor eax, eax
    mov ax, es
    ret
IntelAsmReadEs ENDP

IntelAsmReadCs PROC
    xor eax, eax
    mov ax, cs
    ret
IntelAsmReadCs ENDP

IntelAsmReadSs PROC
    xor eax, eax
    mov ax, ss
    ret
IntelAsmReadSs ENDP

IntelAsmReadDs PROC
    xor eax, eax
    mov ax, ds
    ret
IntelAsmReadDs ENDP

IntelAsmReadFs PROC
    xor eax, eax
    mov ax, fs
    ret
IntelAsmReadFs ENDP

IntelAsmReadGs PROC
    xor eax, eax
    mov ax, gs
    ret
IntelAsmReadGs ENDP

IntelAsmReadLdtr PROC
    xor eax, eax
    sldt ax
    ret
IntelAsmReadLdtr ENDP

IntelAsmReadTr PROC
    xor eax, eax
    str ax
    ret
IntelAsmReadTr ENDP

IntelAsmStoreGdtr PROC
    sgdt fword ptr [rcx]
    ret
IntelAsmStoreGdtr ENDP

IntelAsmStoreIdtr PROC
    sidt fword ptr [rcx]
    ret
IntelAsmStoreIdtr ENDP

IntelAsmLoadGdtr PROC
    lgdt fword ptr [rcx]
    ret
IntelAsmLoadGdtr ENDP

IntelAsmLoadIdtr PROC
    lidt fword ptr [rcx]
    ret
IntelAsmLoadIdtr ENDP

END
