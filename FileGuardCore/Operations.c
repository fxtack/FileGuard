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
    NTSTATUS status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS callbackStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;
    PFG_COMPLETION_CONTEXT completionContext = NULL;
    FG_RULE_CLASS matchedRuleClass = 0;

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PAGED_CODE();

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_CREATE == Data->Iopb->MajorFunction);

    // Check if this is a paging file as we don't want to redirect
    // the location of the paging file.
    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE) ||
        FlagOn(Data->Iopb->TargetFileObject->Flags, FO_VOLUME_OPEN) ||
        FlagOn(Data->Iopb->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID)) {

        callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto Cleanup;
    }

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', get file name information failed", status);
        goto Cleanup;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', parse file name information failed", status);
        goto Cleanup;
    }

    //
    // Ignore volume root directory  operations.
    //
    if (0 == nameInfo->FinalComponent.Length) {
        callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto Cleanup;
    }

    status = FltGetInstanceContext(FltObjects->Instance, &instanceContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', get instance context failed", status);
        goto Cleanup;
    }
    
    //status = FgMatchRule(&instanceContext->RulesTable, 
    //                     instanceContext->RulesTableLock,
    //                     &nameInfo->Name, 
    //                     &matchedRuleClass);
    //if (!NT_SUCCESS(status) && STATUS_NOT_FOUND == status) {

    //    DBG_ERROR("Error(0x%08x), match rule error by file path index: '%wZ'", status, &nameInfo->Name);
    //    goto Cleanup;

    //} else if (NT_SUCCESS(status)) {

    //    switch (matchedRuleClass) {
    //    case RULE_ACCESS_DENIED:
    //        status = STATUS_ACCESS_DENIED;
    //        goto Cleanup;

    //    case RULE_READONLY:
    //        break;

    //    default:
    //        status = STATUS_UNSUCCESSFUL;
    //        goto Cleanup;
    //    }
    //}
    
    status = FgAllocateBuffer(&completionContext, POOL_FLAG_PAGED, sizeof(FG_COMPLETION_CONTEXT));
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("Error(0x%08x), allocate create callback context failed", status);
        goto Cleanup;
    }

    completionContext->MajorFunction = Data->Iopb->MajorFunction;
    completionContext->Create.RuleClass = matchedRuleClass;

    FltReferenceFileNameInformation(nameInfo);
    completionContext->Create.FileNameInfo = nameInfo;

    *CompletionContext = completionContext;

Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
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
    PFLT_FILE_NAME_INFORMATION fileNameInfo = NULL;
    PFG_STREAM_CONTEXT streamContext = NULL, oldStreamContext = NULL;
    FG_RULE_CLASS ruleClass = 0;

    UNREFERENCED_PARAMETER(FltObjects);

    PAGED_CODE();

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_CREATE == Data->Iopb->MajorFunction);

    if (FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING)) {
        status = STATUS_DEVICE_REMOVED;
        goto Cleanup;
    }

    FLT_ASSERT(NULL != CompletionContext);
    completionContext = CompletionContext;
    fileNameInfo = completionContext->Create.FileNameInfo;
    ruleClass = completionContext->Create.RuleClass;

    //
    // We are only interested in successfully opened files.
    // Only after the file is successfully opened can the stream context be set.
    //
    if (!NT_SUCCESS(Data->IoStatus.Status)) {
        goto Cleanup;
    }

    status = FltGetStreamContext(Data->Iopb->TargetInstance,
                                 Data->Iopb->TargetFileObject, 
                                 &streamContext);
    if (STATUS_NOT_FOUND == status) {

        //
        // Allocate and setup stream context.
        //

        status = FltAllocateContext(Globals.Filter, 
                                    FLT_STREAM_CONTEXT, 
                                    sizeof(FG_STREAM_CONTEXT), 
                                    PagedPool, 
                                    &streamContext);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', allocate stream context failed", status);
            goto Cleanup;
        }

        status = FltSetStreamContext(Data->Iopb->TargetInstance,
                                     Data->Iopb->TargetFileObject,
                                     FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                     streamContext,
                                     &oldStreamContext);
        if (!NT_SUCCESS(status) && STATUS_FLT_CONTEXT_ALREADY_DEFINED != status) {
            DBG_ERROR("NTSTATUS: '0x%08x', set stream context failed", status);
            goto Cleanup;
        }

    } else if (!NT_SUCCESS(status)) {

        DBG_ERROR("NTSTATUS: '0x%08x', get stream context failed", status);
        goto Cleanup;
    }
    
    if (STATUS_FLT_CONTEXT_ALREADY_DEFINED == status && NULL != oldStreamContext) {

        oldStreamContext->RuleClass = ruleClass;

    } else if (NULL != streamContext) {

        streamContext->RuleClass = ruleClass;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        FltCancelFileOpen(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject);
        SET_CALLBACK_DATA_STATUS(Data, status);
    }

    if (NULL != streamContext) {
        FltReleaseContext(streamContext);
    }

    if (NULL != oldStreamContext) {
        FltReleaseContext(oldStreamContext);
    }

    if (NULL != fileNameInfo) {
        FltReleaseFileNameInformation(fileNameInfo);
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
    NTSTATUS status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFG_STREAM_CONTEXT streamContext = NULL;

    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(FltObjects);

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_WRITE == Data->Iopb->MajorFunction);

    //
    // Get stream context.
    //
    status = FltGetStreamContext(Data->Iopb->TargetInstance, 
                                 Data->Iopb->TargetFileObject, 
                                 &streamContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("Error(0x%08x), get stream context", status);
        goto Cleanup;
    }

    //
    // The file is read-only.
    //
    if (RULE_READONLY == streamContext->RuleClass) {

        //
        // TODO rule match, add a monitor record.
        //

        status = STATUS_ACCESS_DENIED;
        goto Cleanup;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    if (NULL != streamContext) {
        FltReleaseContext(streamContext);
    }

    return callbackStatus;
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
    NTSTATUS status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS callbackStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
    FILE_INFORMATION_CLASS informationClass;
    PFLT_FILE_NAME_INFORMATION renameNameInfo = NULL, nameInfo = NULL;
    PFG_STREAM_CONTEXT streamContext = NULL;
    PFILE_RENAME_INFORMATION renameInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

    UNREFERENCED_PARAMETER(CompletionContext);

    //
    // Get stream context.
    //
    status = FltGetStreamContext(FltObjects->Instance, 
                                 FltObjects->FileObject, 
                                 &streamContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("NTSTATUS: '0x%08x', get stream context failed", status);
        status = STATUS_UNSUCCESSFUL;
    } else {
        nameInfo = streamContext->NameInfo;
    }

    informationClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;

    switch (informationClass) {
    case FileDispositionInformation:
    case FileDispositionInformationEx:
        
        //
        // Delete file or directory.
        //

        if (RULE_READONLY == streamContext->RuleClass) {
            status = STATUS_ACCESS_DENIED;
            goto Cleanup;
        }

        break;

    case FileRenameInformation:
    case FileRenameInformationEx:

        //
        // Rename file or directory.
        //
        
        status = FltGetDestinationFileNameInformation(Data->Iopb->TargetInstance, 
                                                      Data->Iopb->TargetFileObject, 
                                                      renameInfo->RootDirectory,
                                                      renameInfo->FileName,
                                                      renameInfo->FileNameLength,
                                                      FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
                                                      &renameNameInfo);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', get destination file name information failed", status);
            goto Cleanup;
        }

        DBG_TRACE("Rename source: '%wZ' target: '%wZ'", &nameInfo->Name, &renameNameInfo->Name);

        break;

    default:
        break;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    return callbackStatus;
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

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_CLEANUP == Data->Iopb->MajorFunction);

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

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_CLEANUP == Data->Iopb->MajorFunction);

    return FLT_POSTOP_FINISHED_PROCESSING;
}
