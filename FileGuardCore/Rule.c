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
#include "FileGuard.h"

/*-------------------------------------------------------------
    Core rule basic structures and routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgcCreateRule(
    _In_ FG_RULE* UserRule,
    _Inout_ FGC_RULE** Rule
) {
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING originalPathExpression = { 0 };
    PUNICODE_STRING pathExpression = NULL;
    FGC_RULE* rule = NULL;

    status = FgcAllocateUnicodeString(UserRule->PathExpressionSize, &pathExpression);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate path expression string failed", status);
        goto Cleanup;
    }

    originalPathExpression.Buffer = UserRule->PathExpression;
    originalPathExpression.Length = UserRule->PathExpressionSize;
    originalPathExpression.MaximumLength = UserRule->PathExpressionSize;

    status = RtlUpcaseUnicodeString(pathExpression, &originalPathExpression, FALSE);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, upcase path expression string failed", status);
        goto Cleanup;
    }

    status = FgcAllocateBufferEx(&rule, POOL_FLAG_PAGED, sizeof(FGC_RULE_ENTRY), FG_RULE_ENTRY_PAGED_TAG);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate new rule entry failed", status);
        goto Cleanup;
    }

    FLT_ASSERT(NULL != pathExpression);
    FLT_ASSERT(NULL != rule);

    rule->Code.Value = UserRule->Code.Value;
    rule->PathExpression = pathExpression;
    InterlockedExchange64(&rule->References, 1);

    *Rule = rule;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != rule) {
            FgcFreeBuffer(rule);
        }

        if (NULL != pathExpression) {
            FgcFreeUnicodeString(pathExpression);
        }

        *Rule = NULL;
    }

    return status;
}

/*-------------------------------------------------------------
    Rule entry basic structures and routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgcCreateRuleEntry(
    _In_ FG_RULE *Rule,
    _Inout_ PFGC_RULE_ENTRY *RuleEntry
    ) 
/*++

Routine Description:

    Allocate and initialize a rule entry from a rule. 

Arguments:

    Rule      - Routine will copy rule to the rule entry.

    Entry - A pointer to a variable that receives the rule entry.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Buffer' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'Size' parameter is equal to zero.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    FGC_RULE_ENTRY *newEntry = NULL;
    FGC_RULE *rule = NULL;

    if (NULL == Rule) return STATUS_INVALID_PARAMETER_1;
    if (NULL == RuleEntry) return STATUS_INVALID_PARAMETER_2;

    status = FgcAllocateBufferEx(&newEntry, POOL_FLAG_PAGED, sizeof(FGC_RULE_ENTRY), FG_RULE_ENTRY_PAGED_TAG);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate new rule entry failed", status);
        goto Cleanup;
    }

    status = FgcCreateRule(Rule, &rule);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate new rule failed", status);
        goto Cleanup;
    }

    newEntry->Rule = rule;
    *RuleEntry = newEntry;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != newEntry) {
            FgcFreeBuffer(newEntry);
        }

        *RuleEntry = NULL;
    }

    return status;
}

/*-------------------------------------------------------------
    Rule entry generic table operation routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgcAddRules(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE *Rules,
    _Inout_opt_ USHORT *AddedAmount
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT ruleIdx = 0;
    FG_RULE *rulePtr = NULL;
    PFGC_RULE_ENTRY ruleEntry = NULL;
    PLIST_ENTRY listEntry = NULL, next = NULL;
    UNICODE_STRING pathExpression = { 0 };

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == ListLock) return STATUS_INVALID_PARAMETER_2;
    if (0 == RulesAmount) return STATUS_INVALID_PARAMETER_3;
    if (NULL == Rules) return STATUS_INVALID_PARAMETER_4;

    if (NULL != AddedAmount) (*AddedAmount) = 0;
    rulePtr = Rules;

    FltAcquirePushLockExclusive(ListLock);
    for (; ruleIdx < RulesAmount; ruleIdx++) {

        if (!VALID_RULE_CODE(rulePtr->Code)) {
            pathExpression.Buffer = rulePtr->PathExpression;
            pathExpression.Length = rulePtr->PathExpressionSize;
            pathExpression.MaximumLength = rulePtr->PathExpressionSize;
            LOG_WARNING("Invalid rule, major code: 0x%08x, minor code: 0x%08x, path expression: '%wZ'", 
                        rulePtr->Code.Major,
                        rulePtr->Code.Minor,
                        &pathExpression);

            goto NextNewRule;
        }

        LIST_FOR_EACH_SAFE(listEntry, next, RuleList) {

            ruleEntry = CONTAINING_RECORD(listEntry, FGC_RULE_ENTRY, List);
            if (rulePtr->Code.Value == ruleEntry->Rule->Code.Value) {
                pathExpression.Buffer = rulePtr->PathExpression;
                pathExpression.Length = rulePtr->PathExpressionSize;
                pathExpression.MaximumLength = rulePtr->PathExpressionSize;

                if (0 == RtlCompareUnicodeString(&pathExpression, ruleEntry->Rule->PathExpression, TRUE)) 
                    goto NextNewRule;
            }
        }

        status = FgcCreateRuleEntry(rulePtr, &ruleEntry);
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, create rule entry failed", status);
            break;
        }

        FgcReferenceRule(ruleEntry->Rule);
        InsertHeadList(RuleList, &ruleEntry->List);
        if (NULL != AddedAmount) (*AddedAmount)++;

        DBG_INFO("Rule %p added, major code: 0x%08x, minor code: 0x%08x, path expression: '%wZ'", 
                 ruleEntry, 
                 ruleEntry->Rule->Code.Major,
                 ruleEntry->Rule->Code.Minor,
                 ruleEntry->Rule->PathExpression);

    NextNewRule:

        rulePtr = Add2Ptr(rulePtr, rulePtr->PathExpressionSize + sizeof(FG_RULE));
    }
    FltReleasePushLock(ListLock);

    return status;
}

_Check_return_
NTSTATUS
FgcFindAndRemoveRule(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE *Rules,
    _Inout_opt_ USHORT *RemovedAmount
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT ruleIdx = 0;
    FG_RULE *rulePtr = NULL;
    FGC_RULE_ENTRY *ruleEntry = NULL;
    LIST_ENTRY *listEntry = NULL, *next = NULL;
    UNICODE_STRING pathExpression = { 0 };

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == ListLock) return STATUS_INVALID_PARAMETER_2;
    if (0 == RulesAmount) return STATUS_INVALID_PARAMETER_3;
    if (NULL == Rules) return STATUS_INVALID_PARAMETER_4;

    if (NULL != RemovedAmount) (*RemovedAmount) = 0;
    rulePtr = Rules;

    FltAcquirePushLockExclusive(ListLock);
    for (; ruleIdx < RulesAmount; ruleIdx++) {

        LIST_FOR_EACH_SAFE(listEntry, next, RuleList) {

            ruleEntry = CONTAINING_RECORD(listEntry, FGC_RULE_ENTRY, List);
            if (rulePtr->Code.Value == ruleEntry->Rule->Code.Value) {
                pathExpression.Buffer = rulePtr->PathExpression;
                pathExpression.Length = rulePtr->PathExpressionSize;
                pathExpression.MaximumLength = rulePtr->PathExpressionSize;
                if (0 == RtlCompareUnicodeString(&pathExpression, ruleEntry->Rule->PathExpression, TRUE)) {
                    LOG_INFO("Rule %p removed, major code: 0x%08x, minor code: 0x%08x, path expression: '%wZ'",
                             ruleEntry,
                             ruleEntry->Rule->Code.Major,
                             ruleEntry->Rule->Code.Minor,
                             ruleEntry->Rule->PathExpression);

                    RemoveEntryList(listEntry);
                    FgcReleaseRule(ruleEntry->Rule);
                    FgcFreeRuleEntry(ruleEntry);
                    if (NULL != RemovedAmount) (*RemovedAmount)++;
                }
            }
        }

        rulePtr = Add2Ptr(rulePtr, rulePtr->PathExpressionSize + sizeof(FG_RULE));
    }
    FltReleasePushLock(ListLock);

    return status;
}

_Check_return_
CONST
NTSTATUS
FgcMatchRules(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *ListLock,
    _In_ UNICODE_STRING *FileDevicePathName,
    _Outptr_result_maybenull_ FGC_RULE CONST **MatchedRule
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN matched = FALSE;
    LIST_ENTRY *entry = NULL, *next = NULL;
    FGC_RULE_ENTRY *ruleEntry = NULL;
    
    PAGED_CODE();

    FLT_ASSERT(NULL != RuleList);
    FLT_ASSERT(NULL != ListLock);
    FLT_ASSERT(NULL != FileDevicePathName);
    if (NULL == MatchedRule) return STATUS_INVALID_PARAMETER_4;

    *MatchedRule = NULL;

    FltAcquirePushLockShared(ListLock);
    LIST_FOR_EACH_SAFE(entry, next, RuleList) {

        ruleEntry = CONTAINING_RECORD(entry, FGC_RULE_ENTRY, List);
        matched = FsRtlIsNameInExpression(ruleEntry->Rule->PathExpression, 
                                          FileDevicePathName,
                                          TRUE,
                                          NULL);
        if (matched) {
            DBG_INFO("File '%wZ' matched rule: %p, major code: 0x%08x, minor code: 0x%08x, path expression: '%wZ'",
                     FileDevicePathName, 
                     ruleEntry, 
                     ruleEntry->Rule->Code.Major,
                     ruleEntry->Rule->Code.Minor,
                     ruleEntry->Rule->PathExpression);
            break;
        }
    }

    FltReleasePushLock(ListLock);

    if (matched && NT_SUCCESS(status)) {
        FgcReferenceRule(ruleEntry->Rule);
        *MatchedRule = ruleEntry->Rule;
    }

    return status;
}

_Check_return_
NTSTATUS
FgcMatchRulesEx(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *Lock,
    _In_ UNICODE_STRING *FileDevicePathName,
    _In_opt_ FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    LIST_ENTRY *entry = NULL, *next = NULL;
    FGC_RULE_ENTRY *ruleEntry = NULL;
    BOOLEAN matched = FALSE;
    USHORT rulesAmount = 0;
    FG_RULE *rulePtr = RulesBuffer;
    ULONG bufferRemainSize = RulesBufferSize, thisRuleSize = 0ul;

    *RulesSize = 0ul;
    
    FltAcquirePushLockExclusive(Lock);
    LIST_FOR_EACH_SAFE(entry, next, RuleList) {

        ruleEntry = CONTAINING_RECORD(entry, FGC_RULE_ENTRY, List);
        matched = FsRtlIsNameInExpression(ruleEntry->Rule->PathExpression,
                                          FileDevicePathName,
                                          TRUE,
                                          NULL);
        if (matched) {
            DBG_TRACE("Path '%wZ' matched rule major code: 0x%08x, minor code: 0x%08x, path expression: '%wZ'",
                      FileDevicePathName, 
                      ruleEntry->Rule->Code.Major,
                      ruleEntry->Rule->Code.Minor,
                      ruleEntry->Rule->PathExpression);

            thisRuleSize = sizeof(FG_RULE) + ruleEntry->Rule->PathExpression->Length;
            *RulesSize += thisRuleSize;
            rulesAmount++;

            DBG_TRACE("%lu, %lu", thisRuleSize, bufferRemainSize);

            if (NULL != RulesBuffer && 0 != RulesBufferSize && bufferRemainSize >= thisRuleSize) {
                try {
                    RtlCopyMemory(rulePtr->PathExpression,
                                  ruleEntry->Rule->PathExpression->Buffer,
                                  ruleEntry->Rule->PathExpression->Length);
                    rulePtr->Code.Value = ruleEntry->Rule->Code.Value;
                    rulePtr->PathExpressionSize = ruleEntry->Rule->PathExpression->Length;
                } except(EXCEPTION_EXECUTE_HANDLER) {
                    status = GetExceptionCode();
                    LOG_ERROR("NTSTATUS: 0x%08x, get rule failed", status);
                    FltReleasePushLock(Lock);
                    return status;
                }

                bufferRemainSize -= thisRuleSize;
                if ((LONG)bufferRemainSize > 0) {
                    rulePtr = Add2Ptr(rulePtr, thisRuleSize);
                }
            }
        }
    }
    FltReleasePushLock(Lock);

    if (NULL != RulesAmount) *RulesAmount = rulesAmount;
    if (0 == rulesAmount) status = STATUS_NOT_FOUND;

    if (RulesBufferSize < *RulesSize && NULL != RulesBuffer) {
        RtlZeroMemory(RulesBuffer, RulesBufferSize);
        status = STATUS_BUFFER_TOO_SMALL;
    }

    DBG_INFO("Matched rules amount: %hu, size: %lu", rulesAmount, *RulesSize);

    return status;
}

NTSTATUS
FgcGetRules(
    _In_ LIST_ENTRY *RuleList,
    _In_ EX_PUSH_LOCK *Lock,
    _In_opt_  FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    LIST_ENTRY *listEntry = NULL, *next = NULL;
    FGC_RULE_ENTRY *ruleEntry = NULL;
    USHORT rulesAmount = 0;
    FG_RULE *rulePtr = RulesBuffer;
    ULONG thisRuleSize = 0ul;

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == Lock) return STATUS_INVALID_PARAMETER_2;
    if (NULL == RulesBuffer) return STATUS_INVALID_PARAMETER_3;
    if (NULL == RulesSize) return STATUS_INVALID_PARAMETER_6;

    FltAcquirePushLockExclusive(Lock);
    LIST_FOR_EACH_SAFE(listEntry, next, RuleList) {
        ruleEntry = CONTAINING_RECORD(listEntry, FGC_RULE_ENTRY, List);
        *RulesSize += sizeof(FG_RULE) + ruleEntry->Rule->PathExpression->Length;
        rulesAmount++;
    }

    if (*RulesSize > RulesBufferSize) {
        LOG_WARNING("Get rules buffer too small, buffer size: %lu, must not be less than: %lu",
                    RulesBufferSize,
                    *RulesSize);

        status = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    if (NULL == RulesBuffer || 0 == RulesBufferSize) {
        goto Cleanup;
    }

    LIST_FOR_EACH_SAFE(listEntry, next, RuleList) {
        
        ruleEntry = CONTAINING_RECORD(listEntry, FGC_RULE_ENTRY, List);
        try {
            RtlCopyMemory(rulePtr->PathExpression,
                          ruleEntry->Rule->PathExpression->Buffer,
                          ruleEntry->Rule->PathExpression->Length);
            rulePtr->Code.Value = ruleEntry->Rule->Code.Value;
            rulePtr->PathExpressionSize = ruleEntry->Rule->PathExpression->Length;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
            LOG_ERROR("NTSTATUS: 0x%08x, get rule failed", status);
            break;
        }

        thisRuleSize = sizeof(FG_RULE) + rulePtr->PathExpressionSize;
        RulesBufferSize -= thisRuleSize;
        if ((LONG)RulesBufferSize > 0) {
            rulePtr = Add2Ptr(rulePtr, thisRuleSize);
        } else {
            break;
        }
    }

Cleanup:

    FltReleasePushLock(Lock);

    if (NULL != RulesAmount) *RulesAmount = rulesAmount;

    DBG_INFO("Query rule(s) amount: %hu, size: %lu", rulesAmount, *RulesSize);

    return status;
}

ULONG
FgcCleanupRuleEntriesList(
    _In_ EX_PUSH_LOCK *Lock,
    _In_ LIST_ENTRY *RuleList
    )
{
    LIST_ENTRY *entry = NULL;
    FGC_RULE_ENTRY *ruleEntry = NULL;
    ULONG clean = 0ul;

    FltAcquirePushLockExclusive(Lock);

    while (!IsListEmpty(RuleList)) {
        entry = RemoveHeadList(RuleList);
        ruleEntry = CONTAINING_RECORD(entry, FGC_RULE_ENTRY, List);

        DBG_TRACE("Rule: %p removed, rule major code: 0x%08x, minor code: 0x%08x, rule path expression: '%wZ'",
                  ruleEntry,
                  ruleEntry->Rule->Code.Major,
                  ruleEntry->Rule->Code.Minor,
                  ruleEntry->Rule->PathExpression);

        FgcFreeRuleEntry(ruleEntry);
        clean++;
    }

    FltReleasePushLock(Lock);

    DBG_INFO("Cleanup %lu rules", clean);

    return clean;
}
