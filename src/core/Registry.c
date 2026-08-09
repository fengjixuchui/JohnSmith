#include <ntifs.h>

#include "Registry.h"

#define JOHNSMITH_POOL_TAG_REGISTRY 'rSvJ'
#define JOHNSMITH_POOL_TAG_SECURITY 'sSvJ'

static UNICODE_STRING g_RegistryPath;

static NTSTATUS
JohnSmithRegistryOpenServiceKey(
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE KeyHandle
    )
{
    OBJECT_ATTRIBUTES attributes;

    InitializeObjectAttributes(
        &attributes,
        &g_RegistryPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);
    return ZwOpenKey(KeyHandle, DesiredAccess, &attributes);
}

static NTSTATUS
JohnSmithRegistryOpenParametersKey(
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE KeyHandle
    )
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"Parameters");
    OBJECT_ATTRIBUTES attributes;
    HANDLE serviceKey;
    NTSTATUS status;

    status = JohnSmithRegistryOpenServiceKey(KEY_READ, &serviceKey);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    InitializeObjectAttributes(
        &attributes,
        &name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        serviceKey,
        NULL);
    status = ZwOpenKey(KeyHandle, DesiredAccess, &attributes);
    ZwClose(serviceKey);
    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryInitialize(
    _In_ PCUNICODE_STRING RegistryPath
    )
{
    if (RegistryPath == NULL ||
        (RegistryPath->Length != 0 && RegistryPath->Buffer == NULL) ||
        RegistryPath->Length > MAXUSHORT - sizeof(WCHAR)) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath));
    g_RegistryPath.MaximumLength = RegistryPath->Length + sizeof(WCHAR);
    g_RegistryPath.Buffer = (PWSTR)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        g_RegistryPath.MaximumLength,
        JOHNSMITH_POOL_TAG_REGISTRY);
    if (g_RegistryPath.Buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_RegistryPath.Buffer, g_RegistryPath.MaximumLength);
    RtlCopyUnicodeString(&g_RegistryPath, RegistryPath);
    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
JohnSmithRegistryDestroy(
    VOID
    )
{
    if (g_RegistryPath.Buffer == NULL) {
        return;
    }

    ExFreePoolWithTag(g_RegistryPath.Buffer, JOHNSMITH_POOL_TAG_REGISTRY);
    RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath));
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryQueryDword(
    _In_ PCWSTR ValueName,
    _Out_ PULONG Value
    )
{
    union {
        KEY_VALUE_PARTIAL_INFORMATION Information;
        UCHAR Buffer[
            FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) +
            sizeof(ULONG)];
    } valueInformation;
    UNICODE_STRING name;
    HANDLE keyHandle;
    ULONG resultLength;
    NTSTATUS status;

    if (ValueName == NULL || Value == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitUnicodeString(&name, ValueName);
    status = JohnSmithRegistryOpenParametersKey(KEY_QUERY_VALUE, &keyHandle);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = ZwQueryValueKey(
        keyHandle,
        &name,
        KeyValuePartialInformation,
        &valueInformation,
        sizeof(valueInformation),
        &resultLength);
    if (NT_SUCCESS(status)) {
        if (valueInformation.Information.Type != REG_DWORD ||
            valueInformation.Information.DataLength != sizeof(ULONG)) {
            status = STATUS_OBJECT_TYPE_MISMATCH;
        } else {
            RtlCopyMemory(
                Value,
                valueInformation.Information.Data,
                sizeof(*Value));
        }
    }
    ZwClose(keyHandle);
    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryDeleteValue(
    _In_ PCWSTR ValueName
    )
{
    UNICODE_STRING name;
    HANDLE keyHandle;
    NTSTATUS status;

    if (ValueName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitUnicodeString(&name, ValueName);
    status = JohnSmithRegistryOpenParametersKey(KEY_SET_VALUE, &keyHandle);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = ZwDeleteValueKey(keyHandle, &name);
    ZwClose(keyHandle);
    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
JohnSmithRegistryRestrictAcl(
    VOID
    )
{
    SECURITY_DESCRIPTOR securityDescriptor;
    PACL dacl = NULL;
    HANDLE parametersKey = NULL;
    HANDLE serviceKey = NULL;
    ULONG daclLength;
    NTSTATUS status;

    if (SeExports == NULL ||
        SeExports->SeLocalSystemSid == NULL ||
        SeExports->SeAliasAdminsSid == NULL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    daclLength = sizeof(ACL) +
        2 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(ULONG)) +
        RtlLengthSid(SeExports->SeLocalSystemSid) +
        RtlLengthSid(SeExports->SeAliasAdminsSid);
    dacl = (PACL)ExAllocatePool2(
        POOL_FLAG_PAGED, daclLength, JOHNSMITH_POOL_TAG_SECURITY);
    if (dacl == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = RtlCreateAcl(dacl, daclLength, ACL_REVISION);
    if (NT_SUCCESS(status)) {
        status = RtlAddAccessAllowedAce(
            dacl,
            ACL_REVISION,
            KEY_ALL_ACCESS,
            SeExports->SeLocalSystemSid);
    }
    if (NT_SUCCESS(status)) {
        status = RtlAddAccessAllowedAce(
            dacl,
            ACL_REVISION,
            KEY_ALL_ACCESS,
            SeExports->SeAliasAdminsSid);
    }
    if (NT_SUCCESS(status)) {
        status = RtlCreateSecurityDescriptor(
            &securityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    }
    if (NT_SUCCESS(status)) {
        status = RtlSetDaclSecurityDescriptor(
            &securityDescriptor, TRUE, dacl, FALSE);
    }
    if (NT_SUCCESS(status)) {
        status = JohnSmithRegistryOpenServiceKey(
            READ_CONTROL | WRITE_DAC, &serviceKey);
    }
    if (NT_SUCCESS(status)) {
        status = JohnSmithRegistryOpenParametersKey(
            READ_CONTROL | WRITE_DAC, &parametersKey);
    }
    if (NT_SUCCESS(status)) {
        status = ZwSetSecurityObject(
            parametersKey,
            (SECURITY_INFORMATION)(
                DACL_SECURITY_INFORMATION |
                PROTECTED_DACL_SECURITY_INFORMATION),
            &securityDescriptor);
    }
    if (NT_SUCCESS(status)) {
        status = ZwSetSecurityObject(
            serviceKey,
            (SECURITY_INFORMATION)(
                DACL_SECURITY_INFORMATION |
                PROTECTED_DACL_SECURITY_INFORMATION),
            &securityDescriptor);
    }

    if (parametersKey != NULL) {
        ZwClose(parametersKey);
    }
    if (serviceKey != NULL) {
        ZwClose(serviceKey);
    }
    ExFreePoolWithTag(dacl, JOHNSMITH_POOL_TAG_SECURITY);
    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
JohnSmithRegistryRecordStartResult(
    _In_ ULONG State,
    _In_ NTSTATUS Status
    )
{
    UNICODE_STRING stateName = RTL_CONSTANT_STRING(L"StartState");
    UNICODE_STRING statusName = RTL_CONSTANT_STRING(L"StartStatus");
    HANDLE keyHandle;

    if (!NT_SUCCESS(JohnSmithRegistryOpenServiceKey(
            KEY_SET_VALUE, &keyHandle))) {
        return;
    }

    (VOID)ZwSetValueKey(
        keyHandle, &stateName, 0, REG_DWORD, &State, sizeof(State));
    (VOID)ZwSetValueKey(
        keyHandle, &statusName, 0, REG_DWORD, &Status, sizeof(Status));
    ZwClose(keyHandle);
}
