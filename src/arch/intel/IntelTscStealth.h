#pragma once

/*
 * Timestamp stealth model (trap-both, as of 2026-08-09).
 *
 * HvD --tsc-exit times the guest-visible delta of an RDTSC;CPUID(0);RDTSC
 * probe (lfence;rdtsc;lfence;cpuid;lfence;rdtsc;lfence, 10 samples,
 * Sleep(500) between, gate on p10 < 1000).  CPUID exits unconditionally in
 * VMX non-root operation (SDM 25.1.2), and the exit/entry span (~750 cycles
 * warm, up to ~10K after a C-state wakeup) exceeds the gate whenever the
 * second RDTSC reads the real TSC.  No handler optimization can fix that;
 * the guest-visible value must be compensated instead.
 *
 * Trap-both model (active): RDTSC, RDTSCP, and RDMSR(IA32_TSC) are
 * intercepted.  Every CPUID exit arms a one-shot trap (both the leaf-0 asm
 * micropath and the C path do this).  The next timestamp read that exits
 * while the trap is fresh returns
 *
 *     returnedValue = lastGuestRdtsc + bareMetalCpuidCost
 *
 * so the guest-visible RDTSC;CPUID;RDTSC delta collapses to
 * bareMetalCpuidCost (~134 cycles on the i5-12400F, well under HvD's
 * 1000-cycle gate) regardless of VM-exit latency or C-state wakeup:
 *
 *     guestDelta = (t0 + bareMetalCpuidCost) - t0 = bareMetalCpuidCost
 *
 * This is the only compensation model that provably passes on Intel
 * because it never depends on per-sample VM-exit duration.  TSC_OFFSET
 * stays zero.  Any timestamp exit disarms the trap, and
 * INTEL_TSC_STEALTH_MAX_PAIR_CYCLES expires it, so unrelated guest RDTSC
 * traffic can consume it but cannot leave it armed across samples.
 *
 * Cost: every guest RDTSC/RDTSCP/RDMSR(IA32_TSC) now exits (~750 cycles
 * warm, more when cold) -- a system-wide latency tax that a pure
 * RDTSC-to-RDTSC loop timer would observe.  HvD's gated modules all pair
 * the timestamp read with a CPUID, which is exactly what this model
 * compensates.
 */

#define INTEL_TSC_STEALTH_CALIBRATION_SAMPLES 200u
#define INTEL_TSC_STEALTH_EXIT_ENTRY_MARGIN    256u
#define INTEL_TSC_STEALTH_MAX_PAIR_CYCLES      1000000ull
#define INTEL_TSC_STEALTH_IA32_TSC            0x00000010u
#define INTEL_TSC_STEALTH_IA32_TSC_AUX        0xC0000103u
#define INTEL_TSC_STEALTH_RDTSC_EXITING       (1u << 12)
#define INTEL_TSC_STEALTH_CPUID_EXITING       (1u << 1)

#define INTEL_TSC_STEALTH_EXIT_CPUID          10u
#define INTEL_TSC_STEALTH_EXIT_RDTSC          16u
#define INTEL_TSC_STEALTH_EXIT_RDTSCP         51u

#define INTEL_TSC_STEALTH_MSR_READ_BYTE \
    (INTEL_TSC_STEALTH_IA32_TSC / 8u)
#define INTEL_TSC_STEALTH_MSR_READ_MASK \
    (1u << (INTEL_TSC_STEALTH_IA32_TSC & 7u))

static inline int
IntelTscStealthMsrReadIsIntercepted(
    const unsigned char* bitmap
    )
{
    return bitmap != 0 &&
           (bitmap[INTEL_TSC_STEALTH_MSR_READ_BYTE] &
            INTEL_TSC_STEALTH_MSR_READ_MASK) != 0;
}

static inline int
IntelTscStealthIsTimestampExit(
    unsigned reason
    )
{
    return reason == INTEL_TSC_STEALTH_EXIT_RDTSC ||
           reason == INTEL_TSC_STEALTH_EXIT_RDTSCP;
}

static inline int
IntelTscStealthPairIsFresh(
    unsigned long long currentTsc,
    unsigned long long cpuidEntryTsc
    )
{
    return cpuidEntryTsc != 0 &&
           currentTsc - cpuidEntryTsc <=
               INTEL_TSC_STEALTH_MAX_PAIR_CYCLES;
}

static inline unsigned long long
IntelTscStealthDeriveExitEntryPenalty(
    unsigned long long observedMinimum,
    unsigned long long bareMetalCpuidCost
    )
{
    unsigned long long excess;

    if (observedMinimum <= bareMetalCpuidCost) {
        return 0;
    }
    excess = observedMinimum - bareMetalCpuidCost;
    return excess > INTEL_TSC_STEALTH_EXIT_ENTRY_MARGIN
        ? excess - INTEL_TSC_STEALTH_EXIT_ENTRY_MARGIN
        : 0;
}

static inline unsigned long long
IntelTscStealthCompensatedValue(
    unsigned long long cpuidEntryTsc,
    unsigned long long bareMetalCpuidCost,
    unsigned long long exitEntryPenalty
    )
{
    unsigned long long correctedEntry = exitEntryPenalty < cpuidEntryTsc
        ? cpuidEntryTsc - exitEntryPenalty
        : 0;

    return ~0ull - correctedEntry < bareMetalCpuidCost
        ? ~0ull
        : correctedEntry + bareMetalCpuidCost;
}

/*
 * Trap-both-RDTSC stealth compensation (HyperDbg lineage).  ACTIVE:
 * reached whenever RDTSC/RDTSCP/RDMSR(IA32_TSC) exits while a CPUID-armed
 * trap is fresh.  Also exercised by the host-side self-test.
 *
 * When the second RDTSC of an RDTSC;CPUID;RDTSC probe is intercepted, the
 * guest-visible delta collapses to the bare-metal CPUID cost regardless of
 * VM-exit latency or C-state wakeup:
 *
 *     returnedValue = firstRdtsc + bareMetalCpuidCost
 *     guestDelta     = returnedValue - firstRdtsc = bareMetalCpuidCost
 *
 * This is the only compensation model that provably passes on Intel because
 * it never depends on per-sample VM-exit duration.  TSC_OFFSET stays zero.
 */
static inline unsigned long long
IntelTscStealthCompensatedRdtsc(
    unsigned long long firstRdtsc,
    unsigned long long bareMetalCpuidCost
    )
{
    return ~0ull - firstRdtsc < bareMetalCpuidCost
        ? ~0ull
        : firstRdtsc + bareMetalCpuidCost;
}
