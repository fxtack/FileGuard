#include "FileGuardLib.h"

HRESULT 
FgGetLibVersion(
	_Outptr_ FG_LIB_VERSION* LibVersion
) {

	HRESULT hr = S_OK;

	if (NULL == LibVersion) return E_INVALIDARG;

	LibVersion->Major = FG_LIB_MAJOR_VERSION;
	LibVersion->Minor = FG_LIB_MINOR_VERSION;
	LibVersion->Patch = FG_LIB_PATCH_VERSION;
	LibVersion->Build = FG_LIB_BUILD_VERSION;

	return hr;
}

HRESULT 
FgConnectCore(
	_Outptr_ HANDLE* PortHandle
) {
	HRESULT hr = S_OK;
	HANDLE portHandle = INVALID_HANDLE_VALUE;

	if (NULL == PortHandle) return E_INVALIDARG;

	*PortHandle = NULL;

	hr = FilterConnectCommunicationPort(FG_CORE_CONTROL_PORT_NAME,
									    0,
									    NULL, 
									    0, 
									    NULL, 
									    &portHandle);
	if (NULL != portHandle && INVALID_HANDLE_VALUE != portHandle)
		*PortHandle = portHandle;

	return hr;
}

HRESULT
FgDisconnectCore(
	_Inout_ HANDLE PortHandle
) {
	if (NULL == PortHandle || INVALID_HANDLE_VALUE == PortHandle)
		return E_INVALIDARG;

	CloseHandle(PortHandle);

	return S_OK;
}

HRESULT
FgGetCoreVersion(
	_In_ HANDLE PortHandle,
	_Outptr_ FG_CORE_VERSION* CoreVersion
) {
	return E_NOTIMPL;
}

HRESULT 
FgAddRule(
	_In_ HANDLE PortHandle,
	_Outptr_ FG_RULE* Rule
) {
	return E_NOTIMPL;
}

HRESULT 
FgRemoveRule(
	_In_ HANDLE PortHandle,
	_Outptr_ FG_RULE* Rule
) {
	return E_NOTIMPL;
}

HRESULT 
FgCleanupRules(
	_In_ HANDLE PortHandle,
	_Outptr_ WCHAR* VolumeName
) {
	return E_NOTIMPL;
}
