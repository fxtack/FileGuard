/*++

    The MIT License (MIT)

    Copyright (c) 2023 Fxtack

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Module Name:

    FileGuardCode.c

Abstract:

    Driver entry of FileGuardCore.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

FG_CORE_GLOBALS Globals;

//  operation registration
CONST FLT_OPERATION_REGISTRATION FgOperationCallbacks[] = {

    { IRP_MJ_CREATE,
      0,
      FgPreCreateCallback,
      FgPostCreateCallback },

    { IRP_MJ_WRITE,
      0,
      FgPreWriteCallback,
      NULL },

    { IRP_MJ_SET_INFORMATION,
      0,
      FgPreSetInformationCallback,
      FgPostSetInformationCallback },

    { IRP_MJ_OPERATION_END }
};

extern const FLT_CONTEXT_REGISTRATION FgContextRegistration[];

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),       // Size
    FLT_REGISTRATION_VERSION,       // Version
    0,                              // Flags

    FgContextRegistration,      // Context
    FgOperationCallbacks,             // Operation callbacks

    FgUnload,                   // MiniFilterUnload

    FgInstanceSetup,            // InstanceSetup
    FgInstanceQueryTeardown,    // InstanceQueryTeardown
    FgInstanceTeardownStart,    // InstanceTeardownStart
    FgInstanceTeardownComplete, // InstanceTeardownComplete

    NULL,                           // GenerateFileName
    NULL,                           // GenerateDestinationFileName
    NULL                            // NormalizeNameComponent
};

DRIVER_INITIALIZE DriverEntry;

#pragma alloc_text(INIT, DriverEntry)

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    NTSTATUS status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES attributes = { 0 };
    PSECURITY_DESCRIPTOR securityDescriptor = NULL;
    UNICODE_STRING portName = { 0 };
    PFG_MONITOR_CONTEXT monitorContext = NULL;
    HANDLE monitorHandle = NULL;

    PAGED_CODE();

    // Setup default log level.
#ifdef DBG
    Globals.LogLevel = LOG_LEVEL_INFO | LOG_LEVEL_WARNING | LOG_LEVEL_ERROR;
#else
    GLobals.LogLevel = LOG_LEVEL_DEFAULT;
#endif

    LOG_INFO("Start to load FileGuardCore driver, version: v%d.%d.%d.%d ",
        FG_CORE_VERSION_MAJOR, FG_CORE_VERSION_MINOR, FG_CORE_VERSION_PATCH, FG_CORE_VERSION_BUILD);

    // Register with FltMgr to tell it our callback routines
    try {

        //
        // Setup driver configuration from registry.
        //
        status = FgSetConfiguration(RegistryPath);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', set configuration failed", status);
            leave;
        }

        //
        // Intialize instance context list and lock.
        //
        InitializeListHead(&Globals.InstanceContextList);
        ExInitializeFastMutex(&Globals.InstanceContextListMutex);

        InitializeListHead(&Globals.MonitorRecordsQueue);
        KeInitializeSpinLock(&Globals.MonitorRecordsQueueLock);

        ExInitializeNPagedLookasideList(&Globals.RuleEntryMemoryPool,
                                        NULL,
                                        NULL,
                                        POOL_NX_ALLOCATION,
                                        sizeof(FG_RULE_ENTRY) + sizeof(RTL_BALANCED_LINKS),
                                        FG_RULE_ENTRY_NPAGED_MEM_TAG,
                                        0);

        //
        // Register filter driver.
        //
        status = FltRegisterFilter(DriverObject, &FilterRegistration, &Globals.Filter);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', register filter failed", status);
            leave;
        };

        status = FltBuildDefaultSecurityDescriptor(&securityDescriptor, FLT_PORT_ALL_ACCESS);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', build security descriptor failed", status);
            leave;
        }

        //
        // Create core control port.
        //
        RtlInitUnicodeString(&portName, FG_CORE_CONTROL_PORT_NAME);
        InitializeObjectAttributes(&attributes,
                                   &portName,
                                   OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   securityDescriptor);

        status = FltCreateCommunicationPort(Globals.Filter,
                                            &Globals.ControlCorePort,
                                            &attributes,
                                            NULL,
                                            FgCoreControlPortConnectCallback,
                                            FgCoreControlPortDisconnectCallback,
                                            FgCoreControlMessageNotifyCallback,
                                            1);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', create core control communication port failed", status);
            leave;
        }

        //
        // Create monitor port.
        //
        RtlInitUnicodeString(&portName, FG_MONITOR_PORT_NAME);
        InitializeObjectAttributes(&attributes,
                                   &portName,
                                   OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   securityDescriptor);

        status = FltCreateCommunicationPort(Globals.Filter,
                                            &Globals.MonitorCorePort,
                                            &attributes,
                                            NULL,
                                            FgMonitorPortConnectCallback,
                                            FgMonitorPortDisconnectCallback,
                                            NULL,
                                            1);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', create monitor communication port failed", status);
            leave;
        }

        //
        // Create a kernel thread as monitor.
        //

        status = FgCreateMonitorStartContext(Globals.Filter,
            &Globals.MonitorRecordsQueue,
            &monitorContext);
        if (!NT_SUCCESS(status) || NULL == monitorContext) {
            DBG_ERROR("NTSTATUS: '0x%08x', create monitor start context failed", status);
            leave;
        }

        Globals.MonitorContext = monitorContext;

        // Create system thread as monitor.
        status = PsCreateSystemThread(&monitorHandle,
                                      THREAD_ALL_ACCESS,
                                      NULL,
                                      NULL,
                                      NULL,
                                      FgMonitorStartRoutine,
                                      monitorContext);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', create monitor thread failed", status);
            leave;
        }

        // Reference monitor handle object.
        ObReferenceObjectByHandle(monitorHandle,
                                  THREAD_ALL_ACCESS,
                                  NULL,
                                  KernelMode,
                                  &Globals.MonitorThreadObject,
                                  NULL);
        ZwClose(monitorHandle);

        // Start filter driver.
        status = FltStartFiltering(Globals.Filter);

    } finally {

        if (NULL != securityDescriptor)
            FltFreeSecurityDescriptor(securityDescriptor);

        if (!NT_SUCCESS(status)) {

            LOG_ERROR("NTSTATUS: '0x%08x', driver loading failed", status);

            if (NULL != Globals.MonitorCorePort)
                FltCloseCommunicationPort(Globals.MonitorCorePort);

            if (NULL != Globals.ControlCorePort)
                FltCloseCommunicationPort(Globals.ControlCorePort);

            if (NULL != Globals.Filter)
                FltUnregisterFilter(Globals.Filter);

            if (NULL != Globals.MonitorContext) {
                InterlockedExchangeBoolean(&Globals.MonitorContext->EndDaemonFlag, FALSE);
                KeSetEvent(&Globals.MonitorContext->EventPortConnected, 0, FALSE);
                KeSetEvent(&Globals.MonitorContext->EventWakeMonitor, 0, FALSE);
                FgFreeMonitorStartContext(Globals.MonitorContext);
            }

            if (NULL != Globals.MonitorThreadObject)
                ObReferenceObject(Globals.MonitorThreadObject);

            ExDeleteNPagedLookasideList(&Globals.RuleEntryMemoryPool);

        } else {

            LOG_INFO("Driver loaded successfully");
        }
    }

    return status;
}

NTSTATUS
FgUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
) {
    NTSTATUS status = STATUS_SUCCESS;
    LARGE_INTEGER monitorTerminateTimeout = { 0 };

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Flags);

    LOG_INFO("Unload driver start");

    if (NULL != Globals.ControlCorePort) {
        FltCloseCommunicationPort(Globals.ControlCorePort);
        Globals.ControlCorePort = NULL;
    }

    if (NULL != Globals.MonitorCorePort) {
        FltCloseCommunicationPort(Globals.MonitorCorePort);
        Globals.MonitorCorePort = NULL;
    }

    if (NULL != Globals.MonitorClientPort) {
        FltCloseClientPort(Globals.Filter, &Globals.MonitorClientPort);
        Globals.MonitorClientPort = NULL;
    }

    DBG_TRACE("Communication port closed");

    if (NULL != Globals.Filter) {
        FltUnregisterFilter(Globals.Filter);
    }

    DBG_TRACE("Unregister filter successfully");

    //
    // Stop the monitor thread.
    //

    FLT_ASSERT(NULL != Globals.MonitorContext);

    InterlockedExchangeBoolean(&Globals.MonitorContext->EndDaemonFlag, TRUE);
    KeSetEvent(&Globals.MonitorContext->EventPortConnected, 0, FALSE);
    KeSetEvent(&Globals.MonitorContext->EventWakeMonitor, 0, FALSE);

    if (NULL != Globals.MonitorThreadObject) {

        monitorTerminateTimeout.QuadPart = -1000000;

        status = KeWaitForSingleObject(Globals.MonitorThreadObject,
            Executive,
            KernelMode,
            FALSE,
            &monitorTerminateTimeout);
        if (STATUS_TIMEOUT == status) {
            LOG_WARNING("Wait monitor thread terminate timeout");
            status = STATUS_SUCCESS;
        }

        ObDereferenceObject(Globals.MonitorThreadObject);
        Globals.MonitorThreadObject = NULL;
    }

    FgFreeMonitorStartContext(Globals.MonitorContext);

    ExDeleteNPagedLookasideList(&Globals.RuleEntryMemoryPool);

    LOG_INFO("Unload driver finish");

    return status;
}

NTSTATUS
FgInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

    If this routine is not defined in the registration structure, automatic
    instances are alwasys created.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG volumeNameSize = 0ul;
    PUNICODE_STRING volumeName = NULL;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);

    LOG_INFO("Start setup a instance for the volume");

    if (FLT_FSTYPE_NTFS != VolumeFilesystemType) {
        LOG_WARNING("Attempt to attach to a non NTFS file system");
        status = STATUS_FLT_DO_NOT_ATTACH;
        goto Cleanup;
    }

    status = FltGetVolumeName(FltObjects->Volume, NULL, &volumeNameSize);
    if (!NT_SUCCESS(status) && STATUS_BUFFER_TOO_SMALL != status) {
        LOG_ERROR("NTSTATUS: 0x%08x, get volume name suze failed", status);
        goto Cleanup;
    }
    
    status = FgAllocateUnicodeString((USHORT)volumeNameSize, &volumeName);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate volume name string failed", status);
        goto Cleanup;
    }
    
    status = FltGetVolumeName(FltObjects->Volume, volumeName, NULL);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, get volume name failed", status);
        goto Cleanup;
    }

    LOG_INFO("Setup instance for volume: '%wZ'", volumeName);

Cleanup:

    if (NULL != volumeName) {
        FgFreeUnicodeString(volumeName);
    }

    return status;
}

NTSTATUS
FgInstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    LOG_INFO("Instance query teardown");

    return STATUS_SUCCESS;
}

VOID
FgInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    LOG_INFO("Instance teardown start");
}

VOID
FgInstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    LOG_INFO("Instance teardown completed");
}

_Check_return_
NTSTATUS
FgSetConfiguration(
    _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Descrition:

    This routine sets the filter configuration based on registry values.

Arguments:

    RegistryPath - The path key passed to the driver during DriverEntry.

Return Value:

    Returns the status of this operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES attributes = { 0 };
    HANDLE driverRegKey = NULL;
    UNICODE_STRING valueName = { 0 };
    UCHAR buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)] = { 0 };
    PKEY_VALUE_PARTIAL_INFORMATION value = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;
    ULONG valueLength = sizeof(buffer);
    ULONG resultLength = 0L;

    PAGED_CODE();

    InitializeObjectAttributes(&attributes,
                               RegistryPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    status = ZwOpenKey(&driverRegKey, KEY_READ, &attributes);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    //
    // Read log level from registry.
    //
    RtlInitUnicodeString(&valueName, L"LogLevel");

    status = ZwQueryValueKey(driverRegKey,
                             &valueName,
                             KeyValuePartialInformation,
                             value,
                             valueLength,
                             &resultLength);
    if (NT_SUCCESS(status)) {
        Globals.LogLevel = *(PULONG)value->Data;
    } else {

        LOG_ERROR("NTSTATUS: '0x%08x', read log level registry configuration failed", status);
        status = STATUS_SUCCESS;
    }

Cleanup:

    if (NULL != driverRegKey) {
        ZwClose(driverRegKey);
    }

    return status;
}
