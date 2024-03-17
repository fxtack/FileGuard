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
        goto Cleanup;
    }

    originalPathExpression.Buffer = Rule->PathExpression;
    originalPathExpression.Length = Rule->PathExpressionSize - sizeof(L'\0');
    originalPathExpression.MaximumLength = Rule->PathExpressionSize - sizeof(L'\0');

    status = RtlUpcaseUnicodeString(pathExpression, &originalPathExpression, FALSE);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = FgAllocateBufferEx(&newRuleEntry, 
                                POOL_FLAG_PAGED, 
                                sizeof(FG_RULE_ENTRY),
                                FG_RULE_ENTRY_PAGED_TAG);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    FLT_ASSERT(NULL != pathExpression);
    FLT_ASSERT(NULL != newRuleEntry);

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
FgAddRule(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ PFG_RULE Rule
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PFG_RULE_ENTRY ruleEntry = NULL;

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == ListLock) return STATUS_INVALID_PARAMETER_2;
    if (NULL == Rule) return STATUS_INVALID_PARAMETER_3;

    status = FgCreateRuleEntry(Rule, &ruleEntry);
    if (NT_SUCCESS(status)) {

        FltAcquirePushLockExclusive(ListLock);
        InsertHeadList(RuleList, &ruleEntry->List);
        FltReleasePushLock(ListLock);
    }

    return status;
}

_Check_return_
NTSTATUS
FgFindAndRemoveRule(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ PFG_RULE Rule
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PLIST_ENTRY entry = NULL, next = NULL;
    UNICODE_STRING pathExpression = { 0 };
    PFG_RULE_ENTRY ruleEntry = NULL;

    if (NULL == RuleList) return STATUS_INVALID_PARAMETER_1;
    if (NULL == ListLock) return STATUS_INVALID_PARAMETER_2;
    if (NULL == Rule) return STATUS_INVALID_PARAMETER_3;

    FltAcquirePushLockExclusive(ListLock);

    LIST_FOR_EACH_SAFE(entry, next, RuleList) {
        
        pathExpression.Buffer = Rule->PathExpression;
        pathExpression.Length = Rule->PathExpressionSize - sizeof(L'\0');
        pathExpression.MaximumLength = Rule->PathExpressionSize - sizeof(L'\0');

        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);

        if (0 == RtlCompareUnicodeString(&pathExpression, ruleEntry->PathExpression, TRUE) &&
            Rule->RulePolicy == ruleEntry->RulePolicy &&
            Rule->RuleMatch == ruleEntry->RuleMatch) {

            RemoveEntryList(entry);
            FgFreeRuleEntry(ruleEntry);
        }
    }

    FltReleasePushLock(ListLock);

    return status;
}

_Check_return_
BOOLEAN
FgMatchRule(
    _In_ PLIST_ENTRY RuleList,
    _In_ PEX_PUSH_LOCK ListLock,
    _In_ PFLT_FILE_NAME_INFORMATION FileNameInfo
    )
{
    BOOLEAN matched = FALSE;
    PLIST_ENTRY entry = NULL, next = NULL;
    PFG_RULE_ENTRY ruleEntry = NULL;

    FLT_ASSERT(NULL != RuleList);
    FLT_ASSERT(NULL != ListLock);
    FLT_ASSERT(NULL != FileNameInfo);

    PAGED_CODE();

    FltAcquirePushLockShared(ListLock);

    LIST_FOR_EACH_SAFE(entry, next, RuleList) {

        ruleEntry = CONTAINING_RECORD(entry, FG_RULE_ENTRY, List);
        matched = FsRtlIsNameInExpression(ruleEntry->PathExpression, 
                                          &FileNameInfo->Name, 
                                          FALSE, 
                                          NULL);
        if (matched) break;
    }

    FltReleasePushLock(ListLock);

    return matched;
}