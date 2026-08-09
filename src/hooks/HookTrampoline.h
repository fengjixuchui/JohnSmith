#pragma once

#include "johnsmith/Hypervisor.h"

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
HookTrampolineBuild(
    _In_ PUCHAR OriginalVa,
    _In_ ULONG MinBytes,
    _Out_ PVOID* TrampolineVirtual,
    _Out_ ULONG* BytesCopied
    );

_IRQL_requires_(PASSIVE_LEVEL)
VOID
HookTrampolineFree(
    _In_ PVOID TrampolineVirtual
    );
