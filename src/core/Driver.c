#include <ntifs.h>

#include "johnsmith/Hypervisor.h"
#include "Registry.h"

DRIVER_INITIALIZE DriverEntry;

#define JOHNSMITH_START_SUCCEEDED 2u
#define JOHNSMITH_START_FAILED 3u

static HV_STATE* g_Hypervisor;

_IRQL_requires_(PASSIVE_LEVEL)
static VOID
JohnSmithUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    if (g_Hypervisor != NULL) {
        HvStop(g_Hypervisor);
        g_Hypervisor = NULL;
    }
    JohnSmithRegistryDestroy();
}

_Use_decl_annotations_
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    ULONG state;
    NTSTATUS status;

    DriverObject->DriverUnload = JohnSmithUnload;
    status = JohnSmithRegistryInitialize(RegistryPath);
    if (!NT_SUCCESS(status)) {
        DriverObject->DriverUnload = NULL;
        return status;
    }

    status = JohnSmithRegistryRestrictAcl();
    if (NT_SUCCESS(status)) {
        status = HvStart(&g_Hypervisor);
    }

    state = NT_SUCCESS(status)
        ? JOHNSMITH_START_SUCCEEDED
        : JOHNSMITH_START_FAILED;
    JohnSmithRegistryRecordStartResult(state, status);

    if (!NT_SUCCESS(status)) {
        DriverObject->DriverUnload = NULL;
        g_Hypervisor = NULL;
        JohnSmithRegistryDestroy();
    }

    return status;
}
