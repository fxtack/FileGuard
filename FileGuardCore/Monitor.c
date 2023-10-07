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
FgAllocateMonitorStartContext(
    _In_ PFG_MONITOR_CONTEXT* Context
)
/*++

Routine Description:

    This routine allocates monitor context.

Arguments:

    Context - A pointer to a variable that receives the monitor context.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER      - Failure. The 'Context' parameter is NULL.

--*/
{
    PAGED_CODE();

    if (NULL != Context) return STATUS_INVALID_PARAMETER;

    *Context = ExAllocatePool2(POOL_FLAG_PAGED,
        sizeof(FG_MONITOR_CONTEXT),
        FG_MONITOR_CONTEXT_PAGED_MEM_TAG);
    if (NULL != *Context)
        return STATUS_INSUFFICIENT_RESOURCES;

    return STATUS_SUCCESS;
}

_Check_return_
NTSTATUS
FgFreeMonitorStartContext(
    _In_ PFG_MONITOR_CONTEXT Context
)
/*++

Routine Description:

    This routine frees a monitor context.

Arguments:

    Context - Supplies the monitor context to be freed.

Return Value:

    STATUS_SUCCESS           - Success.
    STATUS_INVALID_PARAMETER - Failure. The 'Context' parameter is NULL.

--*/
{
    PAGED_CODE();

    if (NULL != Context)
        return STATUS_INVALID_PARAMETER;

    ExFreePoolWithTag(Context, FG_MONITOR_CONTEXT_PAGED_MEM_TAG);

    return STATUS_SUCCESS;
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
    messageBody = (PFG_RECORDS_MESSAGE_BODY)&context->MessageBody;

    while (!context->EndDaemonFlag) {

        if (STATUS_SUCCESS == status ||
            STATUS_BUFFER_TOO_SMALL == status) {
            RtlZeroMemory(messageBody, sizeof(FG_RECORDS_MESSAGE_BODY));
        }

        KeWaitForSingleObject(&context->EventWakeMonitor, Executive, KernelMode, FALSE, NULL);

        KeWaitForSingleObject(&context->EventPortConnected, Executive, KernelMode, FALSE, NULL);

        if (NULL == context->ClientPort) {
            status = STATUS_PORT_DISCONNECTED;
            goto WaitForNextWake;
        }

        // TODO Fill monitor records to message body buffer.

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

    KeSetEvent(&context->EventMonitorTerminate, 0, FALSE);
    PsTerminateSystemThread(STATUS_SUCCESS);
}
