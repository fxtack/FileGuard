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

    Declarations of monitor relative structures and routines.

Environment:

    Kernel mode.

--*/

#pragma once

#ifndef __MONITOR_H__
#define __MONITOR_H__

typedef struct _FG_MONITOR_RECORD_ENTRY {

    //
    // This field is used to link to list.
    //
    LIST_ENTRY List;
    
    //
    // The monitor record.
    //
    FG_MONITOR_RECORD Record;

} FG_MONITOR_RECORD_ENTRY, *PFG_MONITOR_RECORD_ENTRY;

_Check_return_
NTSTATUS
FgAllocateMonitorRecordEntry(
    _In_ PFG_RULE Rule,
    _Outptr_ PFG_MONITOR_RECORD_ENTRY *MonitorRecordEntry
    );

VOID
FgFreeMonitorRecordEntry(
    _Inout_ PFG_MONITOR_RECORD_ENTRY MonitorRecordEntry
    );

#define FG_MONITOR_SEND_RECORD_BUFFER_SIZE (32 * 1024)

typedef struct _FG_MONITOR_CONTEXT {

    // Filter object.
    PFLT_FILTER Filter;

    // User application client port.
    PFLT_PORT   ClientPort;

    // Monitor record queue.
    PLIST_ENTRY RecordsQueue;

    // Monitor reocrd queue lock.
    KSPIN_LOCK RecordsQueueLock;

    // The event to notify monitor daemon to send records.
    KEVENT EventWakeMonitor;

    // This event will be set when user application connected to
    // monitor port. When the monitor user port closed, the event 
    // will be clear.
    KEVENT EventPortConnected;

    // Monitor records message body.
    PFG_RECORDS_MESSAGE_BODY MessageBody;

    // Monitor daemon thread ending flag.
    __volatile BOOLEAN EndDaemonFlag;

} FG_MONITOR_CONTEXT, * PFG_MONITOR_CONTEXT;

_Check_return_
NTSTATUS
FgCreateMonitorStartContext(
    _In_ PFLT_FILTER Filter,
    _In_ PLIST_ENTRY RecordsQueue,
    _In_ PFG_MONITOR_CONTEXT* Context
    );

VOID
FgFreeMonitorStartContext(
    _In_ PFG_MONITOR_CONTEXT Context
    );

_IRQL_requires_max_(APC_LEVEL)
VOID
FgMonitorStartRoutine(
    _In_ PVOID MonitorContext
    );

_Check_return_
NTSTATUS
FgGetRecords(
    _In_ PLIST_ENTRY List,
    _In_ PKSPIN_LOCK ListMutex,
    _Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PUCHAR OutputBuffer,
    _In_ ULONG OutputBufferSize,
    _Out_ PULONG ReturnOutputBufferSize
    );

#endif