/*
    @File   Message.c
    @Note   NtFreezerCore message handler.

    @Mode   Kernel
    @Author Fxtack
*/

#include "NtFreezerCore.h"

LONG AsMessageException(
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
) {
    NTSTATUS status = ExceptionPointer->ExceptionRecord->ExceptionCode;

    if (!FsRtlIsNtstatusExpected(status) &&
        !AccessingUserBuffer) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}


inline NTSTATUS HandlerQueryConfig(
    _In_reads_bytes_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
) {
    NTSTATUS status = STATUS_SUCCESS;
    PNTFZ_CONFIG qQueryResult;

    if (Input == NULL || InputBytes != sizeof(REQUEST_QUERY_CONFIG) ||
        Output == NULL || OutputBytes != sizeof(RESPONSE_QUERY_CONFIG) ||
        ReturnBytes == NULL)
        status = STATUS_INVALID_PARAMETER;
    goto returnWithStatus;

    try {
        status = QueryConfigFromTable(((PNTFZ_CONFIG)Input)->Path,
                                      (QRESPONSE_QUERY_CONFIG)Output);
        if (!NT_SUCCESS(status) || qQueryResult == NULL) {
            *ReturnBytes = 0;
            goto returnWithStatus;
        }
        *ReturnBytes = sizeof(RESPONSE_QUERY_CONFIG);

    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
        status =  GetExceptionCode();
    }

returnWithStatus:
    return status;
}


inline NTSTATUS HandlerAddConfig(
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes
) {
    if (Input == NULL || InputBytes != sizeof(REQUEST_ADD_CONFIG))
        return STATUS_INVALID_PARAMETER;


    
    return STATUS_SUCCESS;
}


inline NTSTATUS HandlerRemoveConfig(
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes
) {
    if (Input == NULL || InputBytes != sizeof(REQUEST_REMOVE_CONFIG))
        return STATUS_INVALID_PARAMETER;    

    try {
        // Find up configuration and remove it from table.
    } except (AsMessageException(GetExceptionInformation(), TRUE)) {
        return GetExceptionCode();
    }
    return STATUS_SUCCESS;
}


inline VOID HandlerCleanupConfig(
    VOID
) {
    CleanupConfigTable();
}


inline NTSTATUS HandlerGetVersion(
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
) {
    if (Output == NULL || OutputBytes != sizeof(NTFZ_CORE_VERSION)) {
        return STATUS_INVALID_PARAMETER;
    } else if (!IS_ALIGNED(Output, sizeof(ULONG))) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    try {
        ((PNTFZ_CORE_VERSION)Output)->Major = NTFZ_CORE_VERSION_MAJOR;
        ((PNTFZ_CORE_VERSION)Output)->Minor = NTFZ_CORE_VERSION_MINOR;
        ((PNTFZ_CORE_VERSION)Output)->Patch = NTFZ_CORE_VERSION_PATCH;
    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
        return GetExceptionCode();
    }

    *ReturnBytes = sizeof(NTFZ_CORE_VERSION);
    return STATUS_SUCCESS;
}


NTSTATUS NTFZCoreMessageHandlerRoutine(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);

    NTSTATUS status = STATUS_INVALID_PARAMETER;
    PNTFZ_A2CMSG pMsg;

    if ((Input == NULL) ||
        (InputBytes < (FIELD_OFFSET(NTFZ_A2CMSG, MsgType) + sizeof(NTFZ_A2CMSG)))) {

        KdPrint(("NtFreezer!%s", __func__));
        return status;
    } else {
        pMsg = (PNTFZ_A2CMSG)Input;
    }

    switch (pMsg->MsgType) {
    case QueryConfig:
        status = HandlerQueryConfig(Input,
                                    InputBytes,
                                    Output,
                                    OutputBytes,
                                    ReturnBytes);
        KdPrint(("NtFreezer!%s: Handle message type [QueryConfig], status: %x.", __func__, status));

        break;
    case AddConfig:
        status = HandlerAddConfig(Input, InputBytes);
        KdPrint(("NtFreezer!%s: Handle message type [AddConfig], status: %x.", __func__, status));

        break;
    case RemoveConfig:
        status = HandlerRemoveConfig(Input, InputBytes);
        KdPrint(("NtFreezer!%s: Handle message type [RemoveConfig], status£º%x.", __func__, status));

        break;
    case CleanupConfig:
        HandlerCleanupConfig();
        KdPrint(("NtFreezer!%s: Handle message type [CleanupConfig].", __func__));

        break;
    case GetCoreVersion:
        status = HandlerGetVersion(Output, OutputBytes, ReturnBytes);
        KdPrint(("NtFreezer!%s: Handle message type [GetCoreVersion], status: %x.", __func__, status));

        break;
    default:
        KdPrint(("NtFreezer!%s: Unknown message type.", __func__));
    }

    return status;
}

