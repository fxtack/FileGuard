/*
    @File   NtFreezerAdmin.cpp
    @Note   NtFreezerAdmin.

    @Mode   User
    @Author Fxtack
*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <iostream>
#include <algorithm>
#include <filesystem>

#include <windows.h>
#include <Fileapi.h>
#include <fltUser.h>

#include "NtFreezer.h"
#include "NtFreezerAdmin.h"

int __cdecl wmain(
    _In_ INT argc,
    _In_reads_(argc) WCHAR* argv[]
) {
    using namespace std;

    if (argc <= 1) {
        wprintf(L"Use `--help` or `-h` for help.\n");
        return 0;
    }

    wstring command(argv[1]);
    if (command == L"--help" || command == L"-h") {
        wprintf(
            L"`--version`        Check NtFreezer version.\n"
            L"`--config-add`     Add a config.\n"
            L"`--config-remove`  Remove a config.\n"
            L"`--config-cleanup` Cleanup all configs.\n"
        );
    }

    string invalidParamMsg = "Invalid parameter, enter `--help` or `-h` for usage.";

    try {
        // Initialize NtFreezerAdmin.
        ntfz::Admin admin(NTFZ_PORT_NAME);

        // Command handling.
        if (command == L"/query-config") {
            // Find a NtFreezer configuration.
            if (argc == 3) {
                auto config = admin.TellCoreQueryConfig(wstring(argv[2]));
                cout << "Result config: "
                    << "\nType: " << config->FreezeType 
                    << "\nPath: " << config->Path 
                    << endl;
                return 0;
            }
            throw invalidParamMsg;

        } else if (command == L"/add-config") {
            // Add a NtFreezer configuration.
            if (argc >= 3) {
                wstring configPath(argv[2]);
                if (argc == 3) {
                    admin.TellCoreAddConfig(configPath);                    
                } else if (argc == 5 && wstring(argv[3]) == L"/config-type") {                   
                    admin.TellCoreAddConfig(wstring(argv[4]), configPath);                    
                }
                return 0;
            }
            throw invalidParamMsg;

        } else if (command == L"/remove-config") {
            // Remove NtFreezer configuration.
            if (argc == 3) {               
                admin.TellCoreRemoveConfig(wstring(argv[2]));
                return 0;
            }
            throw invalidParamMsg;

        } else if (command == L"/cleanup-config") {
            // Clean up all configurations.
            if (argc > 2) throw invalidParamMsg;
            admin.TellCoreCleanupConfigs();

        } else if (command == L"/version") {
            // Print NtFreezerAdmin and NtFreezerCore version information.
            if (argc > 2) throw invalidParamMsg;
            admin.TellCorePrintVersion();

        } else {
            cout << "Unknown command, use `-help` or `-h` for help." << endl;
        }

    } catch (ntfz::AdminError error) {
        cout << error << endl;
    } catch (string cmdError) {
        cout << cmdError << endl;
    }
    
    return 0;
}
