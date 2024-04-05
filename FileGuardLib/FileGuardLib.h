#pragma once

#ifndef _FG_LIB_H_
#define _FG_LIB_H_

#ifdef BUILD_LIB
#define FGAPI __declspec(dllexport)
#else
#define FGAPI __declspec(dllimport)
#endif

#include <windows.h>

typedef struct _FGL_RULE {
    ULONG RuleCode;
    WCHAR *RulePathName;
} FGL_RULE, *PFGL_RULE;

HRESULT FGAPI FglConnectCore(
    _Outptr_ HANDLE *Port
);

VOID FGAPI FglDisconnectCore(
    _In_ _Post_ptr_invalid_ HANDLE Port
);

HRESULT FGAPI FglGetCoreVersion(
    _In_ HANDLE Port,
    _Inout_ FG_CORE_VERSION *Version
);

HRESULT FGAPI FglAddBulkRules(
    _In_ CONST HANDLE Port,
    _In_ USHORT RulesAmount,
    _In_ CONST FGL_RULE Rules[],
    _Inout_opt_ USHORT *AddedRulesAmount
);

HRESULT FGAPI FglAddSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE *Rule,
    _Inout_ BOOLEAN *Added
);

HRESULT FGAPI FglRemoveBulkRules(
    _In_ CONST HANDLE Port,
    _In_ USHORT RulesAmount,
    _In_ CONST FGL_RULE Rules[],
    _Inout_opt_ USHORT *RemovedRulesAmount
);

HRESULT FGAPI FglRemoveSingleRule(
    _In_ CONST HANDLE Port,
    _In_ CONST FGL_RULE *Rule,
    _Inout_ BOOLEAN *Removed
);

HRESULT FGAPI FglQueryRules(

);

HRESULT FGAPI FglCleanupRules(
    _In_ CONST HANDLE Port,
    _Inout_opt_ USHORT *CleanedRulesAmount
);

#endif