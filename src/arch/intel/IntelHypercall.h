#pragma once

#include "johnsmith/Intel.h"
#include "IntelHypercallProtocol.h"

#define INTEL_HCALL_PAYLOAD_OFFSET 256u

#pragma pack(push, 1)
typedef struct _INTEL_HCALL_PAGE {
    ULONG CommandId;
    ULONG Sequence;
    ULONG64 Args[4];
    ULONG64 Result;
    ULONG64 ResultSequence;
    UCHAR Reserved[INTEL_HCALL_PAYLOAD_OFFSET - 56];
    UCHAR Payload[PAGE_SIZE - INTEL_HCALL_PAYLOAD_OFFSET];
} INTEL_HCALL_PAGE, *PINTEL_HCALL_PAGE;
#pragma pack(pop)

C_ASSERT(sizeof(INTEL_HCALL_PAGE) == PAGE_SIZE);
C_ASSERT(FIELD_OFFSET(INTEL_HCALL_PAGE, Payload) ==
    INTEL_HCALL_PAYLOAD_OFFSET);
C_ASSERT(FIELD_OFFSET(INTEL_HCALL_PAGE, Result) == 40);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
IntelHypercallPrepare(
    VOID
    );

BOOLEAN
IntelHypercallIsSeeded(
    VOID
    );

ULONG
IntelHypercallSubleaf(
    VOID
    );

ULONG
IntelHypercallRegisterSubleaf(
    VOID
    );

ULONG
IntelHypercallCommandId(
    _In_ INTEL_HYPERCALL_CMD Command
    );

NTSTATUS
IntelHypercallWorkerEnqueueRegister(
    _Inout_ INTEL_CPU_CONTEXT* Context,
    _In_ PEPROCESS Process,
    _In_ PVOID SharedPageUserVa
    );

NTSTATUS
IntelHypercallWorkerEnqueue(
    _In_ INTEL_HYPERCALL_CMD Command,
    _Inout_ INTEL_CPU_CONTEXT* Context,
    _Inout_ PINTEL_HCALL_PAGE Page,
    _In_ ULONG64 Arg0,
    _In_ ULONG64 Arg1
    );

VOID
IntelHypercallReleasePage(
    _Inout_ INTEL_CPU_CONTEXT* Context
    );

NTSTATUS
IntelHypercallWorkerStart(
    VOID
    );

VOID
IntelHypercallWorkerStop(
    VOID
    );
