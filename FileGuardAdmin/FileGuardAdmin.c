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
    FGL_RULE rules[] = {
        { .RuleCode = RULE_ACCESS_DENIED, .RulePathExpression = L"test_path_1"},
        { .RuleCode = RULE_ACCESS_DENIED, .RulePathExpression = L"test_path_2"},
        { .RuleCode = RULE_ACCESS_DENIED, .RulePathExpression = L"test_path_3"}
    };
    FGL_RULE rule = { 0 };
    BOOLEAN added = FALSE, removed = FALSE;
    PFG_MESSAGE message = NULL;

    // Output
    WCHAR* filename = wcsrchr(argv[0], L'\\');
    if (NULL == filename) {
        filename = argv[0];
    }

    hr = FglConnectCore(&port);
    if (FAILED(hr)) {
        fprintf(stderr, "connect core failed: 0x%08x", hr);
        goto Cleanup;
    }

    hr = FglGetCoreVersion(port, &coreVersion);
    if (FAILED(hr)) {
        fprintf(stderr, "get core version failed: 0x%08x", hr);
        goto Cleanup;
    }

    printf("%ws v%hu.%hu.%hu.%hu, core version: v%hu.%hu.%hu.%hu\n",
           ++filename,
           FGA_MAJOR_VERSION, FGA_MINOR_VERSION, FGA_PATCH_VERSION, FGA_BUILD_VERSION,
           coreVersion.Major, coreVersion.Minor, coreVersion.Patch, coreVersion.Build);

    rule.RuleCode = RULE_ACCESS_DENIED;
    rule.RulePathExpression = L"\\Device\\HarddiskVolume\\*";
    hr = FglAddSingleRule(port, &rule, &added);
    if (FAILED(hr)) {
        fprintf(stderr, "add single rule failed: 0x%08x", hr);
        goto Cleanup;
    }

    hr = FglRemoveSingleRule(port, &rule, &removed);
    if (FAILED(hr)) {
        fprintf(stderr, "remove single rule failed: 0x%08x", hr);
        goto Cleanup;
    }

Cleanup:

    if (INVALID_HANDLE_VALUE != port) {
        FglDisconnectCore(port);
    }
}