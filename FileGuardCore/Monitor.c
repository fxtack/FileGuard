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

    FileGuardCore.c

Abstract:

    Definitions of monitor relative routines.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Monitor.h"

_Check_return_
NTSTATUS
FgCreateMonitorStartContext(
    _In_ PFLT_FILTER Filter,
    _In_ PLIST_ENTRY RecordsQueue,
    _In_ PFAST_MUTEX QueueMutex,
    _In_ PFG_MONITOR_CONTEXT* Context
    )
/*++

Routine Description:

    This routine allocates monitor context.

Arguments:

    Filter       - Pointer to the filter structure.

    RecordsQueue - A pointer to monitor records queue that save records what need be 
                   send to user-mode application.

    QueueMutex   - The mutex for records queue.

    Context      - A pointer to a variable that receives the monitor context.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Filter' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'RecordsQueue' parameter is NULL.
    STATUS_INVALID_PARAMETER_3    - Failure. The 'QueueMutex' parameter is NULL.
    STATUS_INVALID_PARAMETER_4    - Failure. The 'Context' parameter is NULL.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.

--*/
{   
    NTSTATUS status = STATUS_SUCCESS;
    PFG_MONITOR_CONTEXT context = NULL;

    PAGED_CODE();

    if (NULL == Filter)       return STATUS_INVALID_PARAMETER_1;
    if (NULL == RecordsQueue) return STATUS_INVALID_PARAMETER_2;
    if (NULL == QueueMutex)   return STATUS_INVALID_PARAMETER_3;
    if (NULL == Context)      return STATUS_INVALID_PARAMETER_4;

    //
    // Allocate monitor context from paged pool.
    //
    context = (PFG_MONITOR_CONTEXT)ExAllocatePool2(POOL_FLAG_NON_PAGED,
                                                   sizeof(FG_MONITOR_CONTEXT),
                                                   FG_MONITOR_CONTEXT_PAGED_MEM_TAG);
    if (NULL == context) 
        return STATUS_INSUFFICIENT_RESOURCES;

    context->Filter            = Filter;
    context->ClientPort        = NULL;
    context->RecordsQueue      = RecordsQueue;
    context->RecordsQueueMutex = QueueMutex;
    context->MessageBody       = NULL;

    //
    // Initialize monitor thread control event.
    //
    KeInitializeEvent(&context->EventWakeMonitor, NotificationEvent, FALSE);
    KeInitializeEvent(&context->EventPortConnected, NotificationEvent, FALSE);

    //
    // Allocate monitor records buffer as message body.
    //
    status = FgAllocateBuffer(&context->MessageBody,
                              POOL_FLAG_NON_PAGED,
                              sizeof(FG_RECORDS_MESSAGE_BODY));
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    //
    // Initialize daemon flag.
    //
    InterlockedExchangeBoolean(&context->EndDaemonFlag, FALSE);

    *Context = context;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != context) {
            FgFreeMonitorStartContext(context);
        }

        if (NULL != context->MessageBody) {
            FgFreeBuffer(context->MessageBody);
        }
    }

    return status;
}

VOID
FgFreeMonitorStartContext(
    _In_ PFG_MONITOR_CONTEXT Context
)
/*++

Routine Description:

    This routine frees a monitor context.

Arguments:

    Context - Supplies the monitor context to be freed.

Return Value:

    None.

--*/
{
    PAGED_CODE();

    FLT_ASSERT(NULL != Context);

    ExFreePoolWithTag(Context, FG_MONITOR_CONTEXT_PAGED_MEM_TAG);
}

_IRQL_requires_max_(APC_LEVEL)
VOID
FgMonitorStartRoutine(
    _In_ PVOID MonitorContext
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
    NTSTATUS                 status = STATUS_SUCCESS;
    PFG_MONITOR_CONTEXT      context = NULL;
    PFG_RECORDS_MESSAGE_BODY messageBody = NULL;

    PAGED_CODE();

    FLT_ASSERT(NULL != MonitorContext);

    context = (PFG_MONITOR_CONTEXT)MonitorContext;
    messageBody = (PFG_RECORDS_MESSAGE_BODY)context->MessageBody;

    while (!context->EndDaemonFlag) {

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
        status = FgGetRecords(context->RecordsQueue, 
                              context->RecordsQueueMutex, 
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
FgGetRecords(
    _In_ PLIST_ENTRY List,
    _In_ PFAST_MUTEX ListMutex,
    _Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PUCHAR OutputBuffer,
    _In_ ULONG OutputBufferSize,
    _Out_ PULONG ReturnOutputBufferSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN recordsAvailable = FALSE;
    PLIST_ENTRY entry = NULL;
    PFG_MONITOR_RECORD_ENTRY record = NULL;
    ULONG bytesWritten = 0UL;

    ExAcquireFastMutex(ListMutex);

    while (!IsListEmpty(List) && (OutputBufferSize > 0)) {

        recordsAvailable = TRUE;

        entry = RemoveHeadList(List);

        record = CONTAINING_RECORD(entry, FG_MONITOR_RECORD_ENTRY, List);

        try {

            RtlCopyMemory(OutputBuffer, record, sizeof(FG_MONITOR_RECORD));

        } except (AsMessageException(GetExceptionInformation(), TRUE)) {

            //
            // Get a copy exception, put entry back to list.
            //
            InsertHeadList(List, entry);

            ExReleaseFastMutex(ListMutex);

            return GetExceptionCode();
        }

        bytesWritten += sizeof(FG_MONITOR_RECORD);

        OutputBufferSize -= sizeof(FG_MONITOR_RECORD);

        //
        // Move pointer to copy next record.
        //
        OutputBuffer += sizeof(FG_MONITOR_RECORD);
        
    }

    ExReleaseFastMutex(ListMutex);

    if ((0 == bytesWritten) && recordsAvailable) {

        status = STATUS_BUFFER_TOO_SMALL;
    } else if (bytesWritten > 0) {

        status = STATUS_SUCCESS;
    }

    *ReturnOutputBufferSize = bytesWritten;

    return status;
}

#pragma warning(pop)