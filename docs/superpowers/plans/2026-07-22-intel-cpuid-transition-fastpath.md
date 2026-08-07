# Intel CPUID Transition Fast Path Implementation Plan

**Goal:** Reduce the real Intel leaf-0 CPUID VM-transition cost without dynamic TSC-offset concealment or changes to rendezvous freeze compensation.

**Architecture:** Keep the existing cached assembly micropath, remove redundant leaf-0 register saves, and allow PAT/EFER to remain live across root/non-root transitions only when every related control permits zero. Retain hardware debug-state management and fall back to complete PAT/EFER save-load triplets on processors that force any member of a triplet.

**Tech stack:** C17 Windows kernel driver, MASM x64, Intel VMX, MSVC/WDK 10.0.26100, portable assert self-check.

## File structure

- Modify `src/intel/intel_rendezvous_policy.h`: add the portable VMX capability predicate used by production code and the self-check.
- Modify `tools/intel-rendezvous-policy-selfcheck.c`: cover persistent and hardware-managed transition decisions.
- Modify `src/intel/intel_vmcs.c`: select all-or-none PAT/EFER controls from cached capability MSRs.
- Modify `src/intel/intel_exit.c`: capture live PAT/EFER during shutdown when transition controls are disabled.
- Modify `asm/intel.asm`: save only `r8/r9` on the leaf-0 micropath and isolate Benchmark VMCALL saves.

## Tasks

1. **Add the failing transition-policy self-check.** Write assertions for `IntelVmxTransitionStateCanPersist` covering: triplet disabled when all controls permit zero, forced-one control selects the full fallback triplet, PAT and EFER decisions are independent. Build and verify RED.

2. **Implement the transition-state predicate.** Add `IntelVmxTransitionStateCanPersist` to `intel_rendezvous_policy.h`. The predicate returns true only when both exit and entry allowed-zero bits are set for every control in the triplet. Build and verify the self-check passes.

3. **Select all-or-none PAT/EFER controls in VMCS setup.** In `intel_vmcs.c`, read the true or legacy capability MSRs once. For each state, if `IntelVmxTransitionStateCanPersist` returns true, leave the triplet disabled. Otherwise request all three controls. Keep guest and host VMCS fields initialized in both modes. Verify the existing rendezvous policy self-check still passes.

4. **Capture live PAT/EFER during shutdown.** In `IntelCaptureStopState`, read PAT and EFER from the guest VMCS field when hardware-managed, or from the live MSR with `RDMSR` when persistent. Verify Debug, Release, and Benchmark build.

5. **Reduce the leaf-0 assembly micropath.** In `asm/intel.asm`, save only `r8` and `r9` on the leaf-0 path. Address the fixed host frame directly from RSP. Do not clobber CPUID output registers until the guest RIP `VMWRITE` succeeds. On probe failure, restore original guest GPRs and fall through to the C slow path. Isolate Benchmark VMCALL saves to `r10/r11` after exit-reason classification only. Verify the benchmark self-check and `git diff --check`.

6. **Verify Pafish and TSC monotonicity.** Record `VMCALL floor` and leaf-0 CPUID on the same-host Benchmark build. Run Pafish `cpu_rdtsc_force_vmexit` and require `0 < average < 1000`. Run cross-core TSC monotonic and libuv workloads and confirm no backward timestamp, freeze, or `new_time >= loop->time` assertion.

## Validation

- The transition-policy self-check fails before implementation and passes afterward.
- Existing Intel rendezvous policy and benchmark self-checks pass.
- Debug, Release, and Benchmark x64 build without warnings.
- `git diff --check` passes.
- `HideRoot`, `TscOffsetPtr`, `LastGuestTsc`, and `TSC_HIDE` remain absent.
- Pafish reports `0 < average < 1000`.
- Cross-core TSC stays monotonic with no libuv assertion.

If the optimized Benchmark `VMCALL floor` stays at or above 1000 TSC ticks, the target is below the host's measured transition floor. Stop there rather than reintroduce dynamic TSC-offset concealment or guest-code patching.
