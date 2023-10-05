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

    Driver.c

Abstract:

    Driver entry of FileGuardCore.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

FG_CORE_GLOBALS Globals;

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
FgCoreUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
FgCoreInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
FgCoreInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
FgCoreInstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

NTSTATUS
FgCoreInstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

_Check_return_
NTSTATUS
FgSetConfiguration(
    _In_ PUNICODE_STRING RegistryPath
);

EXTERN_C_END


// Assign text sections for each routine.
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FgCoreUnload)
#pragma alloc_text(PAGE, FgCoreInstanceQueryTeardown)
#pragma alloc_text(PAGE, FgCoreInstanceSetup)
#pragma alloc_text(PAGE, FgCoreInstanceTeardownStart)
#pragma alloc_text(PAGE, FgCoreInstanceTeardownComplete)
#pragma alloc_text(PAGE, FgCoreControlPortConnectCallback)
#pragma alloc_text(PAGE, FgCoreControlPortDisconnectCallback)
#pragma alloc_text(PAGE, FgSetConfiguration)
#endif

//  operation registration
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE,
      FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
      FgPreCreateOperationCallback,
      FgPostCreateOperationCallback },

    { IRP_MJ_WRITE,
      FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO ,
      FgPreWriteOperationCallback,
      FgPostWriteOperationCallback },

    { IRP_MJ_SET_INFORMATION,
      FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO ,
      FgPreSetInformationOperationCallback,
      FgPostSetInformationOperationCallback },

    { IRP_MJ_CLEANUP,
      FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO ,
      FgPreCleanupOperationCallback,
      FgPostCleanupOperationCallback },

    { IRP_MJ_OPERATION_END }
};

extern const FLT_CONTEXT_REGISTRATION FgCoreContextRegistration[];

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),       //  Size
    FLT_REGISTRATION_VERSION,       //  Version
    0,                              //  Flags

    FgCoreContextRegistration,      //  Context
    Callbacks,                      //  Operation callbacks

    FgCoreUnload,                   //  MiniFilterUnload

    FgCoreInstanceSetup,            //  InstanceSetup
    FgCoreInstanceQueryTeardown,    //  InstanceQueryTeardown
    FgCoreInstanceTeardownStart,    //  InstanceTeardownStart
    FgCoreInstanceTeardownComplete, //  InstanceTeardownComplete

    NULL,                           //  GenerateFileName
    NULL,                           //  GenerateDestinationFileName
    NULL                            //  NormalizeNameComponent
};


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

    // Register with FltMgr to tell it our callback routines
    try {
        
        //
        // Setup driver configuration from registry.
        //
        status = FgSetConfiguration(RegistryPath);
        if (!NT_SUCCESS(status)) leave;

        //
        // Intialize instance context list and lock.
        //
        InitializeListHead(&Globals.InstanceContextList);

        ExInitializeFastMutex(&Globals.InstanceContextListMutex);


        // Register filter driver.
        status = FltRegisterFilter(DriverObject,
                                   &FilterRegistration,
                                   &Globals.Filter);
        if (!NT_SUCCESS(status)) leave;


        status = FltBuildDefaultSecurityDescriptor(&securityDescriptor,
                                                   FLT_PORT_ALL_ACCESS);
        if (!NT_SUCCESS(status)) leave;

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
        if (!NT_SUCCESS(status)) leave;

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
        if (!NT_SUCCESS(status)) leave;
        
        //
        // Create a kernel thread as monitor.
        //

        // Allocate and setup monitor context.
        status = FgAllocateMonitorStartContext(&monitorContext);
        if (!NT_SUCCESS(status) || NULL == monitorContext) leave;

        monitorContext->Filter           = Globals.Filter;
        monitorContext->ClientPort       = NULL;
        monitorContext->RecordsQueue     = &Globals.MonitorRecordsQueue;
        monitorContext->RecordsQueueLock = &Globals.MonitorRecordsQueueLock;

        // Initialize monitor thread control event.
        KeInitializeEvent(&monitorContext->EventWakeMonitor, NotificationEvent, FALSE);
        KeInitializeEvent(&monitorContext->EventPortConnected, NotificationEvent, FALSE);
        KeInitializeEvent(&monitorContext->EventMonitorTerminate, NotificationEvent, FALSE);

        // Allocate monitor records buffer as message body.
        FgAllocateBuffer(&monitorContext->MessageBody, POOL_FLAG_PAGED, sizeof(FG_RECORDS_MESSAGE_BODY));
        
        // Initialize daemon flag.
        InterlockedExchangeBoolean(&monitorContext->EndDaemonFlag, FALSE);

        Globals.MonitorContext = monitorContext;

        // Create system thread as monitor.
        status = PsCreateSystemThread(&monitorHandle, 
                                      THREAD_ALL_ACCESS, 
                                      NULL, 
                                      NULL, 
                                      NULL,
                                      FgMonitorStartRoutine,
                                      monitorContext);
        if (!NT_SUCCESS(status)) leave;

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
        if (!NT_SUCCESS(status)) 
            FltUnregisterFilter(Globals.Filter);

    } finally {

        if (securityDescriptor != NULL)
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
        } else {

            LOG_INFO("Driver loaded successfully, version: v%d.%d.%d", 
                FG_CORE_VERSION_MAJOR, FG_CORE_VERSION_MINOR, FG_CORE_VERSION_PATCH);
        }
    }

    return status;
}


NTSTATUS
FgCoreUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
) {
    NTSTATUS status = STATUS_SUCCESS;
    LARGE_INTEGER monitorTerminateTimeout = { .QuadPart = -1000000 };

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
        Globals.MonitorContext->ClientPort = NULL;
    }

    if (NULL != Globals.Filter) {
        FltUnregisterFilter(Globals.Filter);
    }

    //
    // Stop the monitor thread.
    //

    FLT_ASSERT(NULL != Globals.MonitorContext);

    InterlockedExchangeBoolean(&Globals.MonitorContext->EndDaemonFlag, FALSE);
    KeSetEvent(&Globals.MonitorContext->EventPortConnected, 0, FALSE);
    KeSetEvent(&Globals.MonitorContext->EventWakeMonitor, 0, FALSE);

    if (NULL != Globals.MonitorThreadObject) {
        
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

    LOG_INFO("Unload driver finish");
    
    return status;
}


NTSTATUS
FgCoreInstanceSetup(
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
    PFG_INSTANCE_CONTEXT instanceContext = NULL;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);

    LOG_INFO("Start setup a instance for the volume");

    if (FLT_FSTYPE_NTFS != VolumeFilesystemType) {

        LOG_WARNING("Attempt to attach to a non NTFS file system");

        status = STATUS_VOLUME_NOT_SUPPORTED;
        goto Cleanup;
    }

    //
    // Allocate instance context.
    //
    status = FgCreateInstanceContext(FltObjects->Filter, 
                                     FltObjects->Volume,
                                     FltObjects->Instance,
                                     &instanceContext);
    if (!NT_SUCCESS(status)) {

        DBG_ERROR("NTSTATUS: '0x%08x', allocate instance context failed", status);

        goto Cleanup;
    }

    //
    // Set instance context to volume instance.
    //
    status = FgSetInstanceContext(FltObjects->Instance, 
                                  FLT_SET_CONTEXT_KEEP_IF_EXISTS, 
                                  instanceContext, 
                                  NULL);
    if (!NT_SUCCESS(status)) {

        DBG_ERROR("NTSTATUS: '0x%08x', set instance context failed", status);

        goto Cleanup;
    }

    LOG_INFO("Instance setup finish");

Cleanup:

    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

    return status;
}


NTSTATUS
FgCoreInstanceQueryTeardown(
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
FgCoreInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    
    LOG_INFO("Instance teardown start");
}


VOID
FgCoreInstanceTeardownComplete(
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

    // Setup default log level.
    Globals.LogLevel = LOG_LEVEL_DEFAULT;
    
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