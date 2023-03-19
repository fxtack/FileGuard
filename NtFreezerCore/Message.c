#include "NtFreezerCore.h"

NTSTATUS HandlerGetVersion(
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
);

LONG AsMessageException(
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
);

NTSTATUS NTFZCoreMessageHandlerRoutine(
    _In_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PNTFZ_A2CMSG pMsg;

    if ((Input == NULL) ||
        (InputBytes < (FIELD_OFFSET(NTFZ_A2CMSG, MsgType) + sizeof(NTFZ_A2CMSG)))) {

        KdPrint(("NtFreezer!%s", __func__));
        return STATUS_INVALID_PARAMETER;
    } else {
        pMsg = (PNTFZ_A2CMSG)Input;
    }

    switch (pMsg->MsgType) {
    case AddConfig:
        break;
    case RemoveConfig:
        break;
    case CleanupConfig:
        break;
    case GetCoreVersion:
        status = HandlerGetVersion(
            Output,
            OutputBytes,
            ReturnBytes
        );
        break;
    default:
        KdPrint(("NtFreezer!%s: Unknown message type.", __func__));
    }

    return status;
}

NTSTATUS HandlerGetVersion(
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
) {
    if (Output == NULL || OutputBytes != RESPONSE_GET_VERSION_SIZE) {
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

    *ReturnBytes = RESPONSE_GET_VERSION_SIZE;
    return STATUS_SUCCESS;
}

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