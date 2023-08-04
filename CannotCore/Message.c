/*
    @File   Message.c
    @Note   CannotCore message handler.

    @Mode   Kernel
    @Author Fxtack
*/

#include "CannotCore.h"

LONG AsMessageException(
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
) {
    NTSTATUS status = ExceptionPointer->ExceptionRecord->ExceptionCode;

    if (!FsRtlIsNtstatusExpected(status) && !AccessingUserBuffer) {
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
        status = QueryConfigFromTable((PCANNOT_CONFIG)QueryConfig,
                                      (PCANNOT_CONFIG)ResultConfig);
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
        return STATUS_INVALID_PARAMETER;
    }

    try {
        status = AddConfigToTable((PCANNOT_CONFIG)AddConfig);
    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
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
        status = RemoveConfigFromTable((PCANNOT_CONFIG)RemoveConfig);
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
    if (CoreVersion == NULL || CoreVersionSize != sizeof(CANNOT_CORE_VERSION)) {
        return STATUS_INVALID_PARAMETER;
    } else if (!IS_ALIGNED(CoreVersion, sizeof(ULONG))) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    try {
        ((PCANNOT_CORE_VERSION)CoreVersion)->Major = CANNOT_CORE_VERSION_MAJOR;
        ((PCANNOT_CORE_VERSION)CoreVersion)->Minor = CANNOT_CORE_VERSION_MINOR;
        ((PCANNOT_CORE_VERSION)CoreVersion)->Patch = CANNOT_CORE_VERSION_PATCH;
    } except(AsMessageException(GetExceptionInformation(), TRUE)) {
        return GetExceptionCode();
    }

    *ReturnSize = sizeof(CANNOT_CORE_VERSION);
    return STATUS_SUCCESS;
}


NTSTATUS CannotCoreMessageHandlerRoutine(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnSize) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnSize
) {
    UNREFERENCED_PARAMETER(ConnectionCookie);

    NTSTATUS status = STATUS_INVALID_PARAMETER;
    PCANNOT_COMMAND pMsg;

    PAGED_CODE();

    ASSERT(ReturnSize != NULL);

    if ((Input == NULL) ||
        (InputBytes < (FIELD_OFFSET(CANNOT_COMMAND, MsgType) + sizeof(CANNOT_COMMAND)))) {
        KdPrint(("CannotCore!%s: Bad message from admin\n", __func__));
        return status;
    } else {
        pMsg = (PCANNOT_COMMAND)Input;
        *ReturnSize = 0;
    }

    switch (pMsg->MsgType) {
    case QueryConfig:
        status = HandlerQueryConfig(pMsg->Data,
                                    pMsg->DataBytes,
                                    Output,
                                    OutputBytes,
                                    ReturnSize);
        KdPrint(("CannotCore!%s: Handle message type 'QueryConfig', status: 0x%08x\n", __func__, status));

        break;
    case AddConfig:
        status = HandlerAddConfig(pMsg->Data, pMsg->DataBytes);
        KdPrint(("CannotCore!%s: Handle message type 'AddConfig', status: 0x%08x\n", __func__, status));

        break;
    case RemoveConfig:
        status = HandlerRemoveConfig(pMsg->Data, pMsg->DataBytes);
        KdPrint(("CannotCore!%s: Handle message type: 'RemoveConfig', status: 0x%08x\n", __func__, status));

        break;
    case CleanupConfig:
        status = HandlerCleanupConfig();
        KdPrint(("CannotCore!%s: Handle message type 'CleanupConfig', status: 0x%08x\n", __func__, status));

        break;
    case GetCoreVersion:
        status = HandlerGetVersion(Output, OutputBytes, ReturnSize);
        KdPrint(("CannotCore!%s: Handle message type 'GetCoreVersion', status: 0x%08x\n", __func__, status));

        break;
    default:
        KdPrint(("CannotCore!%s: Unknown message type: %d\n", __func__, pMsg->MsgType));
    }

    return status;
}

