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

/*-------------------------------------------------------------
    Core rule basic structures and routines
-------------------------------------------------------------*/

typedef struct _FGC_RULE {
    FG_RULE_CODE Code;
    PUNICODE_STRING PathExpression;
    volatile LONG64 References;
} FGC_RULE, * PFGC_RULE;

_Check_return_
NTSTATUS
FgcCreateRule(
    _In_ FG_RULE* UserRule,
    _Inout_ FGC_RULE** Rule
);

#define FgcReferenceRule(_rule_) InterlockedIncrement64(&(_rule_)->References)

VOID
FgcReleaseRule(
    _Inout_ FGC_RULE* Rule
);

#define FgcCompareRule(_rule1_, _rule2_) (_rule1_)->Code.Value == (_rule2_)->Code.Value && \
                                          RtlCompareUnicodeString((_rule1_)->PathExpression, (_rule2_)->PathExpression, FALSE)

/*-------------------------------------------------------------
    Rule entry basic structures and routines
-------------------------------------------------------------*/

typedef struct _FG_RULE_ENTRY {
    LIST_ENTRY List;
    FGC_RULE *Rule;
} FGC_RULE_ENTRY, *PFGC_RULE_ENTRY;

_Check_return_
NTSTATUS
FgcCreateRuleEntry(
    _In_ FG_RULE *Rule,
    _Inout_ PFGC_RULE_ENTRY *RuleEntry
    );

FORCEINLINE
VOID
FgcFreeRuleEntry(
    _In_ FGC_RULE_ENTRY *RuleEntry
) {
    if (NULL != RuleEntry->Rule) {
        FgcReleaseRule(RuleEntry->Rule);
    }

    FgcFreeBuffer(RuleEntry);
}

_Check_return_
NTSTATUS
FgcAddRules(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE* Rules,
    _Inout_opt_ USHORT* AddedAmount
    );

_Check_return_
NTSTATUS
FgcFindAndRemoveRule(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE *Rules,
    _Inout_opt_ USHORT *RemovedAmount
    );

_Check_return_
CONST
NTSTATUS
FgcMatchRules(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *ListLock,
    _In_ UNICODE_STRING *FileDevicePathName,
    _Outptr_result_maybenull_ FGC_RULE CONST **MatchedRule
    );

_Check_return_
NTSTATUS
FgcMatchRulesEx(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *Lock,
    _In_ UNICODE_STRING *FileDevicePathName,
    _In_opt_  FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
    );

NTSTATUS
FgcGetRules(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *Lock,
    _In_opt_  FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
    );

ULONG
FgcCleanupRuleEntriesList(
    _In_ EX_PUSH_LOCK *Lock,
    _In_ LIST_ENTRY *RuleList
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FgcMatchRules)
#endif

#endif
