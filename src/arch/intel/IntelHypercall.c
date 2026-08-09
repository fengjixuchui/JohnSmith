#include "IntelHypercall.h"
#include "../../core/Registry.h"

#include <ntddk.h>

static ULONG g_HypercallSeed;
static ULONG g_HypercallSubleaf;
static ULONG g_HypercallRegisterSubleaf;
static ULONG g_HypercallCommandIds[INTEL_HYPERCALL_CMD_COUNT];
static BOOLEAN g_HypercallSeeded;

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
IntelHypercallPrepare(
    VOID
    )
{
    ULONG i;
    ULONG seed;
    NTSTATUS status;

    g_HypercallSeeded = FALSE;
    g_HypercallSeed = 0;
    g_HypercallSubleaf = 0;
    g_HypercallRegisterSubleaf = 0;
    RtlZeroMemory(g_HypercallCommandIds, sizeof(g_HypercallCommandIds));

    status = JohnSmithRegistryQueryDword(L"HypercallSeed", &seed);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (seed == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    g_HypercallSeed = seed;
    g_HypercallSubleaf = (ULONG)IntelHypercallFnv1a(
        seed, INTEL_HYPERCALL_SUBLEAF_TAG);
    g_HypercallRegisterSubleaf = (ULONG)IntelHypercallFnv1a(
        seed, INTEL_HYPERCALL_REGISTER_SUBLEAF_TAG);
    if (g_HypercallSubleaf == 0 ||
        g_HypercallRegisterSubleaf == 0 ||
        g_HypercallSubleaf == g_HypercallRegisterSubleaf) {
        g_HypercallSeed = 0;
        g_HypercallSubleaf = 0;
        g_HypercallRegisterSubleaf = 0;
        return STATUS_INVALID_PARAMETER;
    }

    for (i = 0; i < INTEL_HYPERCALL_CMD_COUNT; ++i) {
        PCSTR tag = IntelHypercallCommandTag((INTEL_HYPERCALL_CMD)i);

        NT_ASSERT(tag != NULL);
        g_HypercallCommandIds[i] = (ULONG)IntelHypercallFnv1a(seed, tag);
    }

    status = JohnSmithRegistryDeleteValue(L"HypercallSeed");
    if (!NT_SUCCESS(status)) {
        g_HypercallSeed = 0;
        g_HypercallSubleaf = 0;
        g_HypercallRegisterSubleaf = 0;
        RtlZeroMemory(g_HypercallCommandIds, sizeof(g_HypercallCommandIds));
        return status;
    }
    g_HypercallSeeded = TRUE;
    return STATUS_SUCCESS;
}

BOOLEAN
IntelHypercallIsSeeded(
    VOID
    )
{
    return g_HypercallSeeded;
}

ULONG
IntelHypercallSubleaf(
    VOID
    )
{
    return g_HypercallSubleaf;
}

ULONG
IntelHypercallRegisterSubleaf(
    VOID
    )
{
    return g_HypercallRegisterSubleaf;
}

ULONG
IntelHypercallCommandId(
    _In_ INTEL_HYPERCALL_CMD Command
    )
{
    if ((ULONG)Command >= INTEL_HYPERCALL_CMD_COUNT) {
        return 0;
    }
    return g_HypercallCommandIds[Command];
}
