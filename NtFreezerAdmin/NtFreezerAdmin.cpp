/*
    @File   NtFreezerAdmin.cpp
    @Note   NtFreezerAdmin.

    @Mode   User
    @Author Fxtack
*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <iostream>

#include <windows.h>
#include <fltUser.h>

#include "NtFreezer.h"

#define NTFZ_ADMIN_VERSION_MAJOR 0
#define NTFZ_ADMIN_VERSION_MINOR 0
#define NTFZ_ADMIN_VERSION_PATCH 0

int __cdecl wmain(
    _In_ INT argc,
    _In_reads_(argc) WCHAR* argv[]
) {

    if (argc == 1) {
        wprintf(L"Use `--help` or `-h` for help.\n");
        return 0;
    }

    std::wstring command(argv[1]);
    if (command == L"--help" || command == L"-h") {
        wprintf(
            L"`--version`\t\t Check NtFreezer version.\n"
            L"`--config-add`\t\t Add a config.\n"
            L"`--config-remove`\t Remove a config.\n"
            L"`--config-cleanup`\t Cleanup all configs.\n"
        );
    } 

    HRESULT hResult = S_OK;
    HANDLE port = INVALID_HANDLE_VALUE;
    NTFZ_A2CMSG msg;
    RESPONSE_GET_VERSION respVersion;
    DWORD returnBytes;
    
    // Connect to NtFreezerCore.
    hResult = FilterConnectCommunicationPort(
        NTFZ_PORT_NAME, 0, NULL, 0, NULL, &port);
    if (IS_ERROR(hResult)) {
        wprintf(L"ERROR: Connect to core failed, ensure that the core driver is loaded.\n");
        return -1;
    }

    // Get NtFreezerCore version.
    msg.MsgType = GetCoreVersion;
    hResult = FilterSendMessage(port,
        &msg, NTFZ_A2CMSG_SIZE,
        &respVersion, RESPONSE_GET_VERSION_SIZE, &returnBytes);
    if (IS_ERROR(hResult) || returnBytes != RESPONSE_GET_VERSION_SIZE) {
        wprintf(L"ERROR: Get core version failed, admin and core versions may not match.\n");
        return -1;
    } else {
        // Different major versions of Admin and Core are incompatible with each other.
        if (respVersion.Major != NTFZ_ADMIN_VERSION_MAJOR) {
            wprintf(
                L"ERROR: Version mismatch,"
                L"please select an admin and core version that can match\n");
            return -1;
        }

        // If the Admin minor version higher than Core minor version is incompatible.
        if (respVersion.Minor < NTFZ_ADMIN_VERSION_MINOR) {
            wprintf(
                L"ERROR: Admin version too high,"
                L"please select an admin and core that can match\n");
            return -1;
        }
    }

    if (command == L"--config-add") {
        
    } else if (command == L"--config-remove") {
    
    } else if (command == L"--config-cleanup") {
    
    } else {
        wprintf(L"Unknown command, use `-help` or `-h` for help.\n");
    }

    return 0;
}
