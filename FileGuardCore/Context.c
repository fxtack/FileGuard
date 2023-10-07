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

    { FLT_STREAM_CONTEXT,
      0,
      FgCleanupStreamContext,
      sizeof(FG_STREAM_CONTEXT),
      FG_STREAM_CONTEXT_PAGED_MEM_TAG },

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

    //
    // Initialize instance context attributes.
    //

    instanceContext->Instance = Instance;
    instanceContext->Volume = Volume;

    RtlInitializeGenericTable(&instanceContext->RulesTable, 
                              FgRuleEntryCompareRoutine, 
                              FgRuleEntryAllocateRoutine, 
                              FgRuleEntryFreeRoutine, 
                              NULL);

    status = FgAllocateResource(&instanceContext->RulesTableResource);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', allocate resource failed", status);
        goto Cleanup;
    }

    *InstanceContext = instanceContext;

Cleanup:
    
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

    //
    // Remove instance context from global list.
    //
    ExAcquireFastMutex(&Globals.InstanceContextListMutex);

    RemoveEntryList(&instanceContext->List);

    ExReleaseFastMutex(&Globals.InstanceContextListMutex);

    //
    // Cleanup all rules in instance context.
    //
    FgCleanupRules(&instanceContext->RulesTable, instanceContext->RulesTableResource);

    //
    // Delete and free resource.
    //
    FgFreeResource(instanceContext->RulesTableResource);
}

/*-------------------------------------------------------------
    Stream context structure and routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgCreateStreamContext(
    _Outptr_ PFG_STREAM_CONTEXT* StreamContext
    )
/*++

Routine Description:

    This routine creates a new stream context.

Arguments:

    StreamContext - Returns the stream context.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER(StreamContext);

    return STATUS_NOT_SUPPORTED;
}

_Check_return_
NTSTATUS
FgFindOrCreateStreamContext(
    _In_ PFLT_CALLBACK_DATA Data,
    _In_ BOOLEAN CreateIfNotFound,
    _Outptr_ PFG_STREAM_CONTEXT* StreamContext,
    _Out_opt_ PBOOLEAN ContextCreated
    )
/*++

Routine Description:

    This routine finds the stream context for the target stream.
    Optionally, if the context does not exist this routing creates
    a new one and attaches the context to the stream.

Arguments:

    Data             - Supplies a pointer to the callbackData which
                       declares the requested operation.

    CreateIfNotFound - Supplies if the stream must be created if missing

    StreamContext    - Returns the stream context

    ContextCreated   - Returns if a new context was created

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_STREAM_CONTEXT streamContext = NULL;
    PFG_STREAM_CONTEXT oldStreamContext = NULL;

    PAGED_CODE();

    *StreamContext = NULL;
    if (NULL != ContextCreated) *ContextCreated = FALSE;

    //
    // Get stream context from file object.
    //
    status = FltGetStreamContext(Data->Iopb->TargetInstance, 
                                 Data->Iopb->TargetFileObject, 
                                 &streamContext);
    if (!NT_SUCCESS(status) && (status == STATUS_NOT_FOUND) && CreateIfNotFound) {

        //
        // Allocate and initialize a new stream context.
        //
        status = FgCreateStreamContext(&streamContext);
        if (!NT_SUCCESS(status)) {

            return status;
        }

        //
        // Set the stream context to file.
        //
        status = FltSetStreamContext(Data->Iopb->TargetInstance, 
                                     Data->Iopb->TargetFileObject, 
                                     FLT_SET_CONTEXT_KEEP_IF_EXISTS, 
                                     streamContext, 
                                     &oldStreamContext);
        if (!NT_SUCCESS(status)) {

            FltReleaseContext(streamContext);
            if (STATUS_FLT_CONTEXT_ALREADY_DEFINED != status) {
                return status;
            }

            streamContext = oldStreamContext;
            status = STATUS_SUCCESS;
        } else {

            if (NULL != ContextCreated) *ContextCreated = TRUE;
        }
    }

    *StreamContext = streamContext;

    return status;
}

VOID
FgCleanupStreamContext(
    _In_ PFLT_CONTEXT StreamContext,
    _In_ FLT_CONTEXT_TYPE ContextType
    )
/*++

Routine Description:

    This routine is called by the FilterManager when the context
    reference counter reaches zero and the context should be freed.

Arguments:

    Context     - Context to cleanup.

    ContextType - Type of the context to be cleaned up.

Return value:

    None.

--*/
{
    UNREFERENCED_PARAMETER(StreamContext);
    UNREFERENCED_PARAMETER(ContextType);
}