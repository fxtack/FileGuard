#pragma once

#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

///////////////////////////////////////////////////////////////
// Core control communication declarations                   //
///////////////////////////////////////////////////////////////

NTSTATUS
FgCoreControlPortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
);

VOID
FgCoreControlPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
);

NTSTATUS
FgCoreControlMessageNotifyCallback(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnSize) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnSize
);

///////////////////////////////////////////////////////////////
// Monitor communication declarations                        //
///////////////////////////////////////////////////////////////

NTSTATUS
FgMonitorPortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
);

VOID
FgMonitorPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
);


#define FG_MONITOR_SEND_RECORD_BUFFER_SIZE (32 * 1024)

typedef struct _FG_MONITOR_CONTEXT{

    // Filter object.
    PFLT_FILTER Filter;

    // User application client port.
    PFLT_PORT   ClientPort;

    // Monitor record queue.
    PLIST_ENTRY RecordsQueue;

    // Monitor reocrd queue lock.
    PFAST_MUTEX RecordsQueueLock;

    // The event to notify monitor daemon to send records.
    KEVENT EventWakeMonitor;

    // This event will be set when user application connected to
    // monitor port. When the monitor user port closed, the event 
    // will be clear.
    KEVENT EventPortConnected;

    // This event will be set when monitor thread terminate.
    KEVENT EventMonitorTerminate;

    // Monitor records message body.
    PFG_RECORDS_MESSAGE_BODY MessageBody;

    // Monitor daemon thread ending flag.
    __volatile BOOLEAN EndDaemonFlag;

} FG_MONITOR_CONTEXT, *PFG_MONITOR_CONTEXT;

_Check_return_
NTSTATUS
FgAllocateMonitorStartContext(
    _In_ PFG_MONITOR_CONTEXT *Context
);

_Check_return_
NTSTATUS
FgFreeMonitorStartContext(
    _In_ PFG_MONITOR_CONTEXT Context
);

_IRQL_requires_max_(APC_LEVEL)
VOID
FgMonitorStartRoutine(
    _In_ PVOID MonitorContext
);

#endif