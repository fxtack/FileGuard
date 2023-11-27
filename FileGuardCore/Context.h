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

    Declarations of context structures and routines.

Environment:

    Kernel mode.

--*/

#pragma once

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

/*-------------------------------------------------------------
    Instance context structure and routines
-------------------------------------------------------------*/

typedef struct _FG_INSTANCE_CONTEXT {

    //
    // Instance context list entry.
    //
    LIST_ENTRY List;

    //
    // Instance volume.
    //
    PFLT_VOLUME Volume;

    //
    // Instance of context.
    //
    PFLT_INSTANCE Instance;

    //
    // Name of instance volume.
    //
    UNICODE_STRING VolumeName;

    //
    // Rule match mode.
    //
    ULONG RuleMatchMode;

    //
    // The generic table save rules that apply in instance.
    //
    RTL_GENERIC_TABLE RulesTable;

    //
    // The rules table lock.
    //
    PEX_PUSH_LOCK RulesTableLock;

} FG_INSTANCE_CONTEXT, *PFG_INSTANCE_CONTEXT;

_Check_return_ 
NTSTATUS 
FgCreateInstanceContext(
    _In_     PFLT_FILTER Filter,
    _In_     PFLT_VOLUME Volume,
    _In_     PFLT_INSTANCE Instance,
    _Outptr_ PFG_INSTANCE_CONTEXT *InstanceContext
);

_Check_return_
NTSTATUS
FgSetInstanceContext(
    _In_ PFLT_INSTANCE Instance,
    _In_ FLT_SET_CONTEXT_OPERATION Operation,
    _In_ PFLT_CONTEXT NewContext,
    _Outptr_opt_result_maybenull_ PFLT_CONTEXT *OldContext
);

_IRQL_requires_max_(APC_LEVEL)
_Check_return_
NTSTATUS
FgGetInstanceContextFromVolumeName(
    _In_ PUNICODE_STRING VolumeName,
    _Outptr_ PFG_INSTANCE_CONTEXT* InstanceContext
);

VOID
FgCleanupInstanceContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
);

/*-------------------------------------------------------------
    File context structure and routines.
-------------------------------------------------------------*/

typedef struct _FG_FILE_CONTEXT {

    //
    // File name inforamtion.
    //
    volatile PFLT_FILE_NAME_INFORMATION NameInfo;

    //
    // Rule class.
    //
    FG_RULE_CLASS RuleClass;

} FG_FILE_CONTEXT, *PFG_FILE_CONTEXT;

VOID
FgCleanupFileContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
);

/*-------------------------------------------------------------
    Callback context structure and routines
-------------------------------------------------------------*/

#pragma warning(push)
#pragma warning(disable: 4201)

typedef struct _FG_COMPLETION_CONTEXT {

    UCHAR MajorFunction;

    union _COMPLETION_CONTEXT_DATA {

        struct {

            FG_RULE_CLASS RuleClass;

            PFLT_FILE_NAME_INFORMATION FileNameInfo;

        } Create;

    } DUMMYUNIONNAME;

} FG_COMPLETION_CONTEXT, *PFG_COMPLETION_CONTEXT;

#pragma warning(pop)

#endif