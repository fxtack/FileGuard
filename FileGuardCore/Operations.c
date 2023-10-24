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

    Operations.c

Abstract:

    Definitions of minifilter IRP callbacks.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Operations.h"

FLT_PREOP_CALLBACK_STATUS
FgPreCreateOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for 'IRP_MJ_CREATE'.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.
    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.
    CompletionContext - The context for the completion routine for this
                        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS                   status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS  callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PFG_INSTANCE_CONTEXT       instanceContext = NULL;
    UNICODE_STRING             newFileName = { 0 };
    PFG_COMPLETION_CONTEXT     completionContext = NULL;
    FG_RULE_CLASS              matchedRuleClass = 0;

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PAGED_CODE();

    RtlInitUnicodeString(&newFileName, NULL);

    // Check if this is a paging file as we don't want to redirect
    // the location of the paging file.
    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE) ||
        FlagOn(Data->Iopb->TargetFileObject->Flags, FO_VOLUME_OPEN) ||
        FlagOn(Data->Iopb->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID)) {

        goto Cleanup;
    }

    status = FltGetFileNameInformation(Data,
                                       FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT,
                                       &nameInfo);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', get file name information failed", status);
        goto Cleanup;
    }

    // Parse the filename information
    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', parse file name information failed", status);
        goto Cleanup;
    }

    //
    // Get instance context.
    //
    status = FltGetInstanceContext(FltObjects->Instance, &instanceContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', get instance context failed", status);
        goto Cleanup;
    }

    //
    // Allocate completion context.
    //
    status = FgAllocateBuffer(&completionContext, POOL_FLAG_PAGED, sizeof(FG_COMPLETION_CONTEXT));
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("Error(0x%08x), allocate create callback context failed", status);
        goto Cleanup;
    }

    //
    // Match rule from rules table.
    //
    status = FgMatchRule(&instanceContext->RulesTable, 
                         instanceContext->RulesTableLock,
                         &nameInfo->Name, 
                         &matchedRuleClass);
    if (STATUS_NOT_FOUND == status) {

        //
        // No rule matched.
        //
        status = STATUS_SUCCESS;

    } else if (!NT_SUCCESS(status)) {

        DBG_ERROR("Error(0x%08x), match rule error by file path index: '%wZ'", status, &nameInfo->Name);
        goto Cleanup;
    }

    switch (matchedRuleClass) {
    case RULE_ACCESS_DENIED:

        status = STATUS_ACCESS_DENIED;
        goto Cleanup;

    case RULE_READONLY:
       
        //
        // Continue post callback handle.
        //
        break;

    default:

        //
        // Rule class unknown.
        //
        status = STATUS_UNSUCCESSFUL;
        goto Cleanup;
    }

    completionContext->MajorFunction = Data->Iopb->MajorFunction;
    completionContext->Create.RuleClass = matchedRuleClass;

    FltReferenceContext(instanceContext);
    completionContext->Create.InstanceContext = instanceContext;

    *CompletionContext = completionContext;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != completionContext) {
            FgFreeBuffer(completionContext);
        }

        //
        // An error occurred, fail the open.
        //
        Data->IoStatus.Status = status;
        Data->IoStatus.Information = 0;
        callbackStatus = FLT_PREOP_COMPLETE;

        FltSetCallbackDataDirty(Data);
    }

    if (NULL != nameInfo) {
        FltReleaseFileNameInformation(nameInfo);
    }

    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

    return callbackStatus;
}

FLT_POSTOP_CALLBACK_STATUS
FgPostCreateOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for 'IRP_MJ_CREATE'.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags             - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    FLT_POSTOP_CALLBACK_STATUS callbackStatus = FLT_POSTOP_FINISHED_PROCESSING;
    PFG_COMPLETION_CONTEXT completionContext = NULL;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;
    PFG_STREAM_CONTEXT streamContext = NULL;
    FG_RULE_CLASS ruleClass = 0;
    BOOLEAN streamContextCreated = FALSE;

    UNREFERENCED_PARAMETER(FltObjects);

    if (FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING)) {
        status = STATUS_DEVICE_REMOVED;
        goto Cleanup;
    }

    PAGED_CODE();

    FLT_ASSERT(NULL != CompletionContext);

    completionContext = CompletionContext;
    instanceContext = completionContext->Create.InstanceContext;
    ruleClass = completionContext->Create.RuleClass;

    //
    // Find or create a stream context from file object to update or save 
    // rule class.
    //
    status = FgFindOrCreateStreamContext(Data, 
                                         FltObjects->Instance, 
                                         TRUE, 
                                         &streamContext, 
                                         &streamContextCreated);
    if (!NT_SUCCESS(status)) {
        
        DBG_ERROR("Error(0x%08x), find or create stream context failed", status);
        goto Cleanup;

    } else if (streamContextCreated && NULL != streamContext) {

        //
        // Create a new stream context for file object.
        //

        FltReferenceContext(instanceContext);
        streamContext->InstanceContext = instanceContext;
        streamContext->RuleClass = ruleClass;

        status = FltSetStreamContext(Data->Iopb->TargetInstance,
                                     Data->Iopb->TargetFileObject,
                                     FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                     streamContext,
                                     NULL);
        if (!NT_SUCCESS(status)) {

            FltReleaseContext(instanceContext);

            DBG_ERROR("Error(0x%08x), set stream context failed", status);
            goto Cleanup;
        }

    } else if (!streamContextCreated && NULL != streamContext) {

        //
        // A stream context already setup, update rule class.
        //

        streamContext->RuleClass = ruleClass;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {

        FltCancelFileOpen(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject);

        Data->IoStatus.Status = status;
        Data->IoStatus.Information = 0;

        FltSetCallbackDataDirty(Data);
    }

    if (NULL != streamContext) {
        FltReleaseContext(streamContext);
    }

    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

    if (NULL != CompletionContext) {
        FgFreeBuffer(CompletionContext);
    }

    return callbackStatus;
}

FLT_PREOP_CALLBACK_STATUS
FgPreWriteOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for 'IRP_MJ_WRITE'.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The context for the completion routine for this
                        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
FgPostWriteOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for 'IRP_MJ_WRITE'.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags             - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    if (FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING)) {

        status = STATUS_DEVICE_REMOVED;
        goto Cleanup;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {

        FltCancelFileOpen(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject);
        Data->IoStatus.Information = 0;
        Data->IoStatus.Status = status;
        FltSetCallbackDataDirty(Data);
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
FgPreSetInformationOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for 'IRP_MJ_SET_INFORMATION'.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The context for the completion routine for this
                        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
FgPostSetInformationOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for 'IRP_MJ_SET_INFORMATION'.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags             - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
FgPreCleanupOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for 'IRP_MJ_CLEANUP'.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The context for the completion routine for this
                        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
FgPostCleanupOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for 'IRP_MJ_CLEANUP'.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data              - Pointer to the filter callbackData that is passed to us.

    FltObjects        - Pointer to the FLT_RELATED_OBJECTS data structure containing
                        opaque handles to this filter, instance, its associated volume and
                        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags             - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}
