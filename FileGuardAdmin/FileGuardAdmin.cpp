/*++

    The MIT License (MIT)

    Copyright (c) 2023 Fxtack

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Module Name:

    FileGuardAdmin.cpp

Abstract:

    FileGuardAdmin command line.

Environment:

    User mode.

--*/

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

#pragma warning(push)
#pragma warning(disable: 4423)
#include "CLI11/CLI11.hpp"
#pragma warning(pop)

#define FGA_MAJOR_VERSION ((USHORT)0)
#define FGA_MINOR_VERSION ((USHORT)1)
#define FGA_PATCH_VERSION ((USHORT)0)
#define FGA_BUILD_VERSION ((USHORT)0)

#define HEX(_num_) L"0x" << std::hex << std::setfill(L'0') << std::setw(8) << (_num_)

#define SYSTEMTIME(_time_) std::setfill(L'0') << std::setw(4) << (_time_).wYear << L"-" \
                                              << std::setw(2) << (_time_).wMonth << L"-" \
                                              << std::setw(2) << (_time_).wDay << L" " \
                                              << std::setw(2) << (_time_).wHour << L":" \
                                              << std::setw(2) << (_time_).wMinute << L":" \
                                              << std::setw(2) << (_time_).wSecond << L"." \
                                              << std::setw(3) << (_time_).wMilliseconds

namespace fileguard {
    
    struct Rule {
        FG_RULE_CODE code;
        std::wstring_view path_expression;
        const std::shared_ptr<char[]> buf; 

        Rule(FG_RULE_CODE code,
            std::wstring_view path_expression, 
            const std::shared_ptr<char[]> buf):
            code(code), path_expression(path_expression), buf(buf) { }
    };

    FG_RULE_MAJOR_CODE RuleMajorNameToCode(std::wstring& major_name) {
        std::transform(major_name.begin(), major_name.end(), major_name.begin(),
            [](wchar_t c) { return std::tolower(c); });

        if (L"access-denied" == major_name) return FG_RULE_MAJOR_CODE::RuleMajorAccessDenied;
        else if (L"readonly" == major_name) return FG_RULE_MAJOR_CODE::RuleMajorReadonly;
        return FG_RULE_MAJOR_CODE::RuleMajorNone;
    }

    std::wstring RuleMajorName(FG_RULE_CODE& code) {
        switch (code.Major) {
        case FG_RULE_MAJOR_CODE::RuleMajorAccessDenied: return L"access-denied";
        case FG_RULE_MAJOR_CODE::RuleMajorReadonly: return L"readonly";
        }
        return L"";
    }

    FG_RULE_MINOR_CODE RuleMinorNameToCode(std::wstring& minor_name) {
        std::transform(minor_name.begin(), minor_name.end(), minor_name.begin(),
            [](wchar_t c) { return std::tolower(c); });

        if (L"monitored" == minor_name) return FG_RULE_MINOR_CODE::RuleMinorMonitored;
        return FG_RULE_MINOR_CODE::RuleMinorNone;
    }

    std::wstring RuleMinorName(FG_RULE_CODE& code) {
        switch (code.Minor) {
        case FG_RULE_MINOR_CODE::RuleMinorMonitored: return L"monitored";
        }
        return L"";
    }

    std::wstring MajorIRPName(UCHAR major_irp_code) {
        const UCHAR IRP_MJ_CREATE = 0x00;
        const UCHAR IRP_MJ_CLOSE = 0x02;
        const UCHAR IRP_MJ_WRITE = 0x04;
        const UCHAR IRP_MJ_SET_INFORMATION = 0x06;
        const UCHAR IRP_MJ_FILE_SYSTEM_CONTROL = 0x0d;

        switch (major_irp_code) {
        case IRP_MJ_CREATE: return L"IRP_MJ_CREATE";
        case IRP_MJ_CLOSE: return L"IRP_MJ_CLOSE";
        case IRP_MJ_WRITE: return L"IRP_MJ_WRITE";
        case IRP_MJ_SET_INFORMATION: return L"IRP_MJ_SET_INFORMATION";
        case IRP_MJ_FILE_SYSTEM_CONTROL: return L"IRP_MJ_FILE_SYSTEM_CONTROL";
        default: return L"Unknown";
        }
    }

    std::vector<std::unique_ptr<Rule>> ResolveRulesBuffer(std::shared_ptr<char[]> buf, size_t buf_size) {
        std::vector<std::unique_ptr<Rule>> rules;
        char* rule_offset_ptr = buf.get();
        while (buf_size > 0) {
            auto rule_ptr = reinterpret_cast<FG_RULE*>(rule_offset_ptr);
            auto rule = std::make_unique<Rule>(rule_ptr->Code,
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

        std::optional<HRESULT> ReceiveMonitorRecords(volatile BOOLEAN* End, MonitorRecordCallback callback) {
            auto hr = FglReceiveMonitorRecords(End, callback);
            return SUCCEEDED(hr) ? std::nullopt : std::make_optional(hr);
        }

        std::optional<HRESULT> SetUnloadAcceptable(bool acceptable) {
            auto hr = FglSetUnloadAcceptable(port_, acceptable);
            return SUCCEEDED(hr) ? std::nullopt : std::make_optional(hr);
        }

        std::optional<HRESULT> SetDetachAcceptable(bool acceptable) {
            auto hr = FglSetDetachAcceptable(port_, acceptable);
            return SUCCEEDED(hr) ? std::nullopt : std::make_optional(hr);
        }
        
        std::variant<bool, HRESULT> AddSingleRule(FG_RULE_CODE& code, std::wstring rule_path_expression) {
            FGL_RULE rule{ code, rule_path_expression.c_str()};
            BOOLEAN added = FALSE;
            auto hr = FglAddSingleRule(port_, &rule, &added);
            if (FAILED(hr)) return hr;
            return bool(added);
        }

        std::variant<bool, HRESULT> RemoveSingleRule(FG_RULE_CODE& code, std::wstring rule_path_expression) {
            FGL_RULE rule{ code, rule_path_expression.c_str() };
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

            CLI::App app("This tool is used to manage file access rules and control the driver.");

            app.set_help_flag("--help", "Print this help message and exit");
            app.set_version_flag("--version", GetVersionInfo());

            app.require_subcommand(1);

            auto unload_cmd = app.add_subcommand("unload", "Unload core driver");
            unload_cmd->callback([&]() { hr = CommandUnload(); });

            auto detach_cmd = app.add_subcommand("detach", "Detach core instance");
            std::wstring volume;
            detach_cmd->add_option("--volume", volume, "Detach instance volume path")->required();
            detach_cmd->callback([&]() { hr = CommandDetach(volume); });

            auto add_cmd = app.add_subcommand("add", "Add a rule");
            std::wstring major_type, minor_type, expr;
            add_cmd->add_option("--major-type", major_type, "Rule major type")->required();
            add_cmd->add_option("--minor-type", minor_type, "Rule minor type")->default_val("monitored");
            add_cmd->add_option("--expr", expr, "Rule path expression")->required();
            add_cmd->callback([&]() { hr = CommandAdd(major_type, minor_type, expr); });

            auto remove_cmd = app.add_subcommand("remove", "Remove a rule");
            remove_cmd->add_option("--major-type", major_type, "Rule major type")->required();
            remove_cmd->add_option("--minor-type", minor_type, "Rule minor type")->default_val("monitored");
            remove_cmd->add_option("--expr", expr, "Rule path expression")->required();
            remove_cmd->callback([&]() { hr = CommandRemove(major_type, expr, minor_type); });

            auto query_cmd = app.add_subcommand("query", "Query all rules and output it");
            std::wstring format = L"list";
            query_cmd->add_option("--format", format, "Default output format")->default_val("list");
            query_cmd->callback([&]() { hr = CommandQuery(format); });

            auto check_cmd = app.add_subcommand("check-matched", "Check which rules will matched for path");
            std::wstring path;
            check_cmd->add_option("--path", path, "Check for matching paths")->required();
            check_cmd->add_option("--format", format, "Default format")->default_val("list");
            check_cmd->callback([&]() { hr = CommandCheckMatched(path, format); });

            auto monitor_cmd = app.add_subcommand("monitor", "Receive monitoring records");
            monitor_cmd->add_option("--format", format, "Default output format")->default_val("list");
            monitor_cmd->callback([&]() { hr = CommandMonitor(format); });

            auto cleanup_cmd = app.add_subcommand("cleanup", "Cleanup all rules");
            cleanup_cmd->callback([&]() { hr = CommandCleanup(); });

            CLI11_PARSE(app, argc_, argv_);

            return hr;
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

        std::string GetVersionInfo() {
            std::string core_ver_str;
            std::ostringstream core_ver_os;

            if (nullptr != core_client_) {
                auto core_ver_opt = core_client_->GetCoreVersion();
                if (std::holds_alternative<FG_CORE_VERSION>(core_ver_opt)) {
                    auto core_ver = std::get<FG_CORE_VERSION>(core_ver_opt);
                    core_ver_os << "v"
                                << core_ver.Major << "."
                                << core_ver.Minor << "."
                                << core_ver.Patch << "."
                                << core_ver.Build;
                } else if (std::holds_alternative<HRESULT>(core_ver_opt)) {
                    core_ver_os << "(error: 0x" << std::hex << std::setfill('0') << std::setw(8)
                                << std::get<HRESULT>(core_ver_opt) << ")";
                }
                core_ver_str = core_ver_os.str();
            } else {
                core_ver_str = "(core not connected)";
            }

            std::ostringstream admin_ver_os;
            admin_ver_os << "Admin: v"
                          << FGA_MAJOR_VERSION << "."
                          << FGA_MINOR_VERSION << "."
                          << FGA_PATCH_VERSION << "."
                          << FGA_BUILD_VERSION << ", "
                          << "Core: " << core_ver_str;

            return admin_ver_os.str();
        }

        HRESULT CommandUnload() {
            auto result = core_client_->SetUnloadAcceptable(true);
            if (result) {
                auto hr = result.value();
                std::wcerr << L"error: set core unload acceptable to FALSE failed: " << HEX(hr) << std::endl;
                return hr;
            }

            HANDLE token = INVALID_HANDLE_VALUE;
            auto ok = OpenProcessToken(GetCurrentProcess(),
                                       TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                       &token);
            if (!ok) {
                auto hr = HRESULT_FROM_WIN32(GetLastError());
                std::wcerr << L"error: unload core failed: " << HEX(hr) << std::endl;
                return hr;
            }

            TOKEN_PRIVILEGES tp = { 0 };
            LUID luid = { 0 };
            ok = LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &luid);
            if (!ok) {
                auto hr = HRESULT_FROM_WIN32(GetLastError());
                std::wcerr << L"error: unload core failed: " << HEX(hr) << std::endl;
                return hr;
            }

            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            ok = AdjustTokenPrivileges(token,
                                       FALSE, 
                                       &tp,
                                       sizeof(TOKEN_PRIVILEGES), 
                                       NULL, 
                                       NULL);
            if (!ok) {
                auto hr = HRESULT_FROM_WIN32(GetLastError());
                std::wcerr << L"error: unload core failed: " << HEX(hr) << std::endl;
                return hr;
            }

            auto hr = FilterUnload(FG_CORE_FILTER_NAME);
            if (FAILED(hr)) {
                std::wcerr << L"error: unload core failed: " << HEX(hr) << std::endl;
                return hr;
            }

            return S_OK;
        }

        HRESULT CommandDetach(std::wstring& volume) {
            auto result = core_client_->SetDetachAcceptable(true);
            if (result) {
                auto hr = result.value();
                std::wcerr << L"error: set core detach acceptable to TRUE failed, hresult: "
                           << HEX(hr) << std::endl;
                return hr;
            }

            auto hr = FilterDetach(FG_CORE_FILTER_NAME, volume.c_str(), NULL);
            if (FAILED(hr)) {
                std::wcerr << L"error: detach volume '" << volume << L"' instance failed, hresult: "
                           << HEX(hr) << std::endl;
            }

            result = core_client_->SetDetachAcceptable(false);
            if (result) {
                auto hr = result.value();
                std::wcerr << L"error: set core detach acceptable to FALSE failed, hresult: "
                           << HEX(hr) << std::endl;
                return hr;
            }

            return S_OK;
        }

        HRESULT CommandAdd(std::wstring& major_type, std::wstring& minor_type, std::wstring& expr) {
            FG_RULE_CODE code;
            code.Major = RuleMajorNameToCode(major_type);
            code.Minor = RuleMinorNameToCode(minor_type);
            if (!VALID_RULE_CODE(code)) {
                std::wcerr << "error: invalid rule type, major: `" << major_type
                                                  << "`, minor: `" << minor_type << "`\n";
                return E_INVALIDARG;
            }
            
            auto result = core_client_->AddSingleRule(code, expr);
            if (auto added = std::get_if<bool>(&result)) {
                if (*added) std::wcout << L"Add rule successfully" << std::endl;
                else std::wcout << L"Rule already exist" << std::endl;
            } else {
                return std::get<HRESULT>(result);
            }

            return S_OK;
        }

        HRESULT CommandRemove(std::wstring& major_type, std::wstring& minor_type, std::wstring& expr) {
            FG_RULE_CODE code;
            code.Major = RuleMajorNameToCode(major_type);
            code.Minor = RuleMinorNameToCode(minor_type);
            if (!VALID_RULE_CODE(code)) {
                std::wcerr << "error: invalid rule type, major: `" << major_type
                    << "`, minor: `" << minor_type << "`\n";
                return E_INVALIDARG;
            }

            auto result = core_client_->RemoveSingleRule(code, expr);
            if (auto removed = std::get_if<bool>(&result)) {
                if (*removed) std::wcout << L"Remove rule successfully" << std::endl;
                else std::wcout << "Rule not found" << std::endl;
            } else {
                return std::get<HRESULT>(result);
            }

            return S_OK;
        }

        HRESULT CommandQuery(std::wstring& format) {
            if (format != L"list" && format != L"csv") {
                std::wcerr << "error: invalid format: '" << format << "'" << std::endl;
                return E_INVALIDARG;
            }

            // Query rules.
            auto result = core_client_->QueryRules();
            if (auto hr = std::get_if<HRESULT>(&result)) return *hr;
            auto rules = std::get_if<std::vector<std::unique_ptr<Rule>>>(&result);
            if (rules->empty()) {
                std::wcout << "empty query result" << std::endl;
                return S_OK;
            }

            // Output rules query result.
            if (format == L"csv") std::wcout << "major_code,minor_code,expression" << std::endl;
            
            auto total_rules = rules->size();
            auto index = 0;
            std::for_each(rules->begin(), rules->end(),
                [&total_rules, &index, &format](const std::unique_ptr<Rule>& rule) {
                    if (format == L"csv") {
                        std::wcout << RuleMajorName(rule->code) << ","
                                   << RuleMinorName(rule->code) << ","
                                   << rule->path_expression
                                   << std::endl;
                    } else if (format == L"list") {
                        std::wcout << "     index: " << index << "/" << total_rules << std::endl
                                   << "major type: " << RuleMajorName(rule->code) << std::endl
                                   << "minor type: " << RuleMinorName(rule->code) << std::endl
                                   << "expression: " << rule->path_expression << std::endl
                                   << std::endl;
                        index++;
                    }
                });

            return S_OK;
        }

        HRESULT CommandCheckMatched(std::wstring& path, std::wstring& format) {
            if (format != L"list" && format != L"csv") {
                std::wcerr << "error: invalid format: '" << format << "'" << std::endl;
                return E_INVALIDARG;
            }

            // Check matched rules.
            auto result = core_client_->CheckMatchedRules(path);
            if (auto hr = std::get_if<HRESULT>(&result)) return *hr;
            auto rules = std::get_if<std::vector<std::unique_ptr<Rule>>>(&result);
            if (rules->empty()) {
                std::wcout << "no rule matched" << std::endl;
                return S_OK;
            }

            // Output matched rules result.
            if (format == L"csv") std::wcout << "major_code,minor_code,expression" << std::endl;

            auto total_rules = rules->size();
            auto index = 0;
            std::for_each(rules->begin(), rules->end(),
                [&total_rules, &index, &format](const std::unique_ptr<Rule>& rule) {
                    if (format == L"csv") {
                        std::wcout << RuleMajorName(rule->code) << ","
                                   << RuleMinorName(rule->code) << ","
                                   << rule->path_expression
                                   << std::endl;
                    } else if (format == L"list") {
                        std::wcout << "     index: " << index << "/" << total_rules << std::endl
                                   << "major type: " << RuleMajorName(rule->code) << std::endl
                                   << "minor type: " << RuleMinorName(rule->code) << std::endl
                                   << "expression: " << rule->path_expression << std::endl
                                   << std::endl;
                        index++;
                    }
                });

            return S_OK;
        }

        HRESULT CommandMonitor(std::wstring& format) {
            if (format != L"list" && format != L"csv") {
                std::wcerr << "error: invalid format: '" << format << "'" << std::endl;
                return E_INVALIDARG;
            }

            volatile BOOLEAN end = FALSE;
            MonitorRecordCallback callback = NULL;
            if (format == L"csv") {
                std::wcout << "major_irp,requestor_pid,requestor_tid,record_time,volume_serial_number,file_id,rule_major_type,rule_minor_type,rule_expression,file_path"
                           << std::endl;
                callback = [](FG_MONITOR_RECORD *record) {
                    FILETIME filetime;
                    SYSTEMTIME utc_systemtime, local_systemtime;
                    TIME_ZONE_INFORMATION timezone;
                    filetime.dwLowDateTime = record->RecordTime.LowPart;
                    filetime.dwHighDateTime = record->RecordTime.HighPart;
                    GetTimeZoneInformation(&timezone);
                    FileTimeToSystemTime(&filetime, &utc_systemtime);
                    SystemTimeToTzSpecificLocalTime(&timezone, &utc_systemtime, &local_systemtime);
                    std::wcout << MajorIRPName(record->MajorFunction) << L","
                               << record->RequestorPid << L","
                               << record->RequestorTid << L","
                               << SYSTEMTIME(local_systemtime) << L","
                               << record->FileIdDescriptor.VolumeSerialNumber << L","
                               << record->FileIdDescriptor.FileId.FileId64.QuadPart << L","
                               << RuleMajorName(record->RuleCode) << L","
                               << RuleMinorName(record->RuleCode) << L","
                               << std::wstring_view(record->Buffer, record->RulePathExpressionSize / sizeof(wchar_t)) << L","
                               << std::wstring_view(record->Buffer + record->RulePathExpressionSize / sizeof(wchar_t), record->FilePathSize / sizeof(wchar_t))
                               << std::endl;
                    };
            } else if (format == L"list") {
                callback = [](FG_MONITOR_RECORD *record) {
                    FILETIME filetime;
                    SYSTEMTIME utc_systemtime, local_systemtime;
                    TIME_ZONE_INFORMATION timezone;
                    filetime.dwLowDateTime = record->RecordTime.LowPart;
                    filetime.dwHighDateTime = record->RecordTime.HighPart;
                    GetTimeZoneInformation(&timezone);
                    FileTimeToSystemTime(&filetime, &utc_systemtime);
                    SystemTimeToTzSpecificLocalTime(&timezone, &utc_systemtime, &local_systemtime);
                    std::wcout << L"           major_irp: " << MajorIRPName(record->MajorFunction) << std::endl
                               << L"       requestor_pid: " << record->RequestorPid << std::endl
                               << L"       requestor_tid: " << record->RequestorTid << std::endl
                               << L"         record_time: " << SYSTEMTIME(local_systemtime) << std::endl
                               << L"volume_serial_number: " << record->FileIdDescriptor.VolumeSerialNumber << std::endl
                               << L"             file_id: " << record->FileIdDescriptor.FileId.FileId64.QuadPart << std::endl
                               << L"          rule_major: " << RuleMajorName(record->RuleCode) << std::endl
                               << L"          rule_minor: " << RuleMinorName(record->RuleCode) << std::endl
                               << L"     rule_expression: " << std::wstring_view(record->Buffer, record->RulePathExpressionSize/sizeof(wchar_t)) << std::endl
                               << L"           file_path: " << std::wstring_view(record->Buffer+record->RulePathExpressionSize/sizeof(wchar_t), record->FilePathSize/sizeof(wchar_t)) << std::endl
                               << std::endl;
                    };
            }

            auto result = core_client_->ReceiveMonitorRecords(&end, callback);
            if (result) return result.value();

            return S_OK;
        }

        HRESULT CommandCleanup() {
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
        std::wcerr << HEX(hr) << std::endl;
        return *hr;
    }
    return std::get<std::unique_ptr<fileguard::Admin>>(std::move(admin_opt))->Parse();
}