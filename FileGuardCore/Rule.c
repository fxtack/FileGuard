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

    Rule routine definitions.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Rule.h"

/*-------------------------------------------------------------
    Rule entry generic table routines
-------------------------------------------------------------*/

RTL_GENERIC_COMPARE_RESULTS
NTAPI
FgRuleEntryCompareRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PVOID LEntry,
    _In_ PVOID REntry
    )
{
    LONG compareResult = 0;
    PFG_RULE_ENTRY lRuleEntry = LEntry;
    PFG_RULE_ENTRY rRuleEntry = REntry;

    UNREFERENCED_PARAMETER(Table);

    FLT_ASSERT(NULL != lRuleEntry);
    FLT_ASSERT(NULL != rRuleEntry);

    compareResult = RtlCompareUnicodeString(&lRuleEntry->FilePathIndex,
                                            &rRuleEntry->FilePathIndex, 
                                            TRUE);
    if (compareResult < 0)  return GenericLessThan;
    else if (compareResult > 0)  return GenericGreaterThan;
    else return GenericEqual;
}

#pragma warning(push)
#pragma warning(disable: 6386)

PVOID
NTAPI
FgRuleEntryAllocateRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ CLONG Size
    )
{
    PVOID entry = NULL;

    UNREFERENCED_PARAMETER(Table);
    
    FLT_ASSERT((sizeof(FG_RULE_ENTRY) + sizeof(RTL_BALANCED_LINKS)) == Size);

    entry = ExAllocateFromNPagedLookasideList(&Globals.RuleEntryMemoryPool);
    if (NULL != entry) {
        RtlZeroMemory(entry, Size);
    }

    return entry;
}

#pragma warning(pop)

VOID
NTAPI
FgRuleEntryFreeRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Entry
    )
{
    UNREFERENCED_PARAMETER(Table);

    ExFreeToNPagedLookasideList(&Globals.RuleEntryMemoryPool, Entry);
}

/*-------------------------------------------------------------
    Rule entry generic table operation routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAddRule(
    _In_ PFG_RULE Rule
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;
    PLIST_ENTRY entry = NULL, next = NULL;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;
    UNICODE_STRING ruleFilePath = { 0 };
    PFG_RULE_ENTRY ruleEntry = NULL;
    BOOLEAN added = FALSE;

    ruleFilePath.MaximumLength = (USHORT)Rule->FilePathNameSize;
    ruleFilePath.Length = (USHORT)Rule->FilePathNameSize;
    ruleFilePath.Buffer = Rule->FilePathName;

    ExAcquireFastMutex(&Globals.InstanceContextListMutex);

    LIST_FOR_EACH_SAFE(entry, next, &Globals.InstanceContextList) {

        instanceContext = CONTAINING_RECORD(entry, FG_INSTANCE_CONTEXT, List);

        if (RtlPrefixUnicodeString(&instanceContext->VolumeName, &ruleFilePath, TRUE)) {

            ExReleaseFastMutex(&Globals.InstanceContextListMutex);

            //
            // Add rule to table that maintain in instance context.
            //
            FltAcquirePushLockExclusive(instanceContext->RulesTableLock);

            ruleEntry = RtlInsertElementGenericTable(&instanceContext->RulesTable, 
                                                     Rule, 
                                                     sizeof(FG_RULE_ENTRY), 
                                                     &added);
            if (NULL != ruleEntry && added) status = STATUS_SUCCESS;
            else status = STATUS_UNSUCCESSFUL;

            FltReleasePushLock(instanceContext->RulesTableLock);

            break;
        }
    }

    ExReleaseFastMutex(&Globals.InstanceContextListMutex);

    return status;
}

_Check_return_
NTSTATUS
FgRemoveRule(
    _In_ PFG_RULE Rule
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    if (NULL == Rule) return STATUS_INVALID_PARAMETER_1;

    return status;
}

_Check_return_
NTSTATUS
FgCleanupRules(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PEX_PUSH_LOCK Lock,
    _Out_opt_ ULONG* RulesRemoved
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID entry = NULL;

    PAGED_CODE();

    if (NULL == Table) return STATUS_INVALID_PARAMETER_1;
    if (NULL == Lock) return STATUS_INVALID_PARAMETER_2;
    if (NULL != RulesRemoved) *RulesRemoved = 0ul;

    FltAcquirePushLockExclusive(Lock);

    while (!RtlIsGenericTableEmpty(Table)) {

        entry = RtlGetElementGenericTable(Table, 0);
        if (!RtlDeleteElementGenericTable(Table, entry)) {
            status = STATUS_UNSUCCESSFUL;
            goto Cleanup;
        }

        if (NULL != RulesRemoved) *RulesRemoved++;
    }

Cleanup:

    FltReleasePushLock(Lock);

    return status;
}

_Check_return_
NTSTATUS
FgMatchRule(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PUNICODE_STRING FilePathIndex,
    _Out_ FG_RULE_CLASS *MatchedRuleClass
    ) 
{
    NTSTATUS status = STATUS_SUCCESS;
    FG_RULE_ENTRY queryEntry = { 0 };
    PFG_RULE_ENTRY resultEntry = NULL;

    if (NULL == Table) return STATUS_INVALID_PARAMETER_1;
    if (NULL == Lock) return STATUS_INVALID_PARAMETER_2;
    if (NULL == MatchedRuleClass) return  STATUS_INVALID_PARAMETER_3;

    queryEntry.FilePathIndex.Length = FilePathIndex->Length;
    queryEntry.FilePathIndex.MaximumLength = FilePathIndex->MaximumLength;
    queryEntry.FilePathIndex.Buffer = FilePathIndex->Buffer;

    FltAcquirePushLockShared(Lock);

    resultEntry = RtlLookupElementGenericTable(Table, &queryEntry);
    if (NULL != resultEntry) {
        *MatchedRuleClass = resultEntry->Rule->Class;
    } else {
        status = STATUS_NOT_FOUND;
    }

    FltReleasePushLock(Lock);

    return status;
}