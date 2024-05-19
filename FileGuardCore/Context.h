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
    File context structure and routines.
-------------------------------------------------------------*/

typedef struct _FG_FILE_CONTEXT {

    //
    // File name inforamtion.
    //
    volatile PFLT_FILE_NAME_INFORMATION FileNameInfo;

    //
    // The policy of the rule.
    //
    volatile FG_RUEL_CODE RuleCode;

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
            PFLT_FILE_NAME_INFORMATION FileNameInfo;
            FG_RUEL_CODE RuleCode;
        } Create;

        struct {
            PFG_FILE_CONTEXT FileContext;
        } SetInformation;

    } DUMMYUNIONNAME;

} FG_COMPLETION_CONTEXT, *PFG_COMPLETION_CONTEXT;

_Check_return_
NTSTATUS
FgAllocateCompletionContext(
    _In_ UCHAR MajorFunction,
    _Inout_ PFG_COMPLETION_CONTEXT* CompletionContext
);

#define FgFreeCompletionContext(_context_) FgFreeBuffer((_context_));

#pragma warning(pop)

#endif