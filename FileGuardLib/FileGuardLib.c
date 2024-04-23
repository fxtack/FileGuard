#define BUILD_LIB

#include <windows.h>
#include <fltuser.h>
#include <stdio.h>

#include "FileGuard.h"
#include "FileGuardLib.h"

HRESULT FglConnectCore(
    _Outptr_ HANDLE* Port
    )
{
    return FilterConnectCommunicationPort(FG_CORE_CONTROL_PORT_NAME,
                                          0, 
                                          NULL, 
                                          0, 
                                          NULL, 
                                          Port);
}

VOID FglDisconnectCore(
    _In_ _Post_ptr_invalid_ HANDLE Port
    )
{
    CloseHandle(Port);
}

HRESULT FglGetCoreVersion(
    _In_ CONST HANDLE Port,
    _Inout_ FG_CORE_VERSION* Version
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE msg = { .Type = GetCoreVersion };
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (NULL == Port || NULL == Version) return E_INVALIDARG;

    hr = FilterSendMessage(Port, 
                           &msg, 
                           sizeof(FG_MESSAGE), 
                           &result,
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr) && SUCCEEDED(HRESULT_FROM_WIN32(result.ResultCode))) {
        Version->Major = result.CoreVersion.Major;
        Version->Minor = result.CoreVersion.Minor;
        Version->Patch = result.CoreVersion.Patch;
        Version->Build = result.CoreVersion.Build;
    }

    return hr;
}

HRESULT FglCreateRulesMessage(
    _In_ USHORT RulesAmount,
    _In_ CONST FGL_RULE Rules[],
    _Outptr_ PFG_MESSAGE* Message
    )
{
    HRESULT hr = S_OK;
    INT i = 0;
    SIZE_T rulesSize = 0, messageSize = 0;
    FG_MESSAGE* message = NULL;
    FG_RULE* rulePtr = NULL;
    USHORT pathExpressionSize = 0;

    if (0 == RulesAmount || NULL == Rules || NULL == Message) return E_INVALIDARG;

    for (; i < RulesAmount; i ++) {
        rulesSize += (wcslen(Rules[i].RulePathExpression) * sizeof(WCHAR) + sizeof(FG_RULE));
    }

    messageSize = rulesSize + sizeof(FG_MESSAGE);
    message = (FG_MESSAGE*)malloc(messageSize);
    if (NULL == message) return E_OUTOFMEMORY;
    else RtlZeroMemory(message, rulesSize + sizeof(FG_MESSAGE));

    rulePtr = (FG_RULE*)message->Rules;

    message->MessageSize = (ULONG)messageSize;
    message->RulesAmount = RulesAmount;
    message->RulesSize = (ULONG)rulesSize;
    for (i = 0; i < RulesAmount; i++) {
        pathExpressionSize = (USHORT)wcslen(Rules[i].RulePathExpression) * sizeof(WCHAR);

        rulePtr->RuleCode = Rules[i].RuleCode;
        rulePtr->PathExpressionSize = pathExpressionSize;
        RtlCopyMemory(rulePtr->PathExpression, Rules[i].RulePathExpression, pathExpressionSize);

        (UCHAR*)rulePtr += (sizeof(FG_RULE) + pathExpressionSize);
    }

    *Message = message;

    return hr;
}

#define FglFreeRulesMessage(_message_) free((_message_));

HRESULT FglAddBulkRules(
    _In_ CONST HANDLE Port,
    _In_ USHORT RulesAmount,
    _In_ CONST FGL_RULE Rules[],
    _Inout_opt_ USHORT* AddedRulesAmount
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE* message = NULL;
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (NULL == Port || 0 == RulesAmount || NULL == Rules) 
        return E_INVALIDARG;

    hr = FglCreateRulesMessage(RulesAmount, Rules, &message);
    if (FAILED(hr)) return hr;

    message->Type = AddRules;
    hr = FilterSendMessage(Port,
                           message,
                           message->MessageSize,
                           &result,
                           sizeof(FG_MESSAGE_RESULT),
                           &returned);
    if (SUCCEEDED(hr) &&
        SUCCEEDED(HRESULT_FROM_WIN32(result.ResultCode)) &&
        NULL != AddedRulesAmount) {
        *AddedRulesAmount = result.RulesAmount;
    }

    if (NULL != message) FglFreeRulesMessage(message);

    return hr;
}

HRESULT FglAddSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE* Rule,
    _Inout_ BOOLEAN *Added
) {
    HRESULT hr = S_OK;
    USHORT addedAmount = 0;
    
    hr = FglAddBulkRules(Port, 1, Rule, &addedAmount);
    if (SUCCEEDED(hr) && addedAmount == 1) *Added = TRUE;
    else *Added = FALSE;

    return hr;
}

HRESULT FglRemoveBulkRules(
    _In_ CONST HANDLE Port,
    _In_ USHORT RulesAmount,
    _In_ CONST FGL_RULE Rules[],
    _Inout_opt_ USHORT* RemovedRulesAmount
) {
    HRESULT hr = S_OK;
    FG_MESSAGE* message = NULL;
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (NULL == Port || 0 == RulesAmount || NULL == Rules)
        return E_INVALIDARG;

    hr = FglCreateRulesMessage(RulesAmount, Rules, &message);
    if (FAILED(hr)) return hr;

    message->Type = RemoveRules;
    hr = FilterSendMessage(Port, 
                           message, 
                           message->MessageSize, 
                           &result, 
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr) &&
        SUCCEEDED(HRESULT_FROM_WIN32(result.ResultCode)) &&
        NULL != RemovedRulesAmount)
        *RemovedRulesAmount = result.RulesAmount;

    if (NULL != message) FglFreeRulesMessage(message);

    return hr;
}

HRESULT FglRemoveSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE* Rule,
    _Inout_ BOOLEAN* Removed
) {
    HRESULT hr = S_OK;
    USHORT addedAmount = 0;

    hr = FglRemoveBulkRules(Port, 1, Rule, &addedAmount);
    if (SUCCEEDED(hr) && addedAmount == 1) *Removed = TRUE;
    else *Removed = FALSE;

    return hr;
}

HRESULT FglQueryRules() {
    return E_NOTIMPL;
}

HRESULT FglCleanupRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ USHORT* CleanedRulesAmount
) {
    HRESULT hr = S_OK;
    FG_MESSAGE message = { 0 };
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (NULL == Port) return E_INVALIDARG;

    message.Type = CleanupRules;
    hr = FilterSendMessage(Port, 
                           &message, 
                           sizeof(FG_MESSAGE), 
                           &result, 
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr) && 
        SUCCEEDED(HRESULT_FROM_WIN32(result.ResultCode)) &&
        NULL != CleanedRulesAmount)
        *CleanedRulesAmount = result.RulesAmount;

    return E_NOTIMPL;
}
