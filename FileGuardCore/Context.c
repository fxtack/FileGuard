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

    Context.c

Abstract:

    Context registrations and definitions of context routines.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Context.h"

/*-------------------------------------------------------------
    File context structure and routines.
-------------------------------------------------------------*/

VOID
FgcCleanupFileContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
    )
{
    PFG_FILE_CONTEXT fileContext = (PFG_FILE_CONTEXT)Context;
    
    UNREFERENCED_PARAMETER(ContextType);

    DBG_TRACE("Cleanup file context, address: '%p'", Context);

    if (NULL != fileContext->FileNameInfo) {
        FltReleaseFileNameInformation(InterlockedExchangePointer(&fileContext->FileNameInfo, NULL));
    }
}

/*-------------------------------------------------------------
    Callback context structure and routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgcAllocateCompletionContext(
    _In_ UCHAR MajorFunction,
    _Inout_ PFG_COMPLETION_CONTEXT* CompletionContext
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_COMPLETION_CONTEXT completionContext = NULL;

    status = FgcAllocateBufferEx(&completionContext, 
                                POOL_FLAG_PAGED, 
                                sizeof(FG_COMPLETION_CONTEXT), 
                                FG_COMPLETION_CONTEXT_PAGED_TAG);
    if (NT_SUCCESS(status)) {
        completionContext->MajorFunction = MajorFunction;
        *CompletionContext = completionContext;
    }

    return status;
}
