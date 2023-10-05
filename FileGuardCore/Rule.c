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

    Rule.c

Abstract:

    Rule function definitions.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Rule.h"


RTL_GENERIC_COMPARE_RESULTS
NTAPI
FgRuleEntryCompareRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PVOID LEntry,
    _In_ PVOID REntry
    )
{
    UNREFERENCED_PARAMETER(Table);
    UNREFERENCED_PARAMETER(LEntry);
    UNREFERENCED_PARAMETER(REntry);

    return GenericEqual;
}


PVOID
NTAPI
FgRuleEntryAllocateRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ CLONG Size
    )
{
    UNREFERENCED_PARAMETER(Table);
    UNREFERENCED_PARAMETER(Size);

    return NULL;
}


VOID
NTAPI
FgRuleEntryFreeRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Entry
    )
{
    UNREFERENCED_PARAMETER(Table);
    UNREFERENCED_PARAMETER(Entry);
}


_Check_return_
NTSTATUS
FgCleanupRules(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PERESOURCE Resource
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID entry = NULL;

    PAGED_CODE();

    FgAcquireResourceExclusive(Resource);

    while (!RtlIsGenericTableEmpty(Table)) {
        entry = RtlGetElementGenericTable(Table, 0);
        RtlDeleteElementGenericTable(Table, entry);
    }

    FgReleaseResource(Resource);

    return status;
}