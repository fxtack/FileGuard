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

    Rule.h

Abstract:

    Rule routine declarations.

Environment:

    Kernel mode.

--*/

#pragma once

#ifndef __RULE_H__
#define __RULE_H__

typedef struct _FG_RULE_ENTRY {
    
    //
    // File path
    //
    UNICODE_STRING FilePathIndex;

    //
    // Rule.
    //
    PFG_RULE Rule;

    //
    // Lock of stream contexts list.
    //
    PKGUARDED_MUTEX StreamContextsListMutex;

    //
    // The list of stream contexts that matched with rule.
    //
    PLIST_ENTRY StreamContextsList;

} FG_RULE_ENTRY, *PFG_RULE_ENTRY;

/*-------------------------------------------------------------
    Rule entry basic routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgInitializeRuleEntry(
    _In_ PFG_RULE Rule,
    _Inout_ PFG_RULE_ENTRY RuleEntry
);

FORCEINLINE
VOID
FgDeleteRuleEntry(
    _In_ PFG_RULE_ENTRY RuleEntry
    )
{
    FLT_ASSERT(NULL != RuleEntry);

    if (NULL != RuleEntry->Rule) {
        FgFreeBuffer(RuleEntry->Rule);
    }

    if (NULL != RuleEntry->StreamContextsListMutex) {
        FgFreeBuffer(RuleEntry->StreamContextsListMutex);
    }

    if (NULL != RuleEntry->StreamContextsList) {
        FgFreeBuffer(RuleEntry->StreamContextsList);
    }
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
);

PVOID 
NTAPI 
FgRuleEntryAllocateRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ CLONG Size
);

VOID 
NTAPI 
FgRuleEntryFreeRoutine(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Entry
);

/*-------------------------------------------------------------
    Rule entry generic table operation routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAddRule(
    _Inout_ PRTL_GENERIC_TABLE RuleTable,
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PFG_RULE Rule
);

_Check_return_
NTSTATUS
FgRemoveRule(
    _Inout_ PRTL_GENERIC_TABLE RuleTable,
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PFG_RULE Rule
);

_Check_return_
NTSTATUS
FgCleanupRules(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PEX_PUSH_LOCK Lock,
    _Out_opt_ ULONG* RulesRemoved
);

_Check_return_
NTSTATUS
FgMatchRuleByFileName(
    _In_ PRTL_GENERIC_TABLE Table,
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PUNICODE_STRING FilePathIndex,
    _Out_ FG_RULE_CLASS *MatchedRuleClass
);

#endif
