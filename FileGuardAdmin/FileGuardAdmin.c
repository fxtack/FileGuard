#include <stdio.h>
#include <wchar.h>
#include <locale.h>

#include <windows.h>
#include <fltUser.h>

#include "FileGuard.h"
#include "FileGuardLib.h"

#define FGA_MAJOR_VERSION ((USHORT)0)
#define FGA_MINOR_VERSION ((USHORT)1)
#define FGA_PATCH_VERSION ((USHORT)0)
#define FGA_BUILD_VERSION ((USHORT)0)

int wmain(int argc, wchar_t* argv[]) {
    HRESULT hr = S_OK;
    HANDLE port = INVALID_HANDLE_VALUE;
    FG_CORE_VERSION coreVersion = { 0 };

    // Output
    WCHAR* filename = wcsrchr(argv[0], L'\\');
    if (NULL == filename) {
        filename = argv[0];
    }

    hr = FglConnectCore(&port);
    if (!SUCCEEDED(hr)) {
        fprintf(stderr, "connect core failed: 0x%08x", hr);
        goto Cleanup;
    }

    hr = FglGetCoreVersion(port, &coreVersion);
    if (!SUCCEEDED(hr)) {
        fprintf(stderr, "get core version failed: 0x%08x", hr);
        goto Cleanup;
    }

    printf("%ws v%hu.%hu.%hu.%hu, core version: v%hu.%hu.%hu.%hu\n",
           ++filename,
           FGA_MAJOR_VERSION, FGA_MINOR_VERSION, FGA_PATCH_VERSION, FGA_BUILD_VERSION,
           coreVersion.Major, coreVersion.Minor, coreVersion.Patch, coreVersion.Build);

Cleanup:

    if (INVALID_HANDLE_VALUE != port) {
        FglDisconnectCore(port);
    }
}