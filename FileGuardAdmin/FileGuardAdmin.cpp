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
#include <vector>
#include <algorithm>

EXTERN_C_START
#include "FileGuard.h"
#include "FileGuardLib.h"
EXTERN_C_END

#define FGA_MAJOR_VERSION ((USHORT)0)
#define FGA_MINOR_VERSION ((USHORT)1)
#define FGA_PATCH_VERSION ((USHORT)0)
#define FGA_BUILD_VERSION ((USHORT)0)

namespace fileguard {
    
    struct Rule {
        FG_RUEL_CODE code;
        std::wstring_view path_expression;
        const std::shared_ptr<char[]> buf; 

        Rule(FG_RUEL_CODE c,
            std::wstring_view path_expression, 
            const std::shared_ptr<char[]> buf):
            code(c), path_expression(path_expression), buf(buf) {}
    };

    ULONG RuleNameToCode(std::wstring& rule_type) {
        std::transform(rule_type.begin(), rule_type.end(), rule_type.begin(),
            [](wchar_t c) { return std::tolower(c); });

        if (L"access-denied" == rule_type) return RuleAccessDenined;
        else if (L"readonly" == rule_type) return RuleReadOnly;
        else if (L"hide" == rule_type) return RuleHide;
        return RuleNone;
    }

    std::wstring RuleCodeToName(FG_RUEL_CODE code) {
        switch (code) {
        case RuleAccessDenined: return L"access-denied";
        case RuleReadOnly: return L"readonly";
        case RuleHide: return L"hide";
        }
        return L"";
    }

    std::vector<std::unique_ptr<Rule>> ResolveRulesBuffer(std::shared_ptr<char[]> buf, size_t buf_size) {
        std::vector<std::unique_ptr<Rule>> rules;
        char* rule_offset_ptr = buf.get();
        while (buf_size > 0) {
            auto rule_ptr = reinterpret_cast<FG_RULE*>(rule_offset_ptr);
            auto rule = std::make_unique<Rule>(rule_ptr->RuleCode, 
                                               std::wstring_view(rule_ptr->PathExpression, rule_ptr->PathExpressionSize/sizeof(wchar_t)),
                                               buf);
            rules.push_back(std::move(rule));
            
            rule_offset_ptr = rule_offset_ptr + sizeof(FG_RULE) + rule_ptr->PathExpressionSize;
            buf_size -= sizeof(FG_RULE) + rule_ptr->PathExpressionSize;
        }

        return rules;
    }

    class CoreClient {
    public:
        static std::pair<std::unique_ptr<CoreClient>, HRESULT> New() noexcept {
            HANDLE port = INVALID_HANDLE_VALUE;
            HRESULT hr = FglConnectCore(&port);
            return std::pair(std::unique_ptr<CoreClient>(new CoreClient(port)), hr);
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
        
        std::variant<bool, HRESULT> AddSingleRule(unsigned long rule_code, std::wstring rule_path_expression) {
            FGL_RULE rule{ rule_code, rule_path_expression.c_str()};
            BOOLEAN added = FALSE;
            auto hr = FglAddSingleRule(port_, &rule, &added);
            if (FAILED(hr)) return hr;
            return bool(added);
        }

        std::variant<bool, HRESULT> RemoveSingleRule(unsigned long rule_code, std::wstring rule_path_expression) {
            FGL_RULE rule{ rule_code, rule_path_expression.c_str() };
            BOOLEAN removed = FALSE;
            auto hr = FglRemoveSingleRule(port_, &rule, &removed);
            if (FAILED(hr)) return hr;
            return bool(removed);
        }

        std::variant<std::vector<std::unique_ptr<Rule>>, HRESULT> QueryRules() {
            unsigned short amount = 0;
            unsigned long size = 0ul;
            auto hr = FglQueryRules(port_, NULL, 0, &amount, &size);
            if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) return hr;
            
            std::shared_ptr<char[]> buf(new char[size], std::default_delete<char[]>());
            hr = FglQueryRules(port_, reinterpret_cast<FG_RULE*>(buf.get()), size, &amount, &size);
            if (FAILED(hr)) return hr;
            return ResolveRulesBuffer(buf, size);
        }

        std::variant<std::vector<std::unique_ptr<Rule>>, HRESULT> CheckMatchedRules(std::wstring path) {
            unsigned short amount = 0;
            unsigned long size = 0ul;
            auto hr = FglCheckMatchedRules(port_, path.c_str(), NULL, 0, &amount, &size);
            if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) return hr;

            std::shared_ptr<char[]> buf(new char[size], std::default_delete<char[]>());
            hr = FglCheckMatchedRules(port_, path.c_str(), reinterpret_cast<FG_RULE*>(buf.get()), size, &amount, &size);
            if (FAILED(hr)) return hr;
            return ResolveRulesBuffer(buf, size);
        }

        std::variant<unsigned long, HRESULT> CleanupRules() {
            unsigned long cleanRules = 0ul;
            auto hr = FglCleanupRules(port_, &cleanRules);
            if (FAILED(hr)) return hr;
            return cleanRules;
        }

    private:
        std::atomic<HANDLE> port_ = INVALID_HANDLE_VALUE;

        CoreClient(HANDLE port) { port_.exchange(port); }
    };

    class Admin {
    public:
        static std::variant<std::unique_ptr<Admin>, HRESULT> New(int argc, wchar_t** argv) noexcept {
            auto result = CoreClient::New();
            std::unique_ptr<Admin> admin(new Admin(argc, argv, std::move(result.first)));
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
                        return E_INVALIDARG;
                    }
                }
            }

            std::wstring flag = argv_[1];
            if (flag == L"--help" || flag == L"-h") {
                PrintUsage();
                return hr;
            }
            if (flag == L"--version" || flag == L"-v") {
                std::wcout << GetVersionInfo() << std::endl;
                return hr;
            }

            std::wstring command = argv_[1];
            if (command == L"add") hr = CommandAdd(args);
            else if (command == L"remove") hr = CommandRemove(args);
            else if (command == L"query") hr = CommandQuery(args);
            else if (command == L"check-matched") hr = CommandCheckMatched(args);
            else if (command == L"cleanup") hr = CommandCleanup(args);
            else {
                std::wcerr << L"error: invalid flag or command: '" << command << L"'" << std::endl;
                return E_INVALIDARG;
            }

            switch (hr) {
            case  E_HANDLE:
                std::wcerr << L"error: cannot connect to core, hresult: " 
                           << std::setfill(L'0') << std::setw(8) << std::hex << hr
                           << std::endl;
                break;
            case E_INVALIDARG:
                break;
            default:
                if(FAILED(hr))
                    std::wcerr << L"error: unhandle command error hresult: " 
                               << std::setfill(L'0') << std::setw(8) << std::hex << hr 
                               << std::endl;
            }

            return hr;
        }

        void PrintUsage() {
            std::wcout << GetAdminImageName() <<
                L" [command] [flags]\n" << GetVersionInfo() <<
                L"\n\nThis tool is used to operate rules\n\n"
                L"commands:\n"
                L"    add: Add a rule.\n"
                L"        --type <access-denied|readonly|hide>\n"
                L"        --expr <expression> \n"
                L"\n"
                L"    remove: Remove a rule.\n"
                L"        --type <access-denied|readonly|hide>\n"
                L"        --expr <expression>\n"
                L"\n"
                L"    query: Query all rules and output it.\n"
                L"        --format <list|csv>\n"
                L"\n"
                L"    check-matched: Check which rules will matched for path.\n"
                L"        --path   <path>\n"
                L"        --format <list|csv>\n"
                L"\n"
                L"    cleanup: Cleanup all rules.\n"
                L"\n"
                L"flags:\n"
                L"    --help, -h\n"
                L"    --version, -v\n";
        }

    private:
        int argc_;
        wchar_t** argv_;
        std::unique_ptr<CoreClient> core_client_;

        Admin(int argc, wchar_t** argv, std::unique_ptr<CoreClient> core_client) {
            argc_ = argc;
            argv_ = argv;
            core_client_ = std::move(core_client);
        };

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
                          << FGA_BUILD_VERSION << L", "
                          << L"Core: " << core_ver_wstr;

            return admin_ver_wos.str();
        }

        HRESULT CommandAdd(std::map<std::wstring, int>& args) {
            const std::wstring fn_type = L"--type", fn_expr = L"--expr";
            auto f_type = args.find(fn_type), f_expr = args.find(fn_expr);
            std::wstring v_type, v_expr;

            if (f_type == args.end()) {
                std::wcerr << L"error: command flag `" << fn_type << "` required\n";
                return E_INVALIDARG;
            }
            if (f_expr == args.end()) {
                std::wcerr << L"error: command flag `" << fn_expr << "` required\n";
                return E_INVALIDARG;
            }

            if (f_type->second + 1 >= argc_) {
                std::wcerr << L"error: command flag `" << fn_type << L"` value invalid\n";
                return E_INVALIDARG;
            }
            if (f_expr->second + 1 >= argc_) {
                std::wcerr << L"error: command flag `" << fn_expr << "` value invalid\n";
                return E_INVALIDARG;
            }

            v_type = argv_[f_type->second + 1];
            v_expr = argv_[f_expr->second + 1];

            if (v_type == fn_expr) {
                std::wcerr << L"error: command flag `" << fn_type << "` value empty" << std::endl;
                return E_INVALIDARG;
            } else if (v_expr == fn_type) {
                std::wcerr << L"error: command flag `" << fn_expr << "` value empty" << std::endl;
                return E_INVALIDARG;
            }

            auto v_type_code = RuleNameToCode(v_type);
            if (!VALID_RULE_CODE(v_type_code)) {
                std::wcerr << "error: invalid rule type: `" << v_type << "`" << std::endl;
                return E_INVALIDARG;
            }

            auto result = core_client_->AddSingleRule(v_type_code, v_expr);
            if (auto added = std::get_if<bool>(&result)) {
                if (*added) std::wcout << L"Add rule successfully" << std::endl;
                else std::wcout << L"Rule already exist" << std::endl;
            } else {
                return std::get<HRESULT>(result);
            }

            return S_OK;
        }

        HRESULT CommandRemove(std::map<std::wstring, int>& args) {
            const std::wstring fn_type = L"--type", fn_expr = L"--expr";
            auto f_type = args.find(fn_type), f_expr = args.find(fn_expr);
            std::wstring v_type, v_expr;

            if (f_type == args.end()) {
                std::wcerr << L"error: command flag `" << fn_type << "` required\n";
                return E_INVALIDARG;
            }
            if (f_expr == args.end()) {
                std::wcerr << L"error: command flag `" << fn_expr << "` required\n";
                return E_INVALIDARG;
            }

            if (f_type->second + 1 >= argc_) {
                std::wcerr << L"error: command flag `" << fn_type << L"` value invalid\n";
                return E_INVALIDARG;
            }
            if (f_expr->second + 1 >= argc_) {
                std::wcerr << L"error: command flag `" << fn_expr << L"` value invalid\n";
                return E_INVALIDARG;
            }

            v_type = argv_[f_type->second + 1];
            v_expr = argv_[f_expr->second + 1];

            if (v_type == fn_expr) {
                std::wcerr << L"error: command flag `" << fn_type << "` value empty" << std::endl;
                return E_INVALIDARG;
            } else if (v_expr == fn_type) {
                std::wcerr << L"error: command flag `" << fn_expr << "` value empty" << std::endl;
                return E_INVALIDARG;
            }

            auto v_type_code = RuleNameToCode(v_type);
            if (!VALID_RULE_CODE(v_type_code)) {
                std::wcerr << "error: invalid rule type: `" << v_type << "`" << std::endl;
                return E_INVALIDARG;
            }

            auto result = core_client_->RemoveSingleRule(RuleNameToCode(v_type), v_expr);
            if (auto removed = std::get_if<bool>(&result)) {
                if (*removed) std::wcout << L"Remove rule successfully" << std::endl;
                else std::wcout << "Rule not found" << std::endl;
            } else {
                return std::get<HRESULT>(result);
            }

            return S_OK;
        }

        HRESULT CommandQuery(std::map<std::wstring, int>& args) {
            const std::wstring fn_format = L"--format";
            auto f_format = args.find(fn_format);
            std::wstring v_format;

            if (f_format == args.end()) {
                std::wcerr << L"error: command flag `"<< fn_format <<"` required\n";
                return E_INVALIDARG;
            }

            if (f_format->second + 1 >= argc_) {
                std::wcerr << L"error: command flag `" << f_format->first << L"` value invalid\n";
                return E_INVALIDARG;
            }

            v_format = argv_[f_format->second + 1];
            std::transform(v_format.begin(), v_format.end(), v_format.begin(),
                [](wchar_t c) { return std::tolower(c); });

            // Query rules.
            auto result = core_client_->QueryRules();
            if (auto hr = std::get_if<HRESULT>(&result)) return *hr;
            auto rules = std::get_if<std::vector<std::unique_ptr<Rule>>>(&result);

            // Output rules query result.
            if (v_format == L"csv") std::wcout << "code,expression" << std::endl;
            else if (v_format == L"list") std::wcout << "total: " << rules->size() << std::endl;
            else { 
                std::wcerr << "error: invalid format: '" << v_format << "'" << std::endl;
                return E_INVALIDARG;
            }
            std::for_each(rules->begin(), rules->end(),
                [&v_format](const std::unique_ptr<Rule>& rule) {
                    if (v_format == L"csv") {
                        std::wcout << RuleCodeToName(rule->code) << ","
                                   << rule->path_expression
                                   << std::endl;
                    } else if (v_format == L"list") {
                        std::wcout << "code: " << RuleCodeToName(rule->code)
                                   << ", expression: " << rule->path_expression
                                   << std::endl;
                    }
                });

            return S_OK;
        }

        HRESULT CommandCheckMatched(std::map<std::wstring, int> args) {
            const std::wstring fn_path = L"--path", fn_format = L"--format";
            auto f_format = args.find(fn_format), f_path = args.find(fn_path);
            std::wstring v_path, v_format;

            if (f_path == args.end()) {
                std::wcerr << L"error: command flag `"<< fn_path <<"` required\n";
                return E_INVALIDARG;
            } 
            if (f_format == args.end()) {
                std::wcerr << L"error: command flag `" << fn_format << "` required\n";
                return E_INVALIDARG;
            }
            
            if (f_path->second + 1 >= argc_) {
                std::wcerr << L"error: command flag `" << fn_path << L"` value invalid\n";
                return E_INVALIDARG;
            }
            if (f_format->second +1 >= argc_) {
                std::wcerr << L"error: command flag `" << fn_format << "` value invalid\n";
                return E_INVALIDARG;
            }

            v_path = argv_[f_path->second + 1];
            v_format = argv_[f_format->second + 1];
            std::transform(v_format.begin(), v_format.end(), v_format.begin(),
                [](wchar_t c) { return std::tolower(c); });

            if (v_path == fn_format) {
                std::wcerr << L"error: command flag `" << fn_path << "` value empty" << std::endl;
                return E_INVALIDARG;
            } else if (v_format == fn_path) {
                std::wcerr << L"error: command flag `" << fn_format << "` value empty" << std::endl;
                return E_INVALIDARG;
            }

            // Check matched rules.
            auto result = core_client_->CheckMatchedRules(v_path);
            if (auto hr = std::get_if<HRESULT>(&result)) return *hr;
            auto rules = std::get_if<std::vector<std::unique_ptr<Rule>>>(&result);

            // Output matched rules result.
            if (v_format == L"csv") std::wcout << "code,expression" << std::endl;
            else if (v_format == L"list") std::wcout << "total: " << rules->size() << std::endl;
            else {
                std::wcerr << "error: invalid format: '" << v_format << "'" << std::endl;
                return E_INVALIDARG;
            }
            std::for_each(rules->begin(), rules->end(), 
                [&v_format](const std::unique_ptr<Rule>& rule) {
                    if (v_format == L"csv") {
                        std::wcout << RuleCodeToName(rule->code) << ","
                                   << rule->path_expression
                                   << std::endl;
                    } else if (v_format == L"list") {
                        std::wcout << "code: " << RuleCodeToName(rule->code)
                                   << ", expression: " << rule->path_expression
                                   << std::endl;
                    }
                });

            std::wcout << v_path << ", " << v_format << std::endl;
            return S_OK;
        }

        HRESULT CommandCleanup(std::map<std::wstring, int> args) {
            auto result = core_client_->CleanupRules();
            if (auto hr = std::get_if<HRESULT>(&result)) return *hr;

            std::wcout << L"Cleanup rules amount: " 
                       << std::get<unsigned long>(result) 
                       << std::endl;
            return S_OK;
        }
    };
}

int wmain(int argc, wchar_t* argv[]) {
    auto admin_opt = fileguard::Admin::New(argc, argv);
    if (auto hr = std::get_if<HRESULT>(&admin_opt)) {
        std::cerr << "0x" << std::hex << std::setfill('0') << std::setw(8) << hr << std::endl;
        return *hr;
    }
    return std::get<std::unique_ptr<fileguard::Admin>>(std::move(admin_opt))->Parse();
}