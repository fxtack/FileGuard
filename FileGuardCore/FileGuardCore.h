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

    FileGuardCore.h

Abstract:

    Basic declarations of FileGuardCore driver.

Environment:

    Kernel mode.

--*/

#ifndef __FG_CORE_H__
#define __FG_CORE_H__

#include <fltKernel.h>
#include <ntddk.h>
#include <dontuse.h>

#include "FileGuard.h"
#include "Utilities.h"
#include "Rule.h"
#include "Operations.h"
#include "Context.h"
#include "Communication.h"
#include "Monitor.h"

//
// FileGuardCore version information.
//
#define FG_CORE_VERSION_MAJOR ((USHORT)0)
#define FG_CORE_VERSION_MINOR ((USHORT)2)
#define FG_CORE_VERSION_PATCH ((USHORT)0)
#define FG_CORE_VERSION_BUILD ((USHORT)0)

//
// Memory pool tags.
//
#define FG_BUFFER_NON_PAGED_TAG               'Fgnb'
#define FG_UNICODE_STRING_NON_PAGED_TAG       'FGus'
#define FG_PUSHLOCK_NON_PAGED_TAG             'FGNr'
#define FG_RULE_ENTRY_PAGED_TAG               'Fgre'
#define FG_COMPLETION_CONTEXT_PAGED_TAG       'Fgct'
#define FG_FILE_CONTEXT_PAGED_TAG             'Fgfc'
#define FG_MONITOR_RECORD_ENTRY_NON_PAGED_TAG 'Fgmr'

NTSTATUS
FgcUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
FgcInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
FgcInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
FgcInstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
FgcInstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

_Check_return_
NTSTATUS
FgcSetConfiguration(
    _In_ PUNICODE_STRING RegistryPath
    );

EXTERN_C_END

// Assign text sections for each routine.
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FgcUnload)
#pragma alloc_text(PAGE, FgcInstanceQueryTeardown)
#pragma alloc_text(PAGE, FgcInstanceSetup)
#pragma alloc_text(PAGE, FgcInstanceTeardownStart)
#pragma alloc_text(PAGE, FgcInstanceTeardownComplete)
#pragma alloc_text(PAGE, FgcSetConfiguration)
#endif

//
// Global variables.
//
typedef struct _FG_CORE_GLOBALS {

    ULONG LogLevel;

    PFLT_FILTER Filter;

    __volatile BOOLEAN AcceptUnload;
    __volatile BOOLEAN AcceptDetach;

    LIST_ENTRY RulesList;
    PEX_PUSH_LOCK RulesListLock;

    PFLT_PORT ControlCorePort;   // Communication port exported for CannotAdmin.
    PFLT_PORT ControlClientPort; // Communication port that CannotAdmin connecting to.

    PFLT_PORT MonitorClientPort; // Monitor client port.
    PFLT_PORT MonitorCorePort;   // Monitor core port.
    PFG_MONITOR_CONTEXT MonitorContext;
    PETHREAD MonitorThreadObject;

    ULONG MaxMonitorRecordsAllocated;
    __volatile ULONG MonitorRecordsAllocated;
    LIST_ENTRY MonitorRecordsQueue;
    KSPIN_LOCK MonitorRecordsQueueLock;

    ULONG MaxRuleEntriesAllocated;         // Maximum of rule entries that can be allocated.
    __volatile ULONG RuleEntriesAllocated; // Amount of rule entries allocated.

} FG_CORE_GLOBALS, *PFG_CORE_GLOBALS;

extern FG_CORE_GLOBALS Globals;

#endif