/**
 * @file Hypervisor.h
 * @brief Vendor-neutral lifecycle, per-CPU state, and backend contract.
 */
#pragma once

#include <ntddk.h>

typedef struct _HV_CPU HV_CPU;
typedef struct _HV_STATE HV_STATE;
typedef struct _HV_BACKEND_OPS HV_BACKEND_OPS;

#define HV_POOL_TAG_CPU_ARRAY       'pCvH'
#define HV_POOL_TAG_BACKEND         'kBvH'
#define HV_POOL_TAG_SLAT_SPLIT      'tSvH'
#define HV_POOL_TAG_HOOK            'kHvH'
#define HV_POOL_TAG_HOOK_POLICY     'pHvH'
#define HV_POOL_TAG_HOOK_META       'mHvH'
#define HV_POOL_TAG_HOOK_CODE       'cHvH'

#define HV_MAX_HOOKS                64u
#define HV_SLAT_MAXIMUM_ADDRESS     (512ull * 1024ull * 1024ull * 1024ull)
#define HV_HYPERCALL_MAGIC_RAX      0x31504F5453564E45ull
#define HV_HYPERCALL_MAGIC_RCX      0xC0DEC0DE4E41454Cull
#define HV_HYPERCALL_MAGIC_RDX      0x53544F504F4E4C59ull
#define HV_HYPERCALL_MAGIC_R8       0xA55A5AA5F00DCAFEull
#define HV_HYPERCALL_SLAT_R9        0x534C4154464C5553ull
#define HV_CR4_CET                  (1ull << 23)

/** @brief Vendor-neutral second-level address-translation permissions. */
typedef enum _HV_PAGE_ACCESS {
    HV_PAGE_ACCESS_NONE = 0,
    HV_PAGE_ACCESS_READ = 1,
    HV_PAGE_ACCESS_WRITE = 2,
    HV_PAGE_ACCESS_EXECUTE = 4
} HV_PAGE_ACCESS;

/** @brief Per-processor states used by synchronized launch and teardown. */
typedef enum _HV_CPU_STATE {
    HV_CPU_EMPTY = 0,
    HV_CPU_PREPARING,
    HV_CPU_PREPARED,
    HV_CPU_STARTING,
    HV_CPU_RUNNING,
    HV_CPU_START_FAILED,
    HV_CPU_STOPPING,
    HV_CPU_STOP_FAILED
} HV_CPU_STATE;

/** @brief Process-wide lifecycle states guarded by the common layer. */
typedef enum _HV_LIFECYCLE {
    HV_LIFECYCLE_STOPPED = 0,
    HV_LIFECYCLE_STARTING,
    HV_LIFECYCLE_RUNNING,
    HV_LIFECYCLE_STOPPING
} HV_LIFECYCLE;

/**
 * @brief Operations implemented by one vendor virtualization backend.
 *
 * Common owns lifecycle and rendezvous.  A backend owns every vendor control
 * page, its SLAT tree, and each HV_CPU::VendorContext.
 */
struct _HV_BACKEND_OPS {
    PCSTR Name;
    NTSTATUS (*Support)(VOID);
    NTSTATUS (*Prepare)(_Inout_ HV_STATE* State);
    VOID (*Free)(_Inout_ HV_STATE* State);
    NTSTATUS (*PrepareCpu)(_Inout_ HV_STATE* State, _Inout_ HV_CPU* Cpu);
    VOID (*FreeCpu)(_Inout_ HV_STATE* State, _Inout_ HV_CPU* Cpu);
    /* Optional PASSIVE_LEVEL service lifecycle pair. */
    NTSTATUS (*StartServices)(_Inout_ HV_STATE* State);
    VOID (*StopServices)(_Inout_ HV_STATE* State);
    /* PASSIVE_LEVEL callback: drain backend activity before CPU teardown. */
    VOID (*Quiesce)(_Inout_ HV_STATE* State);
    /*
     * IPI_LEVEL callbacks: called inside KeIpiGenericCall.  Do not allocate,
     * block, log, or touch pageable code.
     */
    NTSTATUS (*Start)(_Inout_ HV_STATE* State, _Inout_ HV_CPU* Cpu);
    NTSTATUS (*Stop)(_Inout_ HV_STATE* State, _Inout_ HV_CPU* Cpu);
    /* PASSIVE_LEVEL callback. */
    VOID (*ReportStartFailure)(
        _In_ HV_STATE* State,
        _In_ const HV_CPU* Cpu);
    NTSTATUS (*QueryOwnedPageAccess)(
        _Inout_ HV_STATE* State,
        _In_ PHYSICAL_ADDRESS PhysicalAddress,
        _Out_ HV_PAGE_ACCESS* Access);
    NTSTATUS (*SetOwnedPageAccess)(
        _Inout_ HV_STATE* State,
        _In_ PHYSICAL_ADDRESS PhysicalAddress,
        _In_ HV_PAGE_ACCESS Access,
        _Out_ HV_PAGE_ACCESS* PreviousAccess);

    NTSTATUS (*HookInstall)(
        _Inout_ HV_STATE* State,
        _In_ ULONG64 GuestPhysicalAddress,
        _In_reads_bytes_(PatchSize) const VOID* PatchBytes,
        _In_ ULONG PatchOffset,
        _In_ ULONG PatchSize,
        _In_ ULONG64 Cookie,
        _Out_ ULONG* HookId);
    NTSTATUS (*HookRemove)(
        _Inout_ HV_STATE* State,
        _In_ ULONG HookId);
    NTSTATUS (*HookQuery)(
        _In_ HV_STATE* State,
        _In_ ULONG HookId,
        _Out_ ULONG* Valid,
        _Out_ ULONG* Kind,
        _Out_ ULONG64* Cookie,
        _Out_ ULONG64* GuestPhysicalAddress,
        _Out_ ULONG64* ShadowHostPhysicalAddress);
};

struct _HV_CPU {
    ULONG ProcessorIndex;
    volatile LONG State;
    NTSTATUS Status;
    PVOID VendorContext;
};

struct _HV_STATE {
    const HV_BACKEND_OPS* Backend;
    PVOID BackendContext;
    HV_CPU* Cpus;
    ULONG CpuCount;
    volatile LONG Lifecycle;
};

/**
 * @brief Acquires the running hypervisor for a passive-level worker.
 * @param[out] OutState Receives a borrowed state pointer on success.
 * @return `TRUE` when the state is running and teardown is excluded.
 */
BOOLEAN
HvTryAcquireForWorker(
    _Out_ HV_STATE** OutState
    );

/** @brief Releases one acquisition made by `HvTryAcquireForWorker`. */
VOID
HvReleaseForWorker(
    VOID
    );

/**
 * @brief Selects, prepares, and launches a backend on every active processor.
 * @param[out] State Receives the initialized state on success and `NULL` otherwise.
 * @return `STATUS_SUCCESS` or the first preparation/launch failure.
 */
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
HvStart(
    _Outptr_result_maybenull_ HV_STATE** State
    );

/**
 * @brief Quiesces and stops every launched processor, then frees the state.
 * @param[in,out] State Running state returned by `HvStart`.
 */
_IRQL_requires_(PASSIVE_LEVEL)
VOID
HvStop(
    _Inout_ HV_STATE* State
    );

/** @return Intel VMX/EPT backend operations. */
const HV_BACKEND_OPS*
HvIntelGetBackendOps(
    VOID
    );

/** @return AMD SVM/NPT backend operations. */
const HV_BACKEND_OPS*
HvAmdGetBackendOps(
    VOID
    );
