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

    Monitor.c

Abstract:

    Definitions of monitor relative routines.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Monitor.h"

#pragma warning(push)
#pragma warning(disable: 6001)

#define FgcFreeMonitorRecordEntry(_entry_) InterlockedDecrement((volatile LONG*)&Globals.MonitorRecordsAllocated); \
                                           FgcFreeBuffer((_entry_))

_Check_return_
NTSTATUS
FgcRecordRuleMatched(
    _In_ UCHAR MajorFunction,
    _In_ UCHAR MinorFunction,
    _In_opt_ CONST FG_FILE_ID_DESCRIPTOR *FileIdDescriptor,
    _In_ CONST UNICODE_STRING *FilePath,
    _In_opt_ CONST UNICODE_STRING *RenameFilePath,
    _In_ CONST FG_RULE_ENTRY *RuleEntry
    )
/*++

Routine Description:

    This routine allocates a monitor record entry.

Arguments:

    MajorFunction      - IRP major function.
    MinorFunction      - IRP minior function.
    RequestorPid       - IO requestor process id.
    RequestorTid       - IO requestor thread id.
    IoStatus           - IO status.
    FileIdDescriptor   - File id descriptor.
    RenameFilePath     - Rename file path.
    FilePath           - File path.

Return Value:

    STATUS_SUCCESS - Success.
    Other          - Error status.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    SIZE_T allocateSize = 0;
    FG_MONITOR_RECORD_ENTRY *recordEntry = NULL;
    WCHAR *filePathPtr = NULL;

    if (NULL == FilePath) return STATUS_INVALID_PARAMETER_5;

    if (Globals.MonitorRecordsAllocated >= Globals.MaxMonitorRecordsAllocated) {
        return STATUS_NO_MORE_ENTRIES;
    }

    allocateSize = sizeof(FG_MONITOR_RECORD_ENTRY) + 
                   FilePath->Length + 
                   RuleEntry->PathExpression->Length;

    status = FgcAllocateBufferEx(&recordEntry, 
                                 POOL_FLAG_PAGED, 
                                 allocateSize,
                                 FG_MONITOR_RECORD_ENTRY_NON_PAGED_TAG);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate monitor record entry failed", status);
        return status;
    }

    InterlockedIncrement((volatile LONG*)&Globals.MonitorRecordsAllocated);
    
    recordEntry->Record.MajorFunction = MajorFunction;
    recordEntry->Record.MinorFunction = MinorFunction;
    recordEntry->Record.RequestorPid = (ULONG_PTR)PsGetCurrentProcessId();
    recordEntry->Record.RequestorTid = (ULONG_PTR)PsGetCurrentThreadId();
    KeQuerySystemTime(&recordEntry->Record.RecordTime);

    if (NULL != FileIdDescriptor) {
        RtlCopyMemory(&recordEntry->Record.FileIdDescriptor, 
                      FileIdDescriptor, 
                      sizeof(FG_FILE_ID_DESCRIPTOR));
    }

    recordEntry->Record.RuleCode = RuleEntry->Code;

    filePathPtr = recordEntry->Record.Buffer;
    RtlCopyMemory(filePathPtr, RuleEntry->PathExpression->Buffer, RuleEntry->PathExpression->Length);
    recordEntry->Record.RulePathExpressionSize = RuleEntry->PathExpression->Length;

    filePathPtr += RuleEntry->PathExpression->Length;
    RtlCopyMemory(filePathPtr, FilePath->Buffer, FilePath->Length);
    recordEntry->Record.FilePathSize = FilePath->Length;

    if (NULL != RenameFilePath) {
        filePathPtr += FilePath->Length;
        RtlCopyMemory(filePathPtr, RenameFilePath->Buffer, RenameFilePath->Length);
        recordEntry->Record.RenameFilePathSize = RenameFilePath->Length;
    }

    ExInterlockedInsertTailList(&Globals.MonitorRecordsQueue, recordEntry, &Globals.MonitorRecordsQueueLock);
    KeSetEvent(&Globals.MonitorContext->EventWakeMonitor, 0, FALSE);

    return status;
}

#pragma warning(pop)

_Check_return_
NTSTATUS
FgcCreateMonitorStartContext(
    _In_ PFLT_FILTER Filter,
    _In_ LIST_ENTRY *RecordsQueue,
    _In_ KSPIN_LOCK *RecordsQueueLock,
    _In_ PFG_MONITOR_CONTEXT *Context
    )
/*++

Routine Description:

    This routine allocates monitor context.

Arguments:

    Filter           - Pointer to the filter structure.
                     
    RecordsQueue     - A pointer to monitor records queue that save records what need be 
                       send to user-mode application.

    RecordsQueueLock - Lock of record queue.

    Context          - A pointer to a variable that receives the monitor context.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Filter' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'RecordsQueue' parameter is NULL.
    STATUS_INVALID_PARAMETER_3    - Failure. The 'QueueMutex' parameter is NULL.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.

--*/
{   
    NTSTATUS status = STATUS_SUCCESS;
    PFG_MONITOR_CONTEXT context = NULL;

    PAGED_CODE();

    if (NULL == Filter) return STATUS_INVALID_PARAMETER_1;
    if (NULL == RecordsQueue) return STATUS_INVALID_PARAMETER_2;
    if (NULL == Context) return STATUS_INVALID_PARAMETER_3;

    status = FgcAllocateBuffer(&context, sizeof(FG_MONITOR_CONTEXT));
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate monitor start context failed", status);
        goto Cleanup;
    }

    status = FgcAllocateBuffer(&context->MessageBody, sizeof(FG_RECORDS_MESSAGE_BODY));
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate monitor message body failed", status);
        goto Cleanup;
    }

    context->Filter = Filter;
    context->ClientPort = NULL;
    context->RecordsQueue = RecordsQueue;
    context->RecordsQueueLock = RecordsQueueLock;
    //
    // Initialize monitor thread control event.
    //
    KeInitializeEvent(&context->EventWakeMonitor, NotificationEvent, FALSE);
    KeInitializeEvent(&context->EventPortConnected, NotificationEvent, FALSE);

    //
    // Initialize daemon flag.
    //
    InterlockedExchangeBoolean(&context->EndMonitorFlag, FALSE);

    *Context = context;

Cleanup:

    if (!NT_SUCCESS(status)) {
        if (NULL != context) {
            FgcFreeMonitorStartContext(context);
        }
    }

    return status;
}

_IRQL_requires_max_(APC_LEVEL)
VOID
FgcMonitorThreadRoutine(
    _In_ PVOID MonitorStartContext
)
/*++

Routine Description:

    This routine is start routine of monitor thread.

Arguments:

    Monitor - Supplies the monitor context.

Return Value:

    None.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_MONITOR_CONTEXT context = NULL;
    PFG_RECORDS_MESSAGE_BODY messageBody = NULL;

    PAGED_CODE();

    FLT_ASSERT(NULL != MonitorStartContext);

    context = (PFG_MONITOR_CONTEXT)MonitorStartContext;
    messageBody = (PFG_RECORDS_MESSAGE_BODY)context->MessageBody;

    while (!context->EndMonitorFlag) {

        if (STATUS_SUCCESS == status || STATUS_BUFFER_TOO_SMALL == status) {
            RtlZeroMemory(messageBody, sizeof(FG_RECORDS_MESSAGE_BODY));
        }

        KeWaitForSingleObject(&context->EventWakeMonitor, Executive, KernelMode, FALSE, NULL);

        KeWaitForSingleObject(&context->EventPortConnected, Executive, KernelMode, FALSE, NULL);

        if (NULL == context->ClientPort) {
            status = STATUS_PORT_DISCONNECTED;
            goto WaitForNextWake;
        }

        //
        // Get monitor record from record queue.
        //
        status = FgcGetRecords(context->RecordsQueue, 
                               context->RecordsQueueLock, 
                               messageBody->DataBuffer, 
                               FG_MONITOR_SEND_RECORD_BUFFER_SIZE, 
                               &messageBody->DataSize);
        if (STATUS_BUFFER_TOO_SMALL == status) {
            goto WaitForNextWake;
        }

        //
        // Send records message.
        //
        status = FltSendMessage(context->Filter,
                                &context->ClientPort,
                                messageBody,
                                sizeof(FG_RECORDS_MESSAGE_BODY),
                                NULL,
                                NULL,
                                NULL);
        if (STATUS_PORT_DISCONNECTED == status) {
            // TODO Handle error.
        } else if (!NT_SUCCESS(status)) {
            // TODO
        }

    WaitForNextWake:

        if (IsListEmpty(context->RecordsQueue) && STATUS_SUCCESS == status) {
            KeClearEvent(&context->EventWakeMonitor);
        }
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

#pragma warning(push)
#pragma warning(disable: 6386)

_Check_return_
NTSTATUS
FgcGetRecords(
    _In_ PLIST_ENTRY List,
    _In_ PKSPIN_LOCK Lock,
    _Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PUCHAR OutputBuffer,
    _In_ ULONG OutputBufferSize,
    _Out_ PULONG ReturnOutputBufferSize
    )
{
    BOOLEAN recordsAvailable = FALSE;
    PLIST_ENTRY entry = NULL;
    PFG_MONITOR_RECORD_ENTRY recordEntry = NULL;
    ULONG bytesWritten = 0UL;
    KIRQL oldIrql;
    ULONG writeSize = 0UL;

    KeAcquireSpinLock(Lock, &oldIrql);
    while (!IsListEmpty(List) && (OutputBufferSize > 0)) {

        recordsAvailable = TRUE;

        entry = RemoveHeadList(List);
        recordEntry = CONTAINING_RECORD(entry, FG_MONITOR_RECORD_ENTRY, List);
        writeSize = sizeof(FG_MONITOR_RECORD) + recordEntry->Record.FilePathSize;

        if (OutputBufferSize < writeSize) {
            InsertHeadList(List, entry);
            break;
        }

        KeReleaseSpinLock(Lock, oldIrql);

        try {
            RtlCopyMemory(OutputBuffer, &recordEntry->Record, writeSize);
        } except (AsMessageException(GetExceptionInformation(), TRUE)) {
            ExInterlockedInsertHeadList(List, entry, Lock);
            return GetExceptionCode();
        }

        bytesWritten += writeSize;
        OutputBufferSize -= writeSize;
        OutputBuffer += writeSize;
        FgcFreeMonitorRecordEntry(recordEntry);
        
        KeAcquireSpinLock(Lock, &oldIrql);
    }
    KeReleaseSpinLock(Lock, oldIrql);

    *ReturnOutputBufferSize = bytesWritten;

    if ((0 == bytesWritten) && recordsAvailable) {
        return STATUS_BUFFER_TOO_SMALL;
    } else if (bytesWritten > 0) {
        return STATUS_SUCCESS;
    }

    return STATUS_UNSUCCESSFUL;
}

VOID
FgcCleanupMonitorRecords(
    VOID
    )
{
    PLIST_ENTRY pListEntry = NULL;
    FG_MONITOR_RECORD_ENTRY *monitorRecordEntry;
    INT count = 0;
    KIRQL oldIrql;

    KeAcquireSpinLock(&Globals.MonitorRecordsQueueLock, &oldIrql);

    while (!IsListEmpty(&Globals.MonitorRecordsQueue)) {

        pListEntry = RemoveHeadList(&Globals.MonitorRecordsQueue);
        KeReleaseSpinLock(&Globals.MonitorRecordsQueueLock, oldIrql);

        monitorRecordEntry = CONTAINING_RECORD(pListEntry, FG_MONITOR_RECORD_ENTRY, List);
        FgcFreeMonitorRecordEntry(monitorRecordEntry);
        count++;

        KeAcquireSpinLock(&Globals.MonitorRecordsQueueLock, &oldIrql);
    }

    KeReleaseSpinLock(&Globals.MonitorRecordsQueueLock, oldIrql);

    DBG_TRACE("Clean monitor record count: %d", count);
}

#pragma warning(pop)