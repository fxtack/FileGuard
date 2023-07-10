/*
	@File	Driver.c
	@Note	Driver entry and driver lifecycle routine.

	@Mode	Kernel
	@Author	Fxtack
*/

#include "NTFZCore.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

// Driver global variables.
NTFZ_CORE_GLOBALS Globals;

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
NTFZCoreUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
NTFZCoreInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
NTFZCoreInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
NTFZCoreInstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

NTSTATUS
NTFZCoreInstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

NTSTATUS NTFZCorePortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID *ConnectionCookie
);

VOID NTFZCorePortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
);

EXTERN_C_END


//  Assign text sections for each routine.
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NTFZCoreUnload)
#pragma alloc_text(PAGE, NTFZCoreInstanceQueryTeardown)
#pragma alloc_text(PAGE, NTFZCoreInstanceSetup)
#pragma alloc_text(PAGE, NTFZCoreInstanceTeardownStart)
#pragma alloc_text(PAGE, NTFZCoreInstanceTeardownComplete)
#pragma alloc_text(PAGE, NTFZCorePortConnectCallback)
#pragma alloc_text(PAGE, NTFZCorePortDisconnectCallback)
#endif


//  operation registration
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE,
      0,
      NTFZPreOperationCallback,
      NTFZPostOperationCallback },

    { IRP_MJ_WRITE,
      0,
      NTFZPreOperationCallback,
      NTFZPostOperationCallback },

    { IRP_MJ_SET_INFORMATION,
      0,
      NTFZPreOperationCallback,
      NTFZPostOperationCallback },

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),          //  Size
    FLT_REGISTRATION_VERSION,          //  Version
    0,                                 //  Flags

    NULL,                              //  Context
    Callbacks,                         //  Operation callbacks

    NTFZCoreUnload,                    //  MiniFilterUnload

    NTFZCoreInstanceSetup,             //  InstanceSetup
    NTFZCoreInstanceQueryTeardown,     //  InstanceQueryTeardown
    NTFZCoreInstanceTeardownStart,     //  InstanceTeardownStart
    NTFZCoreInstanceTeardownComplete,  //  InstanceTeardownComplete

    NULL,                              //  GenerateFileName
    NULL,                              //  GenerateDestinationFileName
    NULL                               //  NormalizeNameComponent
};


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    NTSTATUS status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES objAttr = { 0 };
    PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
    UNICODE_STRING portName;

    UNREFERENCED_PARAMETER(RegistryPath);
    PAGED_CODE();

    KdPrint(("NtFZCore!%s: Driver entry initialization\n", __func__));

    // Register with FltMgr to tell it our callback routines
    try {
        // Setup global variables.
        Globals.ConfigEntryMaxAllocated = MAX_CONFIG_ENTRY_ALLOCATED;
        Globals.ConfigEntryAllocated    = 0;

        ExInitializeNPagedLookasideList(&Globals.ConfigEntryMemoryPool,
                                        NULL,
                                        NULL,
                                        POOL_NX_ALLOCATION,
                                        sizeof(RTL_BALANCED_LINKS) + sizeof(NTFZ_CONFIG),
                                        MEM_NPAGED_POOL_TAG_CONFIG_ENTRY,
                                        0);
        
        ExInitializeNPagedLookasideList(&Globals.ConfigObjectMemoryPool,
                                        NULL,
                                        NULL,
                                        POOL_NX_ALLOCATION,
                                        sizeof(NTFZ_CONFIG),
                                        MEM_NPAGED_POOL_TAG_CONFIG_OBJECT,
                                        0);

        // Setup config table.
        RtlInitializeGenericTable(&Globals.ConfigTable,
                                  ConfigEntryCompareRoutine,
                                  ConfigEntryAllocateRoutine,
                                  ConfigEntryFreeRoutine,
                                  NULL);

        // Initialize config table share spin lock.
        KeInitializeSpinLock(&Globals.ConfigTableLock);

        // Register filter driver.
        status = FltRegisterFilter(DriverObject,
                                   &FilterRegistration,
                                   &Globals.Filter);
        if (!NT_SUCCESS(status)) leave;

        status = FltBuildDefaultSecurityDescriptor(&pSecurityDescriptor,
                                                   FLT_PORT_ALL_ACCESS);
        if (!NT_SUCCESS(status)) leave;

        // Make unicode string port name.
        RtlInitUnicodeString(&portName, NTFZ_COMMAND_PORT_NAME);
        InitializeObjectAttributes(&objAttr,
                                   &portName,
                                   OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   pSecurityDescriptor);

        // Create port.
        status = FltCreateCommunicationPort(Globals.Filter,
                                            &Globals.CorePort,
                                            &objAttr,
                                            NULL,
                                            NTFZCorePortConnectCallback,
                                            NTFZCorePortDisconnectCallback,
                                            NTFZCoreMessageHandlerRoutine,
                                            1);
        if (!NT_SUCCESS(status)) leave;

        // Start filter driver.
        status = FltStartFiltering(Globals.Filter);
        if (!NT_SUCCESS(status)) 
            FltUnregisterFilter(Globals.Filter);

    } finally {

        if (pSecurityDescriptor != NULL)
            FltFreeSecurityDescriptor(pSecurityDescriptor);

        if (!NT_SUCCESS(status)) {
            KdPrint(("NtFZCore!%s: Driver loading failed\n", __func__));

            if (Globals.CorePort != NULL)
                FltCloseCommunicationPort(Globals.CorePort);

            if (Globals.Filter != NULL)
                FltUnregisterFilter(Globals.Filter);
            
            ExDeleteNPagedLookasideList(&Globals.ConfigEntryMemoryPool);
            ExDeleteNPagedLookasideList(&Globals.ConfigObjectMemoryPool);

        } else {
            KdPrint(("NtFZCore!%s: Driver loaded successfully, version: v%lu.%lu.%lu\n",
                __func__, NTFZ_CORE_VERSION_MAJOR, NTFZ_CORE_VERSION_MINOR, NTFZ_CORE_VERSION_PATCH));
        }
    }

    return status;
}


NTSTATUS
NTFZCoreUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();

    KdPrint(("NTFZCore!%s: Driver unload\n", __func__));

    if (Globals.CorePort != NULL) 
        FltCloseCommunicationPort(Globals.CorePort);

    if (Globals.Filter != NULL)
        FltUnregisterFilter(Globals.Filter);

    CleanupConfigTable();

    ExDeleteNPagedLookasideList(&Globals.ConfigEntryMemoryPool);
    ExDeleteNPagedLookasideList(&Globals.ConfigObjectMemoryPool);

    return STATUS_SUCCESS;
}


NTSTATUS
NTFZCoreInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);
    PAGED_CODE();

    KdPrint(("NtFZCore!%s: Instance setup\n", __func__));

    return STATUS_SUCCESS;
}


NTSTATUS
NTFZCoreInstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();

    KdPrint(("NtFZCore!%s: Instance teardown\n", __func__));

    return STATUS_SUCCESS;
}


VOID
NTFZCoreInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();

    KdPrint(("NtFZCore!%s: Instance teardown starting\n", __func__));
}


VOID
NTFZCoreInstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();

    KdPrint(("NtFZCore!%s: Instance teardown comopleted\n", __func__));
}

NTSTATUS NTFZCorePortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
) {
    UNREFERENCED_PARAMETER(AdminPort);
    UNREFERENCED_PARAMETER(CorePortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ContextBytes);
    UNREFERENCED_PARAMETER(ConnectionCookie);
    PAGED_CODE();

    FLT_ASSERT(Globals.AdminPort == NULL);
    Globals.AdminPort = AdminPort;

    KdPrint(("NtFZCore!%s: Communication port connected\n", __func__));

    return STATUS_SUCCESS;
}

VOID NTFZCorePortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
) {
    UNREFERENCED_PARAMETER(ConnectionCookie);
    PAGED_CODE();

    FLT_ASSERT(Globals.AdminPort != NULL);
    FltCloseClientPort(Globals.Filter, &Globals.AdminPort);
    
    KdPrint(("NtFZCore!%s: Communication port disconnected\n", __func__));
}