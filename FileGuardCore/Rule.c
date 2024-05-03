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
    Rule entry basic routines
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgCreateRuleEntry(
    _In_ PFG_RULE Rule,
    _Inout_ PFG_RULE_ENTRY* RuleEntry
    ) 
/*++

Routine Description:

    Allocate and initialize a rule entry from a rule. 

Arguments:

    Rule      - Routine will copy rule to the rule entry.

    RuleEntry - A pointer to a variable that receives the rule entry.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Buffer' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'Size' parameter is equal to zero.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING originalPathExpression = { 0 };
    PUNICODE_STRING pathExpression = NULL;
    PFG_RULE_ENTRY newRuleEntry = NULL;

    if (NULL == Rule) return STATUS_INVALID_PARAMETER_1;
    if (NULL == RuleEntry) return STATUS_INVALID_PARAMETER_2;
    
    status = FgAllocateUnicodeString(Rule->PathExpressionSize, &pathExpression);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate path expression string failed", status);
        goto Cleanup;
    }

    originalPathExpression.Buffer = Rule->PathExpression;
    originalPathExpression.Length = Rule->PathExpressionSize;
    originalPathExpression.MaximumLength = Rule->PathExpressionSize;

    status = RtlUpcaseUnicodeString(pathExpression, &originalPathExpression, FALSE);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, upcase path expression string failed", status);
        goto Cleanup;
    }

    status = FgAllocateBufferEx(&newRuleEntry, POOL_FLAG_PAGED, sizeof(FG_RULE_ENTRY), FG_RULE_ENTRY_PAGED_TAG);
    if (!NT_SUCCESS(status)) {
        LOG_ERROR("NTSTATUS: 0x%08x, allocate new rule entry failed", status);
        goto Cleanup;
    }

    FLT_ASSERT(NULL != pathExpression);
    FLT_ASSERT(NULL != newRuleEntry);

    newRuleEntry->RuleCode = Rule->RuleCode;
    newRuleEntry->PathExpression = pathExpression;
    *RuleEntry = newRuleEntry;

Cleanup:

    if (!NT_SUCCESS(status)) {

        if (NULL != newRuleEntry) {
            FgFreeBuffer(newRuleEntry);
        }

        if (NULL != pathExpression) {
            FgFreeUnicodeString(pathExpression);
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
FgAddRules(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE *Rules,
    _Inout_opt_ USHORT *AddedAmount
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT ruleIdx = 0;
    FG_RULE* rulePtr = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    PLIST_ENTRY entry = NULL, next = NULL;
    UNICODE_STRING pathExpression = { 0 };

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == ListLock) return STATUS_INVALID_PARAMETER_2;
    if (0 == RulesAmount) return STATUS_INVALID_PARAMETER_3;
    if (NULL == Rules) return STATUS_INVALID_PARAMETER_4;

    if (NULL != AddedAmount) (*AddedAmount) = 0;
    rulePtr = Rules;

    FltAcquirePushLockExclusive(ListLock);

    for (; ruleIdx < RulesAmount; ruleIdx++) {

        LIST_FOR_EACH_SAFE(entry, next, RuleList) {

            ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
            if (rulePtr->RuleCode == ruleEntry->RuleCode) {

                pathExpression.Buffer = rulePtr->PathExpression;
                pathExpression.Length = rulePtr->PathExpressionSize;
                pathExpression.MaximumLength = rulePtr->PathExpressionSize;

                if (0 == RtlCompareUnicodeString(&pathExpression, ruleEntry->PathExpression, TRUE)) 
                    goto NextNewRule;
            }
        }

        status = FgCreateRuleEntry(rulePtr, &ruleEntry);
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, create rule entry failed", status);
            break;
        }

        InsertHeadList(RuleList, &ruleEntry->List);
        if (NULL != AddedAmount) (*AddedAmount)++;

        DBG_INFO("Rule %p added, rule code: 0x%08x, path expression: '%wZ'", 
                 ruleEntry, 
                 ruleEntry->RuleCode, 
                 ruleEntry->PathExpression);

    NextNewRule:

        rulePtr = Add2Ptr(rulePtr, rulePtr->PathExpressionSize + sizeof(FG_RULE));
    }
    FltReleasePushLock(ListLock);

    return status;
}

_Check_return_
NTSTATUS
FgFindAndRemoveRule(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ USHORT RulesAmount,
    _In_ FG_RULE* Rules,
    _Inout_opt_ USHORT* RemovedAmount
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT ruleIdx = 0;
    FG_RULE* rulePtr = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    PLIST_ENTRY entry = NULL, next = NULL;
    UNICODE_STRING pathExpression = { 0 };

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == ListLock) return STATUS_INVALID_PARAMETER_2;
    if (0 == RulesAmount) return STATUS_INVALID_PARAMETER_3;
    if (NULL == Rules) return STATUS_INVALID_PARAMETER_4;

    if (NULL != RemovedAmount) (*RemovedAmount) = 0;
    rulePtr = Rules;

    FltAcquirePushLockExclusive(ListLock);
    for (; ruleIdx < RulesAmount; ruleIdx++) {
        
        LIST_FOR_EACH_SAFE(entry, next, RuleList) {

            ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
            if (rulePtr->RuleCode == ruleEntry->RuleCode) {

                pathExpression.Buffer = rulePtr->PathExpression;
                pathExpression.Length = rulePtr->PathExpressionSize;
                pathExpression.MaximumLength = rulePtr->PathExpressionSize;

                if (0 == RtlCompareUnicodeString(&pathExpression, ruleEntry->PathExpression, TRUE)) {

                    LOG_INFO("Rule %p removed, rule code: 0x%08x, path expression: '%wZ'",
                             ruleEntry,
                             ruleEntry->RuleCode,
                             ruleEntry->PathExpression);

                    RemoveEntryList(entry);
                    FgFreeRuleEntry(ruleEntry);
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
ULONG
FgMatchRules(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ PUNICODE_STRING FileDevicePathName
    )
{
    BOOLEAN matched = FALSE;
    PLIST_ENTRY entry = NULL, next = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    ULONG ruleCode = RULE_NONE;

    FLT_ASSERT(NULL != RuleList);
    FLT_ASSERT(NULL != ListLock);
    FLT_ASSERT(NULL != FileDevicePathName);

    PAGED_CODE();

    FltAcquirePushLockShared(ListLock);

    LIST_FOR_EACH_SAFE(entry, next, RuleList) {

        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
        matched = FsRtlIsNameInExpression(ruleEntry->PathExpression, 
                                          FileDevicePathName,
                                          TRUE,
                                          NULL);
        if (matched) {
            ruleCode = ruleEntry->RuleCode;
            DBG_INFO("File '%wZ' matched rule: %p, path expression: '%wZ', rule code: 0x%08x",
                     FileDevicePathName, 
                     ruleEntry, 
                     ruleEntry->PathExpression, 
                     ruleEntry->RuleCode);
            break;
        }
    }

    FltReleasePushLock(ListLock);

    return ruleCode;
}

_Check_return_
NTSTATUS
FgMatchRulesEx(
    _In_ PLIST_ENTRY RuleEntriesList,
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PUNICODE_STRING FileDevicePathName,
    _In_opt_ FG_RULE* RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT* RulesAmount,
    _Inout_ ULONG* RulesSize
) {
    NTSTATUS status = STATUS_SUCCESS;
    PLIST_ENTRY entry = NULL, next = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    BOOLEAN matched = FALSE;
    USHORT rulesAmount = 0;
    FG_RULE* rulePtr = RulesBuffer;
    ULONG bufferRemainSize = RulesBufferSize, thisRuleSize = 0ul;

    *RulesSize = 0ul;
    
    FltAcquirePushLockExclusive(Lock);

    LIST_FOR_EACH_SAFE(entry, next, RuleEntriesList) {
        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
        matched = FsRtlIsNameInExpression(ruleEntry->PathExpression,
                                          FileDevicePathName,
                                          TRUE,
                                          NULL);
        if (matched) {

            DBG_TRACE("Path '%wZ' matched rule code: %lu, path expression: '%wZ'",
                      FileDevicePathName, 
                      ruleEntry->RuleCode, 
                      ruleEntry->PathExpression);

            thisRuleSize = sizeof(FG_RULE) + ruleEntry->PathExpression->Length;
            *RulesSize += thisRuleSize;
            rulesAmount++;

            DBG_TRACE("%lu, %lu", thisRuleSize, bufferRemainSize);

            if (NULL != RulesBuffer && 0 != RulesBufferSize && bufferRemainSize >= thisRuleSize) {
                try {
                    RtlCopyMemory(rulePtr->PathExpression,
                                  ruleEntry->PathExpression->Buffer,
                                  ruleEntry->PathExpression->Length);
                    rulePtr->RuleCode = ruleEntry->RuleCode;
                    rulePtr->PathExpressionSize = ruleEntry->PathExpression->Length;
                    FLT_ASSERT(!"WATCH HERE");
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

    FLT_ASSERT(!"WATCH HERE");
    DBG_INFO("Matched rules amount: %hu, size: %lu", rulesAmount, *RulesSize);

    return status;
}

NTSTATUS
FgGetRules(
    _In_ PLIST_ENTRY RuleEntriesList,
    _In_ PEX_PUSH_LOCK Lock,
    _In_opt_  FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PLIST_ENTRY entry = NULL, next = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    USHORT rulesAmount = 0;
    FG_RULE *rulePtr = RulesBuffer;
    ULONG thisRuleSize = 0ul;

    if (NULL == RuleEntriesList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == Lock) return STATUS_INVALID_PARAMETER_2;
    if (NULL == RulesBuffer) return STATUS_INVALID_PARAMETER_3;
    if (NULL == RulesSize) return STATUS_INVALID_PARAMETER_6;

    FltAcquirePushLockExclusive(Lock);

    LIST_FOR_EACH_SAFE(entry, next, RuleEntriesList) {
        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
        *RulesSize += sizeof(FG_RULE) + ruleEntry->PathExpression->Length;
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

    LIST_FOR_EACH_SAFE(entry, next, RuleEntriesList) {
        
        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
        try {
            RtlCopyMemory(rulePtr->PathExpression,
                          ruleEntry->PathExpression->Buffer,
                          ruleEntry->PathExpression->Length);
            rulePtr->RuleCode = ruleEntry->RuleCode;
            rulePtr->PathExpressionSize = ruleEntry->PathExpression->Length;
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
FgCleanupRuleEntriesList(
    _In_ PEX_PUSH_LOCK Lock,
    _In_ PLIST_ENTRY RuleEntriesList
    )
{
    PLIST_ENTRY entry = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;
    ULONG clean = 0ul;

    FltAcquirePushLockExclusive(Lock);

    while (!IsListEmpty(RuleEntriesList)) {
        entry = RemoveHeadList(RuleEntriesList);
        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);

        DBG_TRACE("Rule: %p removed, rule code: 0x%08x, rule path expression: '%wZ'",
                  ruleEntry,
                  ruleEntry->RuleCode,
                  ruleEntry->PathExpression);

        FgFreeRuleEntry(ruleEntry);
        clean++;
    }

    FltReleasePushLock(Lock);

    DBG_INFO("Cleanup %lu rules", clean);

    return clean;
}
