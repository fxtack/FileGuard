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

    FileGuardLib.h

Abstract:

    Core-User communication library.

Environment:

    User mode.

--*/

#pragma once

#ifndef _FG_LIB_H_
#define _FG_LIB_H_

#include <windows.h>

typedef struct _FGL_RULE {
    FG_RULE_CODE Code;
    PCWSTR RulePathExpression;
} FGL_RULE, *PFGL_RULE;

typedef void (CALLBACK *MonitorRecordCallback) (_In_opt_ FG_MONITOR_RECORD*);

extern HRESULT FglConnectCore(
    _Outptr_ HANDLE *Port
);

extern VOID FglDisconnectCore(
    _In_ _Post_ptr_invalid_ HANDLE Port
);

extern HRESULT FglReceiveMonitorRecords(
    _In_ volatile BOOLEAN *End,
    _In_ MonitorRecordCallback MonitorRecordCallback
);

extern HRESULT FglGetCoreVersion(
    _In_ HANDLE Port,
    _Inout_ FG_CORE_VERSION *Version
);

extern HRESULT FglSetUnloadAcceptable(
    _In_ HANDLE Port,
    _In_ BOOLEAN acceptable
);

extern HRESULT FglSetDetachAcceptable(
    _In_ HANDLE Port,
    _In_ BOOLEAN acceptable
);

extern HRESULT FglAddBulkRules(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE Rules[],
    _In_ USHORT RulesAmount,
    _Inout_opt_ USHORT *AddedRulesAmount
);

extern HRESULT FglAddSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE *Rule,
    _Inout_ BOOLEAN *Added
);

extern HRESULT FglRemoveBulkRules(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE Rules[],
    _In_ USHORT RulesAmount,
    _Inout_opt_ USHORT *RemovedRulesAmount
);

extern HRESULT FglRemoveSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE *Rule,
    _Inout_ BOOLEAN *Removed
);

extern HRESULT FglCheckMatchedRules(
    _In_ CONST HANDLE Port,
    _In_ PCWSTR PathName,
    _Inout_opt_ FG_RULE *RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT *RulesAmount,
    _Inout_ ULONG *RulesSize
);

extern HRESULT FglQueryRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ FG_RULE* RulesBuffer,
    _In_opt_ ULONG RulesBufferSize,
    _Inout_opt_ USHORT* RulesAmount,
    _Inout_ ULONG* RulesSize
);

extern HRESULT FglCleanupRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ ULONG *CleanedRulesAmount
);

#endif