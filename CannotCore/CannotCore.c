/*
    @File   CannotCore.c
    @Note   Operation callback.

    @Mode   Kernel
    @Author Fxtack
*/

#include "CannotCore.h"


/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
FLT_PREOP_CALLBACK_STATUS
CannotPreOperationCallback (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PAGED_CODE();

    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    FLT_PREOP_CALLBACK_STATUS callbackStatus;
    UNICODE_STRING newFileName;

    // Initialize defaults
    status = STATUS_SUCCESS;
    callbackStatus = FLT_PREOP_SUCCESS_NO_CALLBACK; // pass through - default is no post op callback

    RtlInitUnicodeString(&newFileName, NULL);
    
    // Check if this is a paging file as we don't want to redirect
    // the location of the paging file.
    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE)) {
        goto Cleanup;
    }
    
    // We are not allowing volume opens to be reparsed in the sample.
    if (FlagOn(Data->Iopb->TargetFileObject->Flags, FO_VOLUME_OPEN)) {
        goto Cleanup;
    }

    //
    // SimRep does not honor the FILE_OPEN_REPARSE_POINT create option. For a
    // symbolic the caller would pass this flag, for example, in order to open
    // the link for deletion. There is no concept of deleting the mapping for
    // this filter so it is not clear what the purpose of honoring this flag
    // would be.
    //

    // Don't reparse an open by ID because it is not possible to determine create path intent.
    if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID)) {
        goto Cleanup;
    }

    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY)) {

        // This is a prelude to a rename or hard link creation but the filter
        // is NOT configured to filter these operations. To perform the operation
        // successfully and in a consistent manner this create must not trigger
        // a reparse. Pass through the create without attempting any redirection.
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

        // Note that we use FLT_FILE_NAME_QUERY_DEFAULT when querying the
        // filename. In the precreate the filename should not be in filter
        // manager's name cache so there is no point looking there.
        status = FltGetFileNameInformation(Data,
                                           FLT_FILE_NAME_OPENED |
                                           FLT_FILE_NAME_QUERY_DEFAULT,
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

    KdPrint(("CannotCore!%s: Operation file path: '%wZ'\n", __func__, nameInfo->Name));

    switch(MatchConfig(&nameInfo->Name)) {
    case CannotTypeNothing:
        break;
    case CannotTypeReadOnly:
        Data->IoStatus.Status = STATUS_NOT_FOUND;
        callbackStatus = FLT_PREOP_COMPLETE;
        break;
    case CannotTypeAccessDenied:
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        callbackStatus = FLT_PREOP_COMPLETE;
        break;
    case CannotTypeRedirect:
        KdPrint(("CannotCore!%s: Cannot type not support yet\n", __func__));
        break;
    default:
        KdPrint(("CannotCore!%s: Unknown Cannot type\n", __func__));
    }

Cleanup:

    if (nameInfo != NULL) {
        FltReleaseFileNameInformation(nameInfo);
    }

    if (!NT_SUCCESS(status)) {
        // An error occurred, fail the open
        Data->IoStatus.Status = status;
        callbackStatus = FLT_PREOP_COMPLETE;
    }

    return callbackStatus;
}


/*++

Routine Description:

    This routine is the post-operation completion routine for this
    miniFilter.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
FLT_POSTOP_CALLBACK_STATUS
CannotPostOperationCallback (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}


/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
FLT_PREOP_CALLBACK_STATUS
CannotPreOperationNoPostOperationCallback (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
) {
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
