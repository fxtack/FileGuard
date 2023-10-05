#include "FileGuardCore.h"
#include "Communication.h"

///////////////////////////////////////////////////////////////
// Core control communication functions                      //
///////////////////////////////////////////////////////////////

NTSTATUS FgCoreControlPortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
    )
/*++

Routine Description

    Communication connection callback routine.
    This is called when user-mode connects to the server port.

Arguments

    ClientPort        - This is the client connection port that will be used to send messages from the filter
                      
    ServerPortCookie  - Unused

    ConnectionContext - The connection context passed from the user. This is to recognize which type
                        connection the user is trying to connect.

    SizeofContext     - The size of the connection context.

    ConnectionCookie  - Propagation of the connection context to disconnection callback.

Return Value

    STATUS_SUCCESS                - to accept the connection
    STATUS_INSUFFICIENT_RESOURCES - if memory is not enough
    STATUS_INVALID_PARAMETER_3    - Connection context is not valid.

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(AdminPort);
    UNREFERENCED_PARAMETER(CorePortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ContextBytes);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    FLT_ASSERT(Globals.ControlClientPort == NULL);

    Globals.ControlClientPort = AdminPort;

    DbgPrint("CannotCore!%s: Communicate port connected.\n", __FUNCTION__);

    return STATUS_SUCCESS;
}


VOID FgCoreControlPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
    ) 
/*++

Routine Description

    Communication disconnection callback routine.
    This is called when user-mode disconnects the server port.

Arguments

    ConnectionCookie - The cookie set in connect notify callback. It is connection context.

Return Value

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);

    FLT_ASSERT(Globals.ControlClientPort != NULL);

    FltCloseClientPort(Globals.Filter, &Globals.ControlClientPort);

    DbgPrint("CannotCore!%s: Communicate port disconnected.\n", __FUNCTION__);
}


NTSTATUS
FgCoreControlMessageNotifyCallback(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnSize) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnSize
    ) 
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
    UNREFERENCED_PARAMETER(Input);
    UNREFERENCED_PARAMETER(InputBytes);
    UNREFERENCED_PARAMETER(Output);
    UNREFERENCED_PARAMETER(OutputBytes);
    UNREFERENCED_PARAMETER(ReturnSize);

    return STATUS_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////
// Monitor communication functions                           //
///////////////////////////////////////////////////////////////

NTSTATUS
FgMonitorPortConnectCallback(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
    )
{
    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(CorePortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ContextBytes);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    return STATUS_NOT_IMPLEMENTED;
}

VOID
FgMonitorPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
    ) 
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
}

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
    NTSTATUS                 status             = STATUS_SUCCESS;
    PFG_MONITOR_CONTEXT      context            = NULL;
    PFG_RECORDS_MESSAGE_BODY messageBody = NULL;

    PAGED_CODE();

    FLT_ASSERT(NULL != MonitorContext);

    context     = (PFG_MONITOR_CONTEXT)MonitorContext;
    messageBody = (PFG_RECORDS_MESSAGE_BODY)&context->MessageBody;

    while (!context->EndDaemonFlag) {

        if (STATUS_SUCCESS          == status ||
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
