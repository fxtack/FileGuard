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

#pragma warning(disable: 4057)

FLT_PREOP_CALLBACK_STATUS
FgPreCreateCallback(
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
    PFG_COMPLETION_CONTEXT completionContext = NULL;
    ULONG ruleCode = 0ul;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(FltObjects);

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
    
    // Ignore volume root directory operations.
    if (0 == nameInfo->FinalComponent.Length) {
        callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto Cleanup;
    }
    
    try {
        ruleCode = FgMatchRule(&Globals.RulesList, Globals.RulesListLock, &nameInfo->Name);
        if (RULE_ACCESS_DENIED == ruleCode) {
            SET_CALLBACK_DATA_STATUS(Data, STATUS_ACCESS_DENIED);
            callbackStatus = FLT_PREOP_COMPLETE;
            goto Cleanup;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        status = GetExceptionCode();
        LOG_ERROR("NTSTATUS: 0x%08x, an exception occurred while matching the rules", status);
        goto Cleanup;
    }

    //
    // Allocate and setup completion context.
    //
    status = FgAllocateCompletionContext(Data->Iopb->MajorFunction, &completionContext);
    if (!NT_SUCCESS(status)) {
        DBG_ERROR("Error(0x%08x), allocate create callback context failed", status);
        goto Cleanup;

    } else {

        FltReferenceFileNameInformation(nameInfo);
        completionContext->Create.FileNameInfo = nameInfo;
        completionContext->Create.RuleCode = ruleCode;

        *CompletionContext = completionContext;
    }
    
Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    if (NULL != nameInfo) {
        FltReleaseFileNameInformation(nameInfo);
    }

    return callbackStatus;
}

FLT_POSTOP_CALLBACK_STATUS
FgPostCreateCallback(
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
    PFG_COMPLETION_CONTEXT completionContext = CompletionContext;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PFG_FILE_CONTEXT fileContext = NULL, oldFileContext = NULL;

    UNREFERENCED_PARAMETER(FltObjects);

    PAGED_CODE();

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_CREATE == Data->Iopb->MajorFunction);
    FLT_ASSERT(NULL != completionContext);
    FLT_ASSERT(IRP_MJ_CREATE == completionContext->MajorFunction);
    FLT_ASSERT(NULL != completionContext->Create.FileNameInfo);

    nameInfo = completionContext->Create.FileNameInfo;

    if (FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING)) {
        status = STATUS_DEVICE_REMOVED;
        goto Cleanup;
    }
    
    if (!NT_SUCCESS(Data->IoStatus.Status)) {
        DBG_WARNING("File '%wZ' operation result status: 0x%08x", &nameInfo->Name, Data->IoStatus.Status);
        goto Cleanup;
    }

    status = FltGetFileContext(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject, &fileContext);
    if (!NT_SUCCESS(status) && STATUS_NOT_FOUND != status) {
        DBG_ERROR("NTSTATUS: '0x%08x', get file context failed", status);
        goto Cleanup;

    } else if (STATUS_NOT_FOUND == status) {

        status = FltAllocateContext(Globals.Filter, 
                                    FLT_FILE_CONTEXT, 
                                    sizeof(FG_FILE_CONTEXT), 
                                    NonPagedPool,
                                    &fileContext);
        if (!NT_SUCCESS(status)) {
            DBG_ERROR("NTSTATUS: '0x%08x', allocate file context failed", status);
            goto Cleanup;
        } else {
            RtlZeroMemory(fileContext, sizeof(FG_FILE_CONTEXT));
        }

        status = FltSetFileContext(Data->Iopb->TargetInstance,
                                   Data->Iopb->TargetFileObject,
                                   FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                   fileContext,
                                   &oldFileContext);
        if (!NT_SUCCESS(status) && STATUS_FLT_CONTEXT_ALREADY_DEFINED != status) {
            DBG_ERROR("NTSTATUS: '0x%08x', set file context failed", status);
            goto Cleanup;

        } else if (STATUS_FLT_CONTEXT_ALREADY_DEFINED == status) {

#ifdef DBG

            DBG_TRACE("File context already defined, file context: %p, for '%wZ'", oldFileContext, &nameInfo->Name);

            if (oldFileContext->RuleCode != completionContext->Create.RuleCode) {
                DBG_WARNING("File context %p rule updated from 0x%08x to 0x%08x", 
                            oldFileContext, 
                            oldFileContext->RuleCode,
                            completionContext->Create.RuleCode);
            }
#endif

            InterlockedExchange(&oldFileContext->RuleCode, completionContext->Create.RuleCode); 

        } else {

            DBG_TRACE("File context '%p' setup for file '%wZ'", fileContext, &nameInfo->Name);

            FltReferenceFileNameInformation(nameInfo);
            InterlockedExchangePointer(&fileContext->FileNameInfo, nameInfo);
        }
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        FltCancelFileOpen(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject);
        SET_CALLBACK_DATA_STATUS(Data, status);
    }

    if (NULL != nameInfo) {
        FltReleaseFileNameInformation(nameInfo);
    }

    if (NULL != fileContext) {
        FltReleaseContext(fileContext);
    }

    if (NULL != oldFileContext) {
        FltReleaseContext(oldFileContext);
    }

    if (NULL != completionContext) {
        FgFreeCompletionContext(completionContext);
    }

    return callbackStatus;
}

FLT_PREOP_CALLBACK_STATUS
FgPreWriteCallback(
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
    PFG_FILE_CONTEXT fileContext = NULL;

    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(FltObjects);

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_WRITE == Data->Iopb->MajorFunction);

    //
    // Get file context.
    //
    status = FltGetFileContext(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject, &fileContext);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: '0x%08x', get file context", status);
        goto Cleanup;
    }

    switch (fileContext->RuleCode) {
    case RULE_ACCESS_DENIED:
        SET_CALLBACK_DATA_STATUS(Data, STATUS_ACCESS_DENIED);
        callbackStatus = FLT_PREOP_COMPLETE;
        break;

    case RULE_READONLY:
        SET_CALLBACK_DATA_STATUS(Data, STATUS_MEDIA_WRITE_PROTECTED);
        callbackStatus = FLT_PREOP_COMPLETE;
        break;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    if (NULL != fileContext) {
        FltReleaseContext(fileContext);
    }

    return callbackStatus;
}

FLT_PREOP_CALLBACK_STATUS
FgPreSetInformationCallback(
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
    FLT_PREOP_CALLBACK_STATUS callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFG_FILE_CONTEXT fileContext = NULL;
    PFLT_FILE_NAME_INFORMATION renameNameInfo = NULL;
    PFILE_RENAME_INFORMATION renameInfo = NULL;
    ULONG ruleCode = 0ul;

    UNREFERENCED_PARAMETER(CompletionContext);

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_SET_INFORMATION == Data->Iopb->MajorFunction);

    //
    // Get stream context.
    //
    status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, &fileContext);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, get file context failed", status);
        goto Cleanup;
    } 

    switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass) {
    case FileRenameInformation:
    case FileRenameInformationEx:

        switch (fileContext->RuleCode) {
        case RULE_READONLY:
            SET_CALLBACK_DATA_STATUS(Data, STATUS_MEDIA_WRITE_PROTECTED);
            callbackStatus = FLT_PREOP_COMPLETE;
            break;

        case RULE_ACCESS_DENIED:
            SET_CALLBACK_DATA_STATUS(Data, STATUS_ACCESS_DENIED);
            callbackStatus = FLT_PREOP_COMPLETE;
            break;
        }

        renameInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
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

        try {
            ruleCode = FgMatchRule(&Globals.RulesList, Globals.RulesListLock, &renameNameInfo->Name);
            switch (ruleCode) {
            case RULE_READONLY:
                SET_CALLBACK_DATA_STATUS(Data, STATUS_MEDIA_WRITE_PROTECTED);
                callbackStatus = FLT_PREOP_COMPLETE;
                break;

            case RULE_ACCESS_DENIED:
                SET_CALLBACK_DATA_STATUS(Data, STATUS_ACCESS_DENIED);
                callbackStatus = FLT_PREOP_COMPLETE;
                break;
            }

        } except(EXCEPTION_EXECUTE_HANDLER) {

            status = GetExceptionCode();
            LOG_ERROR("NTSTATUS: 0x%08x, an exception occurred while matching the rules", status);
            goto Cleanup;
        }

    case FileDispositionInformation:
    case FileDispositionInformationEx:

        switch (fileContext->RuleCode) {
        case RULE_READONLY:
            SET_CALLBACK_DATA_STATUS(Data, STATUS_MEDIA_WRITE_PROTECTED);
            callbackStatus = FLT_PREOP_COMPLETE;
            break;

        case RULE_ACCESS_DENIED:
            SET_CALLBACK_DATA_STATUS(Data, STATUS_ACCESS_DENIED);
            callbackStatus = FLT_PREOP_COMPLETE;
            break;
        }
        
        break;
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    if (NULL != renameNameInfo) {
        FltReleaseContext(renameNameInfo);
    }

    if (NULL != fileContext) {
        FltReleaseContext(fileContext);
    }

    return callbackStatus;
}

FLT_PREOP_CALLBACK_STATUS
FgPreFileSystemControlCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for 'IRP_MJ_FILE_SYSTEM_CONTROL'.

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
    PFG_FILE_CONTEXT fileContext = NULL;
    ULONG fsctlCode = 0ul;

    UNREFERENCED_PARAMETER(CompletionContext);

    FLT_ASSERT(NULL != Data);
    FLT_ASSERT(NULL != Data->Iopb);
    FLT_ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == Data->Iopb->MajorFunction);

    //
    // Get stream context.
    //
    status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, &fileContext);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, get file context failed", status);
        goto Cleanup;
    }

    fsctlCode = Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode;
    switch (fsctlCode) {
    case FSCTL_SET_SPARSE:
    case FSCTL_SET_REPARSE_POINT:
    case FSCTL_SET_REPARSE_POINT_EX:
    case FSCTL_DELETE_REPARSE_POINT:

        switch (fileContext->RuleCode) {
        case RULE_ACCESS_DENIED:
            SET_CALLBACK_DATA_STATUS(Data, STATUS_ACCESS_DENIED);
            callbackStatus = FLT_PREOP_COMPLETE;
            break;

        case RULE_READONLY:
            SET_CALLBACK_DATA_STATUS(Data, STATUS_MEDIA_WRITE_PROTECTED);
            callbackStatus = FLT_PREOP_COMPLETE;
            break;
        }
    }

Cleanup:

    if (!NT_SUCCESS(status)) {
        SET_CALLBACK_DATA_STATUS(Data, status);
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    if (NULL != fileContext) {
        FltReleaseContext(fileContext);
    }

    return callbackStatus;
}
