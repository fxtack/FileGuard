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
    Rule entry basic routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgInitializeRuleEntry(
    _In_ PFG_RULE Rule,
    _Inout_ PFG_RULE_ENTRY RuleEntry
    ) 
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_RULE rule = NULL;
    PKGUARDED_MUTEX mutex = NULL;
    PLIST_ENTRY listEntry = NULL;

    if (NULL == Rule) return STATUS_INVALID_PARAMETER_1;
    if (NULL == RuleEntry) return STATUS_INVALID_PARAMETER_2;

    status = FgAllocateBuffer(&rule, POOL_FLAG_PAGED, sizeof(FG_RULE));
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = FgAllocateBuffer(&mutex, POOL_FLAG_PAGED, sizeof(KGUARDED_MUTEX));
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = FgAllocateBuffer(&listEntry, POOL_FLAG_PAGED, sizeof(LIST_ENTRY));
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    RtlCopyMemory(rule, Rule, sizeof(FG_RULE));

    RuleEntry->FilePathIndex.Length = rule->FilePathNameSize;
    RuleEntry->FilePathIndex.MaximumLength = rule->FilePathNameSize;
    RuleEntry->FilePathIndex.Buffer = rule->FilePathName;
    RuleEntry->Rule = rule;
    RuleEntry->StreamContextsListMutex = mutex;
    RuleEntry->StreamContextsList = listEntry;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != rule) {
            FgFreeBuffer(rule);
        }

        if (NULL != mutex) {
            FgFreeBuffer(mutex);
        }

        if (NULL != listEntry) {
            FgFreeBuffer(listEntry);
        }
    }

    return status;
}

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
    NTSTATUS status = STATUS_SUCCESS;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;
    FG_RULE_ENTRY ruleEntry = { 0 };
    PFG_RULE_ENTRY ruleEntry = NULL;
    BOOLEAN added = FALSE;

    if (NULL == Rule) return STATUS_INVALID_PARAMETER_1;

    ruleEntry.FilePathIndex.MaximumLength = (USHORT)Rule->FilePathNameSize;
    ruleEntry.FilePathIndex.Length = (USHORT)Rule->FilePathNameSize;
    ruleEntry.FilePathIndex.Buffer = Rule->FilePathName;

    status = FgGetInstanceContextByPath(&ruleEntry.FilePathIndex, &instanceContext);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    //
    // Add rule to table that maintain in instance context.
    //
    FltAcquirePushLockExclusive(instanceContext->RulesTableLock);

    ruleEntry = RtlInsertElementGenericTable(&instanceContext->RulesTable,
                                             Rule,
                                             sizeof(FG_RULE_ENTRY),
                                             &added);
    if (NULL != ruleEntry && added)
        status = STATUS_SUCCESS;
    else if (NULL != ruleEntry && !added)
        status = STATUS_DUPLICATE_OBJECTID;
    else
        status = STATUS_UNSUCCESSFUL;

    FltReleasePushLock(instanceContext->RulesTableLock);

Cleanup:

    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

    return status;
}

_Check_return_
NTSTATUS
FgRemoveRule(
    _In_ PFG_RULE Rule
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_INSTANCE_CONTEXT instanceContext = NULL;
    FG_RULE_ENTRY ruleEntry = { 0 };
    BOOLEAN removed = FALSE;

    if (NULL == Rule) return STATUS_INVALID_PARAMETER_1;

    ruleEntry.FilePathIndex.MaximumLength = (USHORT)Rule->FilePathNameSize;
    ruleEntry.FilePathIndex.Length = (USHORT)Rule->FilePathNameSize;
    ruleEntry.FilePathIndex.Buffer = Rule->FilePathName;

    status = FgGetInstanceContextByPath(&ruleEntry.FilePathIndex, &instanceContext);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    FltAcquirePushLockExclusive(instanceContext->RulesTableLock);

    removed = RtlDeleteElementGenericTable(&instanceContext->RulesTable, &ruleEntry);
    if (!removed) status = STATUS_NOT_FOUND;

    FltReleasePushLock(instanceContext->RulesTableLock);

Cleanup:

    if (NULL != instanceContext) {
        FltReleaseContext(instanceContext);
    }

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