#pragma once

#include <ntddk.h>

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryInitialize(
    _In_ PCUNICODE_STRING RegistryPath
    );

_IRQL_requires_(PASSIVE_LEVEL)
VOID
JohnSmithRegistryDestroy(
    VOID
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryRestrictAcl(
    VOID
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryQueryDword(
    _In_ PCWSTR ValueName,
    _Out_ PULONG Value
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryDeleteValue(
    _In_ PCWSTR ValueName
    );

_IRQL_requires_(PASSIVE_LEVEL)
VOID
JohnSmithRegistryRecordStartResult(
    _In_ ULONG State,
    _In_ NTSTATUS Status
    );
