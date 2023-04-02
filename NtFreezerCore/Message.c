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
    _In_reads_bytes_(QueryConfigSize) PVOID QueryConfig,
    _In_ ULONG QueryConfigSize,
    _Out_writes_bytes_to_opt_(ResultConfigSize, *ReturnSize) PVOID ResultConfig,
    _In_ ULONG ResultConfigSize,
    _Out_ PULONG ReturnSize
) {
    NTSTATUS status = STATUS_SUCCESS;

    if (QueryConfig == NULL || QueryConfigSize != sizeof(REQUEST_QUERY_CONFIG) ||
        ResultConfig == NULL || ResultConfigSize != sizeof(RESPONSE_QUERY_CONFIG) ||
        ReturnSize == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    try {
        status = QueryConfigFromTable((PNTFZ_CONFIG)QueryConfig,
                                      (PNTFZ_CONFIG)ResultConfig);
        if (!NT_SUCCESS(status)) {
            *ReturnSize = 0;
            goto returnWithStatus;
        }
        *ReturnSize = sizeof(RESPONSE_QUERY_CONFIG);

    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
        status =  GetExceptionCode();
    }

returnWithStatus:
    return status;
}


inline NTSTATUS HandlerAddConfig(
    _In_reads_bytes_opt_(AddConfigSize) PVOID AddConfig,
    _In_ ULONG AddConfigSize
) {
    NTSTATUS status = STATUS_SUCCESS;

    if (AddConfig == NULL || AddConfigSize != sizeof(REQUEST_ADD_CONFIG)) {
        KdPrint(("NtFreezerCore!%s: Invalid input buffer.", __func__));
        return STATUS_INVALID_PARAMETER;
    }

    try {
        status = AddConfigToTable((PNTFZ_CONFIG)AddConfig);
    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
        KdPrint(("NtFreezerCore!%s: Get exception.", __func__));
        status = GetExceptionCode();
    }
    
    return status;
}


inline NTSTATUS HandlerRemoveConfig(
    _In_reads_bytes_opt_(RemoveConfigSize) PVOID RemoveConfig,
    _In_ ULONG RemoveConfigSize
) {
    NTSTATUS status = STATUS_SUCCESS;
    if (RemoveConfig == NULL || RemoveConfigSize != sizeof(REQUEST_REMOVE_CONFIG))
        return STATUS_INVALID_PARAMETER;

    try {
        status = RemoveConfigFromTable((PNTFZ_CONFIG)RemoveConfig);
    } except (AsMessageException(GetExceptionInformation(), TRUE)) {
        status = GetExceptionCode();
    }
    return status;
}


inline NTSTATUS HandlerCleanupConfig(
    VOID
) {
    return CleanupConfigTable();
}


inline NTSTATUS HandlerGetVersion(
    _Out_writes_bytes_to_opt_(CoreVersionSize, *ReturnSize) PVOID CoreVersion,
    _In_ ULONG CoreVersionSize,
    _Out_ PULONG ReturnSize
) {
    if (CoreVersion == NULL || CoreVersionSize != sizeof(NTFZ_CORE_VERSION)) {
        return STATUS_INVALID_PARAMETER;
    } else if (!IS_ALIGNED(CoreVersion, sizeof(ULONG))) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    try {
        ((PNTFZ_CORE_VERSION)CoreVersion)->Major = NTFZ_CORE_VERSION_MAJOR;
        ((PNTFZ_CORE_VERSION)CoreVersion)->Minor = NTFZ_CORE_VERSION_MINOR;
        ((PNTFZ_CORE_VERSION)CoreVersion)->Patch = NTFZ_CORE_VERSION_PATCH;
    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
        return GetExceptionCode();
    }

    *ReturnSize = sizeof(NTFZ_CORE_VERSION);
    return STATUS_SUCCESS;
}


NTSTATUS NTFZCoreMessageHandlerRoutine(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnSize) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnSize
) {
    UNREFERENCED_PARAMETER(ConnectionCookie);

    NTSTATUS status = STATUS_INVALID_PARAMETER;
    PNTFZ_A2CMSG pMsg;

    PAGED_CODE();

    ASSERT(ReturnSize != NULL);

    if ((Input == NULL) ||
        (InputBytes < (FIELD_OFFSET(NTFZ_A2CMSG, MsgType) + sizeof(NTFZ_A2CMSG)))) {
        KdPrint(("NtFreezerCore!%s: Bad message from admin.", __func__));
        return status;
    } else {
        pMsg = (PNTFZ_A2CMSG)Input;
        *ReturnSize = 0;
    }

    switch (pMsg->MsgType) {
    case QueryConfig:
        status = HandlerQueryConfig(pMsg->Data,
                                    pMsg->DataBytes,
                                    Output,
                                    OutputBytes,
                                    ReturnSize);
        KdPrint(("NtFreezerCore!%s: Handle message type [QueryConfig], status: [%x].", __func__, status));

        break;
    case AddConfig:
        status = HandlerAddConfig(pMsg->Data, pMsg->DataBytes);
        KdPrint(("NtFreezerCore!%s: Handle message type [AddConfig], status: [%x].", __func__, status));

        break;
    case RemoveConfig:
        status = HandlerRemoveConfig(pMsg->Data, pMsg->DataBytes);
        KdPrint(("NtFreezerCore!%s: Handle message type [RemoveConfig], status£º[%x].", __func__, status));

        break;
    case CleanupConfig:
        status = HandlerCleanupConfig();
        KdPrint(("NtFreezerCore!%s: Handle message type [CleanupConfig], status: [%x].", __func__, status));

        break;
    case GetCoreVersion:
        status = HandlerGetVersion(Output, OutputBytes, ReturnSize);
        KdPrint(("NtFreezerCore!%s: Handle message type [GetCoreVersion], status: [%x].", __func__, status));

        break;
    default:
        KdPrint(("NtFreezerCore!%s: Unknown message type.", __func__));
    }

    return status;
}

