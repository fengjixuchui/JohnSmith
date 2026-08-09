# Assembly ABI contracts

This document records the interfaces shared by the x64 assembly entry paths
and C. It covers `Intel.asm`, `Amd.asm`, `HookDispatch.asm`, `HookProbe.asm`,
and the shared `X86Entry.inc` helpers.

## VM-exit handler frame

Intel and AMD use the same general-purpose and volatile-SIMD save frame before
calling their C exit handlers. `X86Entry.inc` owns that byte-for-byte common
sequence.

| Region | Size | Purpose |
| --- | ---: | --- |
| Volatile XMM save | `0x60` | Preserves XMM0-XMM5 across the C call |
| Guest GPR save | `0x78` | Materializes the C guest-register structure |
| Handler call frame | `0x28` | 32-byte home area plus call-site alignment |

After the XMM save, the guest-register structure begins at `[rsp+0x60]` and
the owning `HV_CPU*` is at `[rsp+0xD8]`. The C definitions in `Intel.h` and
`Amd.h` assert the assembly-visible layout.

| Offset | Register | Offset | Register |
| ---: | --- | ---: | --- |
| `0x00` | RAX | `0x40` | R9 |
| `0x08` | RCX | `0x48` | R10 |
| `0x10` | RDX | `0x50` | R11 |
| `0x18` | RBX | `0x58` | R12 |
| `0x20` | RBP | `0x60` | R13 |
| `0x28` | RSI | `0x68` | R14 |
| `0x30` | RDI | `0x70` | R15 |
| `0x38` | R8 |  |  |

The Windows x64 ABI allows a C function to overwrite XMM0-XMM5. Both VM-exit
paths therefore preserve those registers even though normal kernel calls do
not require the caller to do so.

## Intel VMX entry

VMCS host RSP points to an `INTEL_HOST_STACK_FRAME`. Fast paths may use R8 and
R9 only after saving their guest values. Any failed or ineligible fast probe
falls through to the common slow path, which captures the exit-entry TSC and
guest CR2 before entering C.

`INVEPT` and `INVVPID` report success with CF=0 and ZF=0. Their wrappers use
`SETBE` to return one for either VM-failure class and zero for success.

## AMD SVM entry

The dedicated host stack contains four stable values: the owning `HV_CPU*`,
guest VMCB virtual address, guest VMCB physical address, and host VMCB physical
address. The VMRUN loop must retain its VMSAVE/VMLOAD ordering: save the guest
VMCB, load the host VMCB, call C, then restore guest registers before the next
VMRUN.

## Hook dispatcher

The generated hook thunk enters `AsmHookDispatcher` with this stack:

| Entry offset | Value |
| ---: | --- |
| `0x00` | Hook ID |
| `0x08` | Original R10 |
| `0x10` | Original caller return address |

The unwind record describes the two pre-entry pushes with `.allocstack 10h`.
The dispatcher preserves RFLAGS, all ABI-volatile GPRs, and XMM0-XMM5 while it
calls `HookObserveDispatch`. After restoring R10, its dead stacked slot is
reused for the selected trampoline address. A null result selects a local
return stub.

The final `ret` transfers through that slot without changing a guest register
or RFLAGS. It leaves the original caller return address at the top of the
target's stack.

`JohnSmithHookProbeTarget` is deliberately simple: its first 21 bytes must
remain accepted by the conservative instruction decoder in
`HookTrampoline.c`, with RET immediately after the copied window.

## Change rules

- Keep assembly-visible offsets named; do not introduce unexplained numeric
  stack offsets.
- Update the C layout assertions and this document together when a shared
  frame changes.
- Preserve the 32-byte Windows home area and 16-byte call-site alignment.
- Comment register ownership, hardware flag semantics, and recovery behavior;
  do not narrate individual instructions.
- Verify object code and unwind metadata after editing an entry path. Host-side
  verification must never load the driver.
