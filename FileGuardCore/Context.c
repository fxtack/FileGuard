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

    FileGuardCore.c

Abstract:

    Context registrations and definitions of context routines.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Context.h"

//
// Context registrations.
//
const FLT_CONTEXT_REGISTRATION FgCoreContextRegistration[] = {

    { FLT_INSTANCE_CONTEXT,
      0,
      FgCleanupInstanceContext,
      sizeof(FG_INSTANCE_CONTEXT),
      FG_INSTANCE_CONTEXT_PAGED_MEM_TAG },

    { FLT_FILE_CONTEXT,
      0,
      FgCleanupFileContext,
      sizeof(FG_FILE_CONTEXT),
      FG_FILE_CONTEXT_PAGED_MEM_TAG },

    { FLT_CONTEXT_END }
};

/*-------------------------------------------------------------
    Instance context structure and routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgCreateInstanceContext(
    _In_     PFLT_FILTER Filter,
    _In_     PFLT_VOLUME Volume,
    _In_     PFLT_INSTANCE Instance,
    _Outptr_ PFG_INSTANCE_CONTEXT* InstanceContext
    ) 
/*++

Routine Description:

    This routine creates a new instance context.

Arguments:

    Filter          - Pointer to the filter structure.

    Volume          - Pointer to the FLT_VOLUME.

    Instance        - Opaque instance pointer for the caller.
                      This parameter is required and cannot be NULL.

    InstanceContext - The output instance context.

Return Value:

    Status

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;
    ULONG volumeNameSize = 0UL;

    PAGED_CODE();

    //
    // Allocate a instance context.
    //
    status = FltAllocateContext(Filter, 
                                FLT_INSTANCE_CONTEXT, 
                                sizeof(FG_INSTANCE_CONTEXT), 
                                NonPagedPool, 
                                &instanceContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x' allocate instance context failed", status);
        goto Cleanup;
    }

    instanceContext->VolumeName.Buffer = NULL;

    status = FgAllocatePushLock(&instanceContext->RulesTableLock);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', allocate resource failed", status);
        goto Cleanup;
    }

    status = FltGetVolumeName(Volume, NULL, &volumeNameSize);
    if (!NT_SUCCESS(status) && STATUS_BUFFER_TOO_SMALL != status) {

        DBG_ERROR("Error(0x%08x), get volume name failed", status);
        goto Cleanup;

    } else {

        status = FgAllocateUnicodeString((USHORT)volumeNameSize, &instanceContext->VolumeName);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("Error(0x%08x), allocate unicode string for get volume name failed", status);
            goto Cleanup;
        }

        status = FltGetVolumeName(Volume, &instanceContext->VolumeName, &volumeNameSize);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("Error(0x%08x), get volume name failed", status);
            goto Cleanup;
        }
    }
    
    RtlInitializeGenericTable(&instanceContext->RulesTable, 
                              FgRuleEntryCompareRoutine, 
                              FgRuleEntryAllocateRoutine, 
                              FgRuleEntryFreeRoutine, 
                              NULL);

    instanceContext->Volume = Volume;
    instanceContext->Instance = Instance;

    FltReferenceContext(instanceContext);
    *InstanceContext = instanceContext;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != instanceContext) {

            if (NULL != instanceContext->RulesTableLock) {
                FgFreePushLock(instanceContext->RulesTableLock);
            }

            if (NULL != instanceContext->VolumeName.Buffer) {
                FgFreeUnicodeString(&instanceContext->VolumeName);
            }
        }
    }
    
    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

    return status;
}

_Check_return_
NTSTATUS
FgSetInstanceContext(
    _In_ PFLT_INSTANCE Instance,
    _In_ FLT_SET_CONTEXT_OPERATION Operation,
    _In_ PFLT_CONTEXT NewContext,
    _Outptr_opt_result_maybenull_ PFLT_CONTEXT* OldContext
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();
    
    //
    // Set instance context.
    //
    status = FltSetInstanceContext(Instance, 
                                   Operation, 
                                   NewContext, 
                                   OldContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', set instance context failed", status);
        return status;
    }
    
    //
    // Add instance context to global list.
    //
    ExAcquireFastMutex(&Globals.InstanceContextListMutex);
    InsertHeadList(&Globals.InstanceContextList, &((PFG_INSTANCE_CONTEXT)NewContext)->List);
    ExReleaseFastMutex(&Globals.InstanceContextListMutex);

    return status;
}

_IRQL_requires_max_(APC_LEVEL)
_Check_return_
NTSTATUS
FgGetInstanceContextFromVolumeName(
    _In_ PUNICODE_STRING VolumeName,
    _Outptr_ PFG_INSTANCE_CONTEXT* InstanceContext
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PFLT_VOLUME volume = NULL;
    PFLT_INSTANCE instance = NULL;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;

    PAGED_CODE();

    if (NULL == VolumeName) return STATUS_INVALID_PARAMETER_1;
    if (NULL == InstanceContext) return STATUS_INVALID_PARAMETER_2;
    
    status = FltGetVolumeFromName(Globals.Filter, VolumeName, &volume);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: '0x%08x', get volume from name failed", status);
        goto Cleanup;
    }

    status = FltGetVolumeInstanceFromName(Globals.Filter, volume, NULL, &instance);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: '0x%08x', get volume instance from name failed", status);
        goto Cleanup;
    }

    status = FltGetInstanceContext(instance, &instanceContext);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: '0x%08x', get instance context failed", status);
        goto Cleanup;
    }

    FltReferenceContext(instanceContext);
    *InstanceContext = instanceContext;

Cleanup:

    if (NULL != volume) {
        FltObjectDereference(volume);
    }

    if (NULL != instance) {
        FltObjectDereference(instance);
    }

    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

    return status;
}

VOID
FgCleanupInstanceContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
    ) 
/*++

Routine Description:

    This routine is called by the filter manager before freeing any of the minifilter
    driver's contexts of that type.

    In this routine, the driver has to perform any needed cleanup, such as freeing
    additional memory that the minifilter driver allocated inside the context structure.

    We delete the cache table if the file system supports one.

Arguments:

    Context     - Pointer to the minifilter driver's portion of the context.

    ContextType - Supposed to be FLT_INSTANCE_CONTEXT (win8 or later).

Return Value:

    None

--*/
{
    PFG_INSTANCE_CONTEXT instanceContext = (PFG_INSTANCE_CONTEXT)Context;

    PAGED_CODE();

    FLT_ASSERT(NULL != Context);
    FLT_ASSERT(FLT_INSTANCE_CONTEXT == ContextType);

    DBG_TRACE("Volume '%wZ' instance cleanup", &instanceContext->VolumeName);

    //
    // Remove instance context from global list.
    //
    ExAcquireFastMutex(&Globals.InstanceContextListMutex);
    RemoveEntryList(&instanceContext->List);
    ExReleaseFastMutex(&Globals.InstanceContextListMutex);

    //
    // Cleanup all rules in instance context.
    //
    FgCleanupRules(&instanceContext->RulesTable, instanceContext->RulesTableLock, NULL);

    if (NULL != instanceContext->VolumeName.Buffer) {
        FgFreeUnicodeString(&instanceContext->VolumeName);
    }

    //
    // Delete and free resource.
    //
    if (NULL != instanceContext->RulesTableLock) {
        FgFreePushLock(instanceContext->RulesTableLock);
    }

    DBG_TRACE("Instance context cleanup finished");
}

/*-------------------------------------------------------------
    File context structure and routines.
-------------------------------------------------------------*/

VOID
FgCleanupFileContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
    )
{
    PFG_FILE_CONTEXT fileContext = (PFG_FILE_CONTEXT)Context;

    FLT_ASSERT(FLT_FILE_CONTEXT == ContextType);

    DBG_TRACE("Cleanup file context, address: '%p'", Context);

    if (NULL != fileContext->NameInfo) {
        FltReleaseFileNameInformation(InterlockedExchangePointer(&fileContext->NameInfo, NULL));
    }
}
