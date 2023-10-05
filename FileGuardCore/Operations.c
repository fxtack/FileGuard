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

    Contains minifilter IRP callbacks.

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
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    UNICODE_STRING newFileName;

    RtlInitUnicodeString(&newFileName, NULL);

    // Check if this is a paging file as we don't want to redirect
    // the location of the paging file.
    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE) ||
        FlagOn(Data->Iopb->TargetFileObject->Flags, FO_VOLUME_OPEN) ||
        FlagOn(Data->Iopb->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID)) {
        goto Cleanup;
    }

    //  Get the name information.
    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY)) {

        // The SL_OPEN_TARGET_DIRECTORY flag indicates the caller is attempting
        // to open the target of a rename or hard link creation operation. We
        // must clear this flag when asking fltmgr for the name or the result
        // will not include the final component. We need the full path in order
        // to compare the name to our mapping.
        ClearFlag(Data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY);

        // Get the filename as it appears below this filter. Note that we use
        // FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY when querying the filename
        // so that the filename as it appears below this filter does not end up
        // in filter manager's name cache.
        status = FltGetFileNameInformation(Data,
            FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY,
            &nameInfo);

        // Restore the SL_OPEN_TARGET_DIRECTORY flag so the create will proceed
        // for the target. The file systems depend on this flag being set in
        // the target create in order for the subsequent SET_INFORMATION
        // operation to proceed correctly.
        SetFlag(Data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY);


    } else {

        status = FltGetFileNameInformation(Data,
                                           FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT,
                                           &nameInfo);
    }

    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // Parse the filename information
    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

Cleanup:

    if (NULL != nameInfo) {
        FltReleaseFileNameInformation(nameInfo);
    }

    if (!NT_SUCCESS(status)) {

        // An error occurred, fail the open.
        Data->IoStatus.Status = status;
        Data->IoStatus.Information = 0;
        callbackStatus = FLT_PREOP_COMPLETE;
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
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    return FLT_POSTOP_FINISHED_PROCESSING;
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
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

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
