/*
    @File   Main.cpp
    @Note   NTFZ Admin main.

    @Mode   User
    @Author Fxtack
*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#include <windows.h>
#include <Fileapi.h>
#include <fltUser.h>

#include "NTFZ.h"
#include "NTFZAdmin.h"

inline std::wstring formatConfigPathParam(
    _In_opt_ PCWSTR ConfigPathParam
) {
    if (ConfigPathParam == NULL || wcslen(ConfigPathParam) == 0)
        return L"";

    std::wstring configPath(ConfigPathParam);

    // The config string is invalid if its length exceeds 260.
    if (configPath.length() > MAX_PATH) {
        std::stringstream errStr;
        errStr << "Invalid config path length: " << configPath.length()
               << ", path character length must be less than " << MAX_PATH << ".";
        throw std::string(errStr.str());
    }

    std::wstring::size_type first = configPath.find_first_not_of(' ');
    if (first == std::wstring::npos)
        return L"";

    std::wstring::size_type last = configPath.find_last_not_of(' ');
    return configPath.substr(first, last - first + 1);
}

int __cdecl wmain(
    _In_ INT argc,
    _In_reads_(argc) WCHAR* argv[]
) {
    using namespace std;
    namespace fs = filesystem;

    if (argc <= 1) {
        wprintf(L"Use `--help` for help.\n");
        return 0;
    }

    wstring command(argv[1]);
    if (command == L"--help") {
        wprintf(
            L"--version        Check NTFZ version.\n"
            L"--add-config     Add a config.\n"
            L"--remove-config  Remove a config.\n"
            L"--cleanup-config Cleanup all configs.\n"
        );
        return 0;
    }

    string invalidParamError = "Invalid parameter, enter `--help` for usage.";

    try {
        // Initialize NTFZAdmin.
        ntfz::Admin admin(NTFZ_COMMAND_PORT_NAME);

        // Command handling.
        if (command == L"--query-config") {
            // Find a NTFZ configuration.
            if (argc == 3) {
                auto config = admin.TellCoreQueryConfig(formatConfigPathParam(argv[2]));
                wcout << L"Result config: "
                    << L"\nType: " << config->FreezeType 
                    << L"\nPath: " << wstring(config->Path)
                    << endl;
                return 0;
            }
            throw invalidParamError;

        } else if (command == L"--add-config") {
            // Add a NTFZ configuration.
            if (argc >= 3) {
                wstring configPath = formatConfigPathParam(argv[2]);
                if (configPath.empty()) throw invalidParamError;

                if (argc == 3) {
                    admin.TellCoreAddConfig(configPath);                    
                } else if (argc == 5 && wstring(argv[3]) == L"--config-type") {
                    admin.TellCoreAddConfig(wstring(argv[4]), configPath);                    
                }
                wcout << L"Add config successfully." << endl;
                return 0;
            }
            throw invalidParamError;

        } else if (command == L"--remove-config") {
            // Remove NTFZ configuration.
            if (argc == 3) {
                admin.TellCoreRemoveConfig(formatConfigPathParam(argv[2]));
                wcout << L"Remove config successfully." << endl;
                return 0;
            }
            throw invalidParamError;

        } else if (command == L"--cleanup-config") {
            // Clean up all configurations.
            if (argc > 2) throw invalidParamError;
            admin.TellCoreCleanupConfigs();
            wcout << L"Cleanup all configs successfully." << endl;

        } else if (command == L"--version") {
            // Print NTFZAdmin and NTFZCore version information.
            if (argc > 2) throw invalidParamError;
            admin.PrintVersion();

        } else {
            cout << "Unknown command, use `--help` for help." << endl;
        }

    } catch (ntfz::AdminError error) {
        cout << error << endl;
    } catch (string cmdError) {
        cout << cmdError << endl;
    }
    
    return 0;
}
