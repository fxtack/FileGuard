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

    Basic declarations of FileGuardCore driver.

Environment:

    Kernel mode.

--*/

#ifndef __FG_CORE_H__
#define __FG_CORE_H__

//
// Configure the generic table routines to use AVL trees.
//
#define RTL_USE_AVL_TABLES 0 

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
#define FG_CORE_VERSION_MAJOR 0
#define FG_CORE_VERSION_MINOR 1
#define FG_CORE_VERSION_PATCH 1
#define FG_CORE_VERSION_BUILD 2

//
// Memory pool tags.
//
#define FG_INSTANCE_CONTEXT_PAGED_MEM_TAG 'FGic'
#define FG_FILE_CONTEXT_PAGED_MEM_TAG     'FGfc'
#define FG_UNICODE_STRING_PAGED_MEM_TAG   'FGus'
#define FG_BUFFER_PAGED_MEM_TAG           'FGbf'
#define FG_MONITOR_CONTEXT_PAGED_MEM_TAG  'FGmc'
#define FG_PUSHLOCK_NON_PAGED_MEM_TAG     'FGNr'
#define FG_RULE_ENTRY_NPAGED_MEM_TAG      'FGre'

//
// Global variables.
//
typedef struct _FG_CORE_GLOBALS {

    PFLT_FILTER Filter;            // Filter instances.

    PFLT_PORT   ControlCorePort;   // Communication port exported for CannotAdmin.
    PFLT_PORT   ControlClientPort; // Communication port that CannotAdmin connecting to.

    PFLT_PORT   MonitorClientPort; // Monitor client port.
    PFLT_PORT   MonitorCorePort;   // Monitor core port.

    PFG_MONITOR_CONTEXT MonitorContext;
    PETHREAD            MonitorThreadObject;

    ULONG LogLevel;

    LIST_ENTRY MonitorRecordsQueue;
    KSPIN_LOCK MonitorRecordsQueueLock;

    ULONG MaxRuleEntriesAllocated;             // Maximum of rule entries that can be allocated.
    __volatile ULONG RuleEntriesAllocated;     // Amount of rule entries allocated.
    NPAGED_LOOKASIDE_LIST RuleEntryMemoryPool; // Memory pool of rule entry.
    
    LIST_ENTRY InstanceContextList;            // Instance context list.
    FAST_MUTEX InstanceContextListMutex;       // Instance context list lock.

} FG_CORE_GLOBALS, *PFG_CORE_GLOBALS;

extern FG_CORE_GLOBALS Globals;

#endif