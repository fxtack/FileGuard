#define BUILD_LIB

#include <windows.h>
#include <fltuser.h>

#include "FileGuard.h"
#include "FileGuardLib.h"

HRESULT FGAPI FglConnectCore(
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

VOID FGAPI FglDisconnectCore(
    _In_ _Post_ptr_invalid_ HANDLE Port
    )
{
    CloseHandle(Port);
}

HRESULT FGAPI FglGetCoreVersion(
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
    if (!IS_ERROR(hr) && !IS_ERROR(HRESULT_FROM_WIN32(result.ResultCode))) {
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
    SIZE_T rulesSize = 0;
    FG_MESSAGE* message = NULL;
    FG_RULE* rulePtr = NULL;
    USHORT pathExpressionSize = 0;

    if (0 == RulesAmount || NULL == Rules || NULL == Message) return E_INVALIDARG;

    for (; i < RulesAmount; i ++) {
        rulesSize += (wcslen(Rules[i].RulePathName) * sizeof(WCHAR) + sizeof(FG_RULE));
    }

    message = (FG_MESSAGE*)malloc(rulesSize + sizeof(FG_MESSAGE));
    if (NULL == message) return E_OUTOFMEMORY;

    rulePtr = (FG_RULE*)message->Rules;

    message->RulesNumber = RulesAmount;
    message->RulesSize;
    for (; i < RulesAmount; i++) {
        pathExpressionSize = (USHORT)wcslen(Rules[i].RulePathName) * sizeof(WCHAR);

        rulePtr->RuleCode = Rules[i].RuleCode;
        rulePtr->PathExpressionSize = pathExpressionSize;
        RtlCopyMemory(rulePtr->PathExpression, Rules[i].RulePathName, pathExpressionSize);

        (UCHAR*)rulePtr += (sizeof(FG_RULE) + pathExpressionSize);
    }

    *Message = message;

    return hr;
}

#define FglFreeRulesMessage(_message_) free((_message_));

HRESULT FGAPI FglAddBulkRules(
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
    if (IS_ERROR(hr)) return hr;

    message->Type = AddRules;
    hr = FilterSendMessage(Port,
                           message,
                           sizeof(FG_MESSAGE),
                           &result,
                           sizeof(FG_MESSAGE_RESULT),
                           &returned);
    if (!IS_ERROR(hr) && 
        !IS_ERROR(HRESULT_FROM_WIN32(result.ResultCode))
        && NULL != AddedRulesAmount)
        *AddedRulesAmount = result.RulesAmount;

    if (NULL != message) FglFreeRulesMessage(message);

    return hr;
}

HRESULT FGAPI FglAddSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE* Rule,
    _Inout_ BOOLEAN *Added
) {
    HRESULT hr = S_OK;
    USHORT addedAmount = 0;
    
    hr = FglAddBulkRules(Port, 1, Rule, &addedAmount);
    if (!IS_ERROR(hr) && addedAmount == 1) *Added = TRUE;
    else *Added = FALSE;

    return hr;
}

HRESULT FGAPI FglRemoveBulkRules(
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
    if (IS_ERROR(hr)) return hr;

    message->Type = RemoveRules;
    hr = FilterSendMessage(Port, 
                           message, 
                           sizeof(FG_MESSAGE), 
                           &result, 
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (!IS_ERROR(hr) &&
        !IS_ERROR(HRESULT_FROM_WIN32(result.ResultCode)) &&
        NULL != RemovedRulesAmount)
        *RemovedRulesAmount = result.RulesAmount;

    if (NULL != message) FglFreeRulesMessage(message);

    return hr;
}

HRESULT FGAPI FglRemoveSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE* Rule,
    _Inout_ BOOLEAN* Removed
) {
    HRESULT hr = S_OK;
    USHORT addedAmount = 0;

    hr = FglRemoveBulkRules(Port, 1, Rule, &addedAmount);
    if (!IS_ERROR(hr) && addedAmount == 1) *Removed = TRUE;
    else *Removed = FALSE;

    return hr;
}

HRESULT FGAPI FglQueryRules() {
    return E_NOTIMPL;
}

HRESULT FGAPI FglCleanupRules(
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
    if (!IS_ERROR(hr) && 
        !IS_ERROR(HRESULT_FROM_WIN32(result.ResultCode)) &&
        NULL != CleanedRulesAmount)
        *CleanedRulesAmount = result.RulesAmount;

    return E_NOTIMPL;
}
