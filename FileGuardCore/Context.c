#include "FileGuardCore.h"
#include "Context.h"

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

_Check_return_
NTSTATUS
FgCreateInstanceContext(
    _In_     PFLT_FILTER Filter,
    _In_     PFLT_VOLUME Volume,
    _In_     PFLT_INSTANCE Instance,
    _Outptr_ PFG_INSTANCE_CONTEXT* InstanceContext
    ) 
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


_Check_return_
NTSTATUS
FgCreateStreamContext(
    _Outptr_ PFG_STREAM_CONTEXT* StreamContext
    )
/*++

Routine Description:

    This routine creates a new stream context

Arguments:

    StreamContext         - Returns the stream context

Return Value:

    Status

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

    Data                  - Supplies a pointer to the callbackData which
                            declares the requested operation.
    CreateIfNotFound      - Supplies if the stream must be created if missing
    StreamContext         - Returns the stream context
    ContextCreated        - Returns if a new context was created

Return Value:

    Status

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

Summary:

    This function is called by the FilterManager when the context
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