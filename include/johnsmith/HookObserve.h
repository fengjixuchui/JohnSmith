/**
 * @file HookObserve.h
 * @brief Execute-hook observation API.
 */
#pragma once

#include "Hypervisor.h"

/** @brief Receives a hook hit before control continues through its trampoline. */
typedef VOID
(*HOOK_OBSERVE_CALLBACK)(
    _In_ ULONG HookId,
    _In_ ULONG64 CallerReturnAddress
    );

/** @brief Stable observation data returned by query and list operations. */
typedef struct _HOOK_OBSERVE_QUERY_ENTRY {
    ULONG HookId;
    ULONG Active;
    ULONG64 GuestPhysicalAddress;
    ULONG64 Cookie;
    LONG64 HitCount;
} HOOK_OBSERVE_QUERY_ENTRY, *PHOOK_OBSERVE_QUERY_ENTRY;

C_ASSERT(sizeof(HOOK_OBSERVE_QUERY_ENTRY) == 32);

/** @brief Initializes observation and thunk state during backend preparation. */
VOID
ObserveHookInitialize(
    VOID
    );

/**
 * @brief Installs an execute hook and publishes its observation record.
 * @return `STATUS_SUCCESS` or the first validation, allocation, or backend error.
 */
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ObserveHookInstall(
    _Inout_ HV_STATE* State,
    _In_ PVOID TargetVirtualAddress,
    _In_ ULONG64 Cookie,
    _In_ HOOK_OBSERVE_CALLBACK Callback,
    _Out_ ULONG* HookId
    );

/** @brief Removes a backend hook while retaining its record until teardown. */
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ObserveHookRemove(
    _Inout_ HV_STATE* State,
    _In_ ULONG HookId
    );

/** @brief Copies the current observation data for one hook. */
NTSTATUS
ObserveHookQuery(
    _In_ ULONG HookId,
    _Out_ PHOOK_OBSERVE_QUERY_ENTRY Query
    );

/** @brief Copies up to `Capacity` active hook records and returns the count. */
ULONG
ObserveHookList(
    _Out_writes_to_(Capacity, return) PHOOK_OBSERVE_QUERY_ENTRY Queries,
    _In_ ULONG Capacity
    );

/** @brief Releases all observation, thunk, and trampoline allocations. */
VOID
ObserveHookReset(
    VOID
    );
