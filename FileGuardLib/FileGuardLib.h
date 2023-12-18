#pragma once

#ifndef __FGLIB_H__
#define __FGLIB_H__

#include <windows.h>
#include <fltUser.h>

#include "FileGuard.h"

#define FG_LIB_MAJOR_VERSION ((USHORT)0)
#define FG_LIB_MINOR_VERSION ((USHORT)0)
#define FG_LIB_PATCH_VERSION ((USHORT)1)
#define FG_LIB_BUILD_VERSION ((USHORT)0)

typedef struct _FG_LIB_VERSION {
	USHORT Major;
	USHORT Minor;
	USHORT Patch;
	USHORT Build;
} FG_LIB_VERSION, *PFG_LIB_VERSION;

extern 
HRESULT 
FgGetLibVersion(
	_Outptr_ FG_LIB_VERSION *LibVersion
);

extern 
HRESULT 
FgConnectCore(
	_Outptr_ HANDLE* PortHandle
);

extern
HRESULT
FgDisconnectCore(
	_Inout_ HANDLE PortHandle
);

extern 
HRESULT 
FgGetCoreVersion(
	_In_ HANDLE PortHandle,
	_Outptr_ FG_CORE_VERSION *CoreVersion
);

extern 
HRESULT 
FgAddRule(
	_In_ HANDLE PortHandle,
	_Outptr_ FG_RULE *Rule
);

extern 
HRESULT 
FgRemoveRule(
	_In_ HANDLE PortHandle,
	_Outptr_ FG_RULE *Rule
);

extern 
HRESULT 
FgCleanupRules(
	_In_ HANDLE PortHandle,
	_Outptr_ WCHAR *VolumeName
);

#endif