#include <stdio.h>
#include <wchar.h>
#include <locale.h>

#include <windows.h>
#include <fltUser.h>

#include <iostream>
#include <string>
#include <optional>
#include <variant>
#include <memory>
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
            return std::make_unique<CoreClient>(port);
        }

        CoreClient(HANDLE port) {
            port_.exchange(port);
            GetCoreVersion();
        }

        ~CoreClient() {
            auto port = port_.exchange(INVALID_HANDLE_VALUE);
            if (INVALID_HANDLE_VALUE != port) CloseHandle(port);
        }

        HRESULT GetCoreVersion() {
            FG_CORE_VERSION core_version = {};
            auto hr = FglGetCoreVersion(port_, &core_version);
            if (FAILED(hr)) return hr;
            core_version_ = core_version;
        }

    private:
        std::optional<FG_CORE_VERSION> core_version_;
        std::atomic<HANDLE> port_;
    };

    class Admin {
    public:
        static std::variant<std::unique_ptr<Admin>, HRESULT> NewAdmin(int argc, wchar_t** argv) noexcept {
            auto admin = std::make_unique<Admin>(argc, argv);
            return std::move(admin);
        }

        Admin(int argc, wchar_t** argv) {
            argc_ = argc;
            argv_ = argv;
        };
        ~Admin() = default;

        void PrintUsage() {
            const char* USAGE_FORMAT =
                "%ws\n"
                "Version: %hu.%hu.%hu.%hu\n"
                "This tool is used to operate rules\n"
                "\n"
                "Command\n"
                "  add\n"
                "    --expr, -e <expression> \n"
                "    --type, -t <access-denied|readonly|hide>\n"
                "  remove\n"
                "    --expr, -e <expression>\n"
                "    --type, -t <access-denied|readonly|hide>\n"
                "  query\n"
                "    --format, -f <list|csv|json>\n"
                "  check-matched\n"
                "    --path, -p <path>\n"
                "  clean\n"
                "\n"
                "Flags\n"
                "  --help, -h\n"
                "  -version, -v\n";

            printf(USAGE_FORMAT, AdminImageName(),
                FGA_MAJOR_VERSION, FGA_MINOR_VERSION,
                FGA_PATCH_VERSION, FGA_BUILD_VERSION);
        }

    private:
        int argc_;
        wchar_t** argv_;

        std::wstring& AdminImageName() {
            static std::wstring image_name;
            if (image_name.empty()) {
                std::wstring image_path = argv_[0];
                size_t pos = image_path.find_first_of('\\');
                image_name = image_path.substr(pos + 1);
            }
            return image_name;
        }

        std::optional<std::unique_ptr<CoreClient>> core_client_;
    };
}

int wmain(int argc, wchar_t* argv[]) {
    HRESULT hr = S_OK;
    HANDLE port = INVALID_HANDLE_VALUE;
    FG_CORE_VERSION coreVersion = { 0 };
    FGL_RULE rules[] = {
        { RULE_READONLY, L"*\\baned.txt" },
        { RULE_READONLY, L"*baned.txt" },
        { RULE_READONLY, L"*\\baned.txt" }
    };
    FGL_RULE rule = { 0 };
    BOOLEAN added = FALSE, removed = FALSE;
    USHORT addedAmount = 0, removedAmount = 0;
    ULONG cleanRules = 0;
    PFG_MESSAGE message = NULL;

    UCHAR *buffer = NULL;
    USHORT queryAmount = 0;
    ULONG querySize = 0ul;
    FG_RULE *rulePtr = NULL;
    ULONG thisRuleSize = 0ul;
    INT i = 0;

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

    rule.RuleCode = RULE_READONLY;
    rule.RulePathExpression = L"\\Device\\HarddiskVolume\\*";
    hr = FglAddSingleRule(port, &rule, &added);
    if (FAILED(hr)) {
        fprintf(stderr, "add single rule failed: 0x%08x", hr);
        goto Cleanup;
    }

    ////hr = FglRemoveSingleRule(port, &rule, &removed);
    ////if (FAILED(hr)) {
    ////    fprintf(stderr, "remove single rule failed: 0x%08x", hr);
    ////    goto Cleanup;
    ////}

    hr = FglAddBulkRules(port, rules, 3, &addedAmount);
    if (FAILED(hr)) {
        fprintf(stderr, "add rules failed: 0x%08x", hr);
        goto Cleanup;
    }

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
    
    hr = FglCheckMatchedRules(port,
                              L"\\Device\\HarddiskVolume6\\baned.txt",
                              NULL, 
                              0,
                              &queryAmount,
                              &querySize);
    printf("hresult: 0x%08x, matched amount: %hu, size: %lu\n", hr, queryAmount, querySize);

    buffer = (UCHAR*)malloc(querySize);
    if (NULL == buffer) goto Cleanup;
    else RtlZeroMemory(buffer, querySize);
    hr = FglCheckMatchedRules(port,
                             L"\\Device\\HarddiskVolume6\\baned.txt",
                             (FG_RULE*)buffer, 
                             querySize,
                             &queryAmount, 
                             &querySize);
    printf("hresult: 0x%08x, matched amount: %hu, size: %lu\n", hr, queryAmount, querySize);
    rulePtr = (FG_RULE*)buffer;
    while (querySize > 0) {
        printf("remain size: %lu, rule code: %lu, path expression: %.*ls, size: %lu\n", 
               querySize,
               rulePtr->RuleCode, 
               rulePtr->PathExpressionSize/sizeof(WCHAR),
               rulePtr->PathExpression,
               rulePtr->PathExpressionSize);

        thisRuleSize = sizeof(FG_RULE) + rulePtr->PathExpressionSize;
        printf("This rule size: %lu\n", thisRuleSize);
        rulePtr = (FG_RULE*)Add2Ptr(rulePtr, thisRuleSize);
        querySize -= thisRuleSize;
    }

Cleanup:

    if (INVALID_HANDLE_VALUE != port) {
        FglDisconnectCore(port);
    }
}