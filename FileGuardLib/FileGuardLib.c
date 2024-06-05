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

    FileGuardLib.c

Abstract:

    Core-User communication library.

Environment:

    User mode.

--*/

#define BUILD_LIB

#include <windows.h>
#include <warning.h>
#include <fltuser.h>
#include <stdio.h>

#include "FileGuard.h"
#include "FileGuardLib.h"

HRESULT FglConnectCore(
    _Outptr_ HANDLE *Port
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

HRESULT FglParseMonitorRecords(
    _In_ FG_RECORDS_MESSAGE_BODY *RecordMessageBody,
    _Out_writes_(ArrayLength) PFG_MONITOR_RECORD *RecordsArray,
    _In_ USHORT ArrayLength,
    _Out_ USHORT *ParsedCount
    )
{
    ULONG totalSize = RecordMessageBody->DataSize;
    UCHAR *buffer = RecordMessageBody->DataBuffer;
    ULONG offset = 0;
    USHORT recordCount = 0;

    if (RecordMessageBody == NULL || RecordsArray == NULL || ParsedCount == NULL)
        return E_INVALIDARG;

    while (offset < totalSize) {
        if (recordCount >= ArrayLength) {
            // Buffer too small to hold all parsed pointers
            *ParsedCount = recordCount;
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        PFG_MONITOR_RECORD record = (PFG_MONITOR_RECORD)(buffer + offset);
        RecordsArray[recordCount++] = record;
        offset += sizeof(FG_MONITOR_RECORD) + record->FilePathSize;
    }

    *ParsedCount = recordCount;
    return S_OK;
}

HRESULT FglReceiveMonitorRecords(
    _In_ volatile BOOLEAN *End,
    _In_ MonitorRecordCallback MonitorRecordCallback
    )
{
    HRESULT hr = S_OK;
    HANDLE port = INVALID_HANDLE_VALUE;
    FG_MONITOR_RECORDS_MESSAGE *recordsMessage = NULL;
    USHORT parsedRecordsArrayLength = 32, parsedRecordsCount = 0;
    PFG_MONITOR_RECORD *parsedRecords = NULL, *temp = NULL;
    ULONG i = 0;

    hr = FilterConnectCommunicationPort(FG_MONITOR_PORT_NAME,
                                        0,
                                        NULL,
                                        0,
                                        NULL,
                                        &port);
    if (FAILED(hr)) return hr;

    recordsMessage = malloc(sizeof(FG_MONITOR_RECORDS_MESSAGE));
    if (NULL == recordsMessage) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

    parsedRecords = (PFG_MONITOR_RECORD*)malloc(parsedRecordsArrayLength * sizeof(PFG_MONITOR_RECORD));
    if (NULL == parsedRecords) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

    while (!(*End)) {
        hr = FilterGetMessage(port,
                              &recordsMessage->Header, 
                              sizeof(FG_MONITOR_RECORDS_MESSAGE), 
                              NULL);
        if (FAILED(hr)) goto Cleanup;

        while (TRUE) {
            hr = FglParseMonitorRecords(&recordsMessage->Body,
                                        parsedRecords,
                                        parsedRecordsArrayLength,
                                        &parsedRecordsCount);
            if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
                break;
            }

            parsedRecordsArrayLength *= 2;
            temp = (PFG_MONITOR_RECORD*)realloc(parsedRecords, parsedRecordsArrayLength * sizeof(PFG_MONITOR_RECORD));
            if (temp == NULL) {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                goto Cleanup;
            }

            parsedRecords = temp;
        }

        if (FAILED(hr)) goto Cleanup;
        
        for (i = 0; i < parsedRecordsCount; i++) MonitorRecordCallback(parsedRecords[i]);
       
    }

Cleanup:

    if (NULL != parsedRecords) free(parsedRecords);

    return hr;
}

HRESULT FglGetCoreVersion(
    _In_ CONST HANDLE Port,
    _Inout_ FG_CORE_VERSION *Version
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE msg = { .Type = GetCoreVersion };
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (NULL == Version) return E_INVALIDARG;

    hr = FilterSendMessage(Port, 
                           &msg, 
                           sizeof(FG_MESSAGE), 
                           &result,
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr)) hr = HRESULT_FROM_WIN32(result.ResultCode);
    if (SUCCEEDED(hr)) {
        Version->Major = result.CoreVersion.Major;
        Version->Minor = result.CoreVersion.Minor;
        Version->Patch = result.CoreVersion.Patch;
        Version->Build = result.CoreVersion.Build;
    }

    return hr;
}

HRESULT FglSetUnloadAcceptable(
    _In_ HANDLE Port,
    _In_ BOOLEAN Acceptable
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE msg = { .Type = SetUnloadAcceptable, .UnloadAcceptable = Acceptable };
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    hr = FilterSendMessage(Port,
                           &msg, 
                           sizeof(FG_MESSAGE), 
                           &result, 
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr)) hr = result.ResultCode;
    return hr;
}

HRESULT FglSetDetachAcceptable(
    _In_ HANDLE Port,
    _In_ BOOLEAN Acceptable
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE msg = { .Type = SetDetachAcceptable, .UnloadAcceptable = Acceptable };
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    hr = FilterSendMessage(Port,
                           &msg,
                           sizeof(FG_MESSAGE),
                           &result,
                           sizeof(FG_MESSAGE_RESULT),
                           &returned);
    if (SUCCEEDED(hr)) hr = result.ResultCode;
    return hr;
}

HRESULT FglCreateRulesMessage(
    _In_ CONST FGL_RULE Rules[],
    _In_ USHORT RulesAmount,
    _Outptr_ PFG_MESSAGE *Message
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
        if (!VALID_RULE_CODE(Rules[i].RuleCode)) {
            free(message);
            return E_INVALIDARG;
        }
        rulePtr->RuleCode = Rules[i].RuleCode;

        pathExpressionSize = (USHORT)wcslen(Rules[i].RulePathExpression) * sizeof(WCHAR);
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
    _In_ CONST FGL_RULE Rules[],
    _In_ USHORT RulesAmount,
    _Inout_opt_ USHORT *AddedRulesAmount
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE* message = NULL;
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (0 == RulesAmount || NULL == Rules) 
        return E_INVALIDARG;

    hr = FglCreateRulesMessage(Rules, RulesAmount, &message);
    if (FAILED(hr)) return hr;

    message->Type = AddRules;
    hr = FilterSendMessage(Port,
                           message,
                           message->MessageSize,
                           &result,
                           sizeof(FG_MESSAGE_RESULT),
                           &returned);
    if (SUCCEEDED(hr)) hr = HRESULT_FROM_WIN32(result.ResultCode);
    if (SUCCEEDED(hr) && NULL != AddedRulesAmount)
        *AddedRulesAmount = (USHORT)result.AffectedRulesAmount;

    if (NULL != message) FglFreeRulesMessage(message);

    return hr;
}

HRESULT FglAddSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE *Rule,
    _Inout_ BOOLEAN *Added
) {
    HRESULT hr = S_OK;
    USHORT addedAmount = 0;
    
    hr = FglAddBulkRules(Port, Rule, 1, &addedAmount);
    if (SUCCEEDED(hr) && addedAmount == 1) *Added = TRUE;
    else *Added = FALSE;

    return hr;
}

HRESULT FglRemoveBulkRules(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE Rules[],
    _In_ USHORT RulesAmount,
    _Inout_opt_ USHORT *RemovedRulesAmount
) {
    HRESULT hr = S_OK;
    FG_MESSAGE* message = NULL;
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;

    if (0 == RulesAmount || NULL == Rules)
        return E_INVALIDARG;

    hr = FglCreateRulesMessage(Rules, RulesAmount, &message);
    if (FAILED(hr)) return hr;

    message->Type = RemoveRules;
    hr = FilterSendMessage(Port, 
                           message, 
                           message->MessageSize, 
                           &result, 
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr)) hr = HRESULT_FROM_WIN32(result.ResultCode);
    if (SUCCEEDED(hr) && NULL != RemovedRulesAmount)
        *RemovedRulesAmount = (USHORT)result.AffectedRulesAmount;

    if (NULL != message) FglFreeRulesMessage(message);

    return hr;
}

HRESULT FglRemoveSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE *Rule,
    _Inout_ BOOLEAN *Removed
) {
    HRESULT hr = S_OK;
    USHORT addedAmount = 0;

    hr = FglRemoveBulkRules(Port, Rule, 1, &addedAmount);
    if (SUCCEEDED(hr) && addedAmount == 1) *Removed = TRUE;
    else *Removed = FALSE;

    return hr;
}

HRESULT FglCheckMatchedRules(
    _In_ CONST HANDLE Port,
    _In_ PCWSTR PathName,
    _Inout_opt_ FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
) {
    HRESULT hr = S_OK;
    SIZE_T pathNameSize = 0;
    FG_MESSAGE *message = NULL;
    ULONG messageSize = 0ul;
    ULONG resultSize = 0ul;
    PFG_MESSAGE_RESULT result = NULL;
    DWORD returned = 0ul;

    if (NULL == RulesSize) return E_INVALIDARG;

    pathNameSize = wcslen(PathName) * sizeof(WCHAR);

    messageSize = sizeof(FG_MESSAGE) + pathNameSize;
    message = malloc(messageSize);
    if (NULL == message) return E_OUTOFMEMORY;
    else memset(message, 0, messageSize);

    message->PathNameSize = (USHORT)pathNameSize;
    RtlCopyMemory(message->PathName, PathName, pathNameSize);

    resultSize = sizeof(FG_MESSAGE_RESULT) + RulesBufferSize;
    result = malloc(resultSize);
    if (NULL == result) return E_OUTOFMEMORY;
    else memset(result, 0, resultSize);

    message->Type = CheckMatchedRule;
    hr = FilterSendMessage(Port, 
                           message, 
                           messageSize, 
                           result, 
                           resultSize, 
                           &returned);
    if (!SUCCEEDED(hr)) return hr;

    if (NULL != RulesAmount) *RulesAmount = result->Rules.RulesAmount;
    *RulesSize = result->Rules.RulesSize;

    hr = HRESULT_FROM_WIN32(result->ResultCode);
    if (SUCCEEDED(hr) && NULL != RulesBuffer) {
        RtlCopyMemory(RulesBuffer, result->Rules.RulesBuffer, result->Rules.RulesSize);
    }
        
    return hr;
}

HRESULT FglQueryRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ FG_RULE* RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT* RulesAmount,
    _Inout_ ULONG* RulesSize
    )
{
    HRESULT hr = S_OK;
    FG_MESSAGE message = { 0 };
    ULONG resultSize = 0ul;
    PFG_MESSAGE_RESULT result = NULL;
    DWORD returned = 0ul;

    if (NULL == RulesSize) return E_INVALIDARG;

    resultSize = sizeof(FG_MESSAGE_RESULT) + RulesBufferSize;
    result = malloc(resultSize);
    if (NULL == result) return E_OUTOFMEMORY;
    else memset(result, 0, resultSize);
    
    message.Type = QueryRules;
    hr = FilterSendMessage(Port, 
                           &message, 
                           sizeof(FG_MESSAGE),
                           result,
                           resultSize,
                           &returned);
    if (!SUCCEEDED(hr)) return hr;

    if (NULL != RulesAmount) *RulesAmount = result->Rules.RulesAmount;
    *RulesSize = result->Rules.RulesSize;

    hr = HRESULT_FROM_WIN32(result->ResultCode);
    if (SUCCEEDED(hr) && NULL != RulesBuffer)
        RtlCopyMemory(RulesBuffer, result->Rules.RulesBuffer, result->Rules.RulesSize);

    return hr;
}

HRESULT FglCleanupRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ ULONG *CleanedRulesAmount
) {
    HRESULT hr = S_OK;
    FG_MESSAGE message = { 0 };
    FG_MESSAGE_RESULT result = { 0 };
    DWORD returned = 0ul;
    
    message.Type = CleanupRules;
    hr = FilterSendMessage(Port, 
                           &message, 
                           sizeof(FG_MESSAGE), 
                           &result, 
                           sizeof(FG_MESSAGE_RESULT), 
                           &returned);
    if (SUCCEEDED(hr)) hr = HRESULT_FROM_WIN32(result.ResultCode);
    if (SUCCEEDED(hr) && NULL != CleanedRulesAmount) 
        *CleanedRulesAmount = result.AffectedRulesAmount;

    return hr;
}
