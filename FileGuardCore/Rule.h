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

    LIST_ENTRY List;
    
    //
    // File path
    //
    PUNICODE_STRING PathExpression;

    //
    // The policy of the rule.
    //
    ULONG RuleCode;

} FG_RULE_ENTRY, *PFG_RULE_ENTRY;

/*-------------------------------------------------------------
    Rule entry basic routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAddRules(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE* Rules,
    _Inout_opt_ USHORT* AddedAmount
);

_Check_return_
NTSTATUS
FgFindAndRemoveRule(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE* Rules,
    _Inout_opt_ USHORT* RemovedAmount
);

_Check_return_
ULONG
FgMatchRule(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ PUNICODE_STRING FileDevicePathName
);

FORCEINLINE
VOID
FgFreeRuleEntry(
    _In_ PFG_RULE_ENTRY RuleEntry
) {
    FLT_ASSERT(NULL != RuleEntry);

    if (NULL != RuleEntry->PathExpression) {
        FgFreeUnicodeString(InterlockedExchangePointer(&RuleEntry->PathExpression, NULL));
    }

    if (NULL != RuleEntry) {
        FgFreeBuffer(RuleEntry);
    }
}

FORCEINLINE
ULONG
FgCleanupRuleEntriesList(
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PLIST_ENTRY RuleEntriesList
) {
    PLIST_ENTRY entry = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    ULONG clean = 0ul;

    FltAcquirePushLockExclusive(Lock);

    while (!IsListEmpty(RuleEntriesList)) {
        entry = RemoveHeadList(RuleEntriesList);
        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
        FgFreeRuleEntry(ruleEntry);
        clean++;
    }

    FltReleaseContext(Lock);

    return clean;
}

#endif
