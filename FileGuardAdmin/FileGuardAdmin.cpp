#include <windows.h>
#include <fltUser.h>

#include <iostream>
#include <sstream>
#include <iomanip> 
#include <string>
#include <optional>
#include <variant>
#include <memory>
#include <map>
#include <atomic>

EXTERN_C_START
#include "FileGuard.h"
#include "FileGuardLib.h"
EXTERN_C_END

#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))

#define FGA_MAJOR_VERSION ((USHORT)0)
#define FGA_MINOR_VERSION ((USHORT)1)
#define FGA_PATCH_VERSION ((USHORT)0)
#define FGA_BUILD_VERSION ((USHORT)0)

namespace fileguard {
    class CoreClient {
    public:
        static std::variant<std::unique_ptr<CoreClient>, HRESULT> NewCoreClient() noexcept {
            HANDLE port = INVALID_HANDLE_VALUE;
            HRESULT hr = FglConnectCore(&port);
            if (FAILED(hr)) return hr;
            std::unique_ptr<CoreClient> core_client(new CoreClient(port));
            return std::move(core_client);
        }

        ~CoreClient() {
            auto port = port_.exchange(INVALID_HANDLE_VALUE);
            if (INVALID_HANDLE_VALUE != port) CloseHandle(port);
        }

        std::variant<FG_CORE_VERSION, HRESULT> GetCoreVersion() {
            FG_CORE_VERSION core_version = {};
            auto hr = FglGetCoreVersion(port_, &core_version);
            if (SUCCEEDED(hr)) return core_version;
            return hr;
        }

    private:
        std::optional<FG_CORE_VERSION> core_version_;
        std::atomic<HANDLE> port_;

        CoreClient(HANDLE port) {
            port_.exchange(port);
        }
    };

    class Admin {
    public:
        static std::variant<std::unique_ptr<Admin>, HRESULT> NewAdmin(int argc, wchar_t** argv) noexcept {
            auto core_client_opt = CoreClient::NewCoreClient();
            std::unique_ptr<CoreClient> core_client = nullptr;
            if (std::holds_alternative<std::unique_ptr<CoreClient>>(core_client_opt)) {
                core_client = std::get<std::unique_ptr<CoreClient>>(std::move(core_client_opt));
            }
            
            std::unique_ptr<Admin> admin(new Admin(argc, argv, std::move(core_client)));
            return std::move(admin);
        }

        ~Admin() = default;

        HRESULT Parse() {
            HRESULT hr = S_OK;
            std::map<std::wstring, int> args;

            if (argc_ < 2) {
                PrintUsage();
                return hr;
            }

            for (int i = 1; i < argc_; i++) {
                std::wstring flag = argv_[i];
                if (flag.find(L"--") == 0 || (flag.length() == 2 && flag[0] == L'-')) {
                    auto result = args.emplace(flag, i);
                    if (!result.second) {
                        std::wcerr << L"error: invalid parameter: repeated flags '" << flag << "'\n\n";
                        PrintUsage();
                        return E_INVALIDARG;
                    }
                }
            }

            if (args.count(L"--help") || args.count(L"-h")) {
                PrintUsage();
                return hr;
            }

            if (args.count(L"--version") || args.count(L"-v")) {
                std::wcout << GetVersionInfo() << std::endl;
                return hr;
            }

            std::wstring command = argv_[1];
            if (command == L"add") {
                std::wcout << L"add" << std::endl;
            } else if (command == L"remove") {
                std::wcout << L"remove" << std::endl;
            } else if (command == L"check-matched") {
                std::wcout << L"check-matched" << std::endl;
            } else if (command == L"clean") {
                std::wcout << L"clean" << std::endl;
            } else {
                std::wcerr << L"error: unknown command: '" << command << L"'" << std::endl;
                hr = E_INVALIDARG;
            }

            return hr;
        }

        void PrintUsage() {
            std::wcout << GetAdminImageName() <<
                L" [command] [flags]\n" << GetVersionInfo() <<
                L"\n\nThis tool is used to operate rules\n\n"
                L"commands:\n"
                L"    add\n"
                L"        --expr, -e <expression> \n"
                L"        --type, -t <access-denied|readonly|hide>\n"
                L"    remove\n"
                L"        --expr, -e <expression>\n"
                L"        --type, -t <access-denied|readonly|hide>\n"
                L"    query\n"
                L"        --format, -f <list|csv|json>\n"
                L"    check-matched\n"
                L"        --path, -p <path>\n"
                L"    clean\n"
                L"\n"
                L"flags:\n"
                L"    --help, -h\n"
                L"    --version, -v\n\n";    
        }

    private:
        int argc_;
        wchar_t** argv_;
        std::unique_ptr<CoreClient> core_client_;

        std::wstring& GetAdminImageName() {
            static std::wstring image_name;
            if (image_name.empty()) {
                std::wstring image_path = argv_[0];
                size_t pos = image_path.find_last_of('\\');
                image_name = image_path.substr(pos + 1);
            }
            return image_name;
        }

        std::wstring GetVersionInfo() {
            std::wstring core_ver_wstr;
            std::wostringstream core_ver_wos;

            if (nullptr != core_client_) {
                auto core_ver_opt = core_client_->GetCoreVersion();
                if (std::holds_alternative<FG_CORE_VERSION>(core_ver_opt)) {
                    auto core_ver = std::get<FG_CORE_VERSION>(core_ver_opt);
                    core_ver_wos << L"v"
                                 << core_ver.Major << L"."
                                 << core_ver.Minor << L"."
                                 << core_ver.Patch << L"."
                                 << core_ver.Patch;
                } else if (std::holds_alternative<HRESULT>(core_ver_opt)) {
                    core_ver_wos << L"(error: 0x" << std::setfill(L'0') << std::setw(8)
                                 << std::hex << std::get<HRESULT>(core_ver_opt)
                                 << L")";
                }
                core_ver_wstr = core_ver_wos.str();
            } else {
                core_ver_wstr = L"(core not connected)";
            }

            std::wostringstream admin_ver_wos;
            admin_ver_wos << L"Admin: v"
                          << FGA_MAJOR_VERSION << L"."
                          << FGA_MINOR_VERSION << L"."
                          << FGA_PATCH_VERSION << L"."
                          << FGA_BUILD_VERSION << L"\n"
                          << L"Core:  " << core_ver_wstr;

            return admin_ver_wos.str();
        }

        Admin(int argc, wchar_t** argv, std::unique_ptr<CoreClient> core_client) {
            argc_ = argc;
            argv_ = argv;
            core_client_ = std::move(core_client);
        };
    };
}

int wmain(int argc, wchar_t* argv[]) {
    
    auto admin_opt = fileguard::Admin::NewAdmin(argc, argv);
    if (std::holds_alternative<HRESULT>(admin_opt)) {
        auto hr = std::get<HRESULT>(admin_opt);
        std::cerr << "0x" << std::hex << std::setfill('0') << std::setw(8) << hr << std::endl;
        return hr;
    }

    return std::get<std::unique_ptr<fileguard::Admin>>(std::move(admin_opt))->Parse();

    //HRESULT hr = S_OK;
    //HANDLE port = INVALID_HANDLE_VALUE;
    //FG_CORE_VERSION coreVersion = { 0 };
    //FGL_RULE rules[] = {
    //    { RULE_READONLY, L"*\\baned.txt" },
    //    { RULE_READONLY, L"*baned.txt" },
    //    { RULE_READONLY, L"*\\baned.txt" }
    //};
    //FGL_RULE rule = { 0 };
    //BOOLEAN added = FALSE, removed = FALSE;
    //USHORT addedAmount = 0, removedAmount = 0;
    //ULONG cleanRules = 0;
    //PFG_MESSAGE message = NULL;

    //UCHAR *buffer = NULL;
    //USHORT queryAmount = 0;
    //ULONG querySize = 0ul;
    //FG_RULE *rulePtr = NULL;
    //ULONG thisRuleSize = 0ul;
    //INT i = 0;

    //// Output
    //WCHAR* filename = wcsrchr(argv[0], L'\\');
    //if (NULL == filename) {
    //    filename = argv[0];
    //}

    //hr = FglConnectCore(&port);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "connect core failed: 0x%08x", hr);
    //    goto Cleanup;
    //}

    //hr = FglGetCoreVersion(port, &coreVersion);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "get core version failed: 0x%08x", hr);
    //    goto Cleanup;
    //}

    //printf("%ws v%hu.%hu.%hu.%hu, core version: v%hu.%hu.%hu.%hu\n",
    //       ++filename,
    //       FGA_MAJOR_VERSION, FGA_MINOR_VERSION, FGA_PATCH_VERSION, FGA_BUILD_VERSION,
    //       coreVersion.Major, coreVersion.Minor, coreVersion.Patch, coreVersion.Build);

    //rule.RuleCode = RULE_READONLY;
    //rule.RulePathExpression = L"\\Device\\HarddiskVolume\\*";
    //hr = FglAddSingleRule(port, &rule, &added);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "add single rule failed: 0x%08x", hr);
    //    goto Cleanup;
    //}

    ////hr = FglRemoveSingleRule(port, &rule, &removed);
    ////if (FAILED(hr)) {
    ////    fprintf(stderr, "remove single rule failed: 0x%08x", hr);
    ////    goto Cleanup;
    ////}

    //hr = FglAddBulkRules(port, rules, 3, &addedAmount);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "add rules failed: 0x%08x", hr);
    //    goto Cleanup;
    //}

    //hr = FglQueryRules(port, NULL, 0, &queryAmount, &querySize);
    //printf("0x%08x, query result amount: %hu, size: %lu\n", hr, queryAmount, querySize);

    //buffer = malloc(querySize);
    //if (NULL == buffer) goto Cleanup;
    //hr = FglQueryRules(port, (FG_RULE*)buffer, querySize, &queryAmount, &querySize);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "Query rules failed: 0x%08x", hr);
    //    goto Cleanup;
    //}

    //rulePtr = buffer;
    //while (querySize > 0) {
    //    printf("remain size: %lu, rule code: %lu, path expression: %.*ls, size: %lu\n", 
    //           querySize,
    //           rulePtr->RuleCode, 
    //           rulePtr->PathExpressionSize/sizeof(WCHAR),
    //           rulePtr->PathExpression,
    //           rulePtr->PathExpressionSize);
    //    rulePtr = ((UCHAR*)rulePtr) + (sizeof(FG_RULE) + rulePtr->PathExpressionSize);
    //    querySize -= ((sizeof(FG_RULE) + rulePtr->PathExpressionSize));
    //}
    

    //hr = FglRemoveBulkRules(port, rules, 2, &removedAmount);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "remove rules failed: 0x%08x", hr);
    //    goto Cleanup;
    //}

    //hr = FglCleanupRules(port, &cleanRules);
    //if (FAILED(hr)) {
    //    fprintf(stderr, "clean rules failed: 0x%08x", hr);
    //    goto Cleanup;
    //} else {
    //    printf("Cleanup %lu rules\n", cleanRules);
    //}
    
//    hr = FglCheckMatchedRules(port,
//                              L"\\Device\\HarddiskVolume6\\baned.txt",
//                              NULL, 
//                              0,
//                              &queryAmount,
//                              &querySize);
//    printf("hresult: 0x%08x, matched amount: %hu, size: %lu\n", hr, queryAmount, querySize);
//
//    buffer = (UCHAR*)malloc(querySize);
//    if (NULL == buffer) goto Cleanup;
//    else RtlZeroMemory(buffer, querySize);
//    hr = FglCheckMatchedRules(port,
//                             L"\\Device\\HarddiskVolume6\\baned.txt",
//                             (FG_RULE*)buffer, 
//                             querySize,
//                             &queryAmount, 
//                             &querySize);
//    printf("hresult: 0x%08x, matched amount: %hu, size: %lu\n", hr, queryAmount, querySize);
//    rulePtr = (FG_RULE*)buffer;
//    while (querySize > 0) {
//        printf("remain size: %lu, rule code: %lu, path expression: %.*ls, size: %lu\n", 
//               querySize,
//               rulePtr->RuleCode, 
//               rulePtr->PathExpressionSize/sizeof(WCHAR),
//               rulePtr->PathExpression,
//               rulePtr->PathExpressionSize);
//
//        thisRuleSize = sizeof(FG_RULE) + rulePtr->PathExpressionSize;
//        printf("This rule size: %lu\n", thisRuleSize);
//        rulePtr = (FG_RULE*)Add2Ptr(rulePtr, thisRuleSize);
//        querySize -= thisRuleSize;
//    }
//
//Cleanup:
//
//    if (INVALID_HANDLE_VALUE != port) {
//        FglDisconnectCore(port);
//    }
}