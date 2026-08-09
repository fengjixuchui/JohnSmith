#pragma once

#include "johnsmith/Hypervisor.h"

VOID
HookThunkInitialize(
    VOID
    );

VOID
HookThunkReset(
    VOID
    );

NTSTATUS
HookThunkAllocate(
    _Out_ PVOID* SlotVirtual,
    _Out_ ULONG* HookNonce
    );

VOID
HookThunkFree(
    _In_ PVOID SlotVirtual
    );

ULONG
HookThunkSlotSize(
    VOID
    );
