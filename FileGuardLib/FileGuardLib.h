#pragma once

#ifndef _FG_LIB_H_
#define _FG_LIB_H_

#include <windows.h>

typedef struct _FGL_RULE {
    ULONG RuleCode;
    WCHAR *RulePathExpression;
} FGL_RULE, *PFGL_RULE;

extern HRESULT FglConnectCore(
    _Outptr_ HANDLE *Port
);

extern VOID FglDisconnectCore(
    _In_ _Post_ptr_invalid_ HANDLE Port
);

extern HRESULT FglGetCoreVersion(
    _In_ HANDLE Port,
    _Inout_ FG_CORE_VERSION *Version
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

extern HRESULT FglQueryRules(

);

extern HRESULT FglCleanupRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ ULONG* CleanedRulesAmount
);

#endif