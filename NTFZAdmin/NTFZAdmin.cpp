/*
    @File   NTFZAdmin.cpp
    @Note   Admin function.

    @Mode   User
    @Author Fxtack  
*/

#include <windows.h>
#include <Fileapi.h>
#include <fltUser.h>

#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#include "NTFZ.h"
#include "NTFZAdmin.h"

inline NTFZ_CONFIG_TYPE FzConfigTypeCode(
    _In_ std::wstring FzConfig
) {
    std::transform(
        FzConfig.begin(),
        FzConfig.end(),
        FzConfig.begin(),
        toupper);
    if (FzConfig == L"ACCESS_DENIED") {
        return FzTypeAccessDenied;
    } else if (FzConfig == L"NOT_FOUND") {
        return FzTypeNotFound;
    } else if (FzConfig == L"STATIC_REPARSE") {
        return FzTypeStaticReparse;
    } else {
        return FzTypeUndefined;
    }
}

// Get path device names.
// e.g. C:\dir\file -> \Device\HarddiskVolume8
inline std::wstring PathDeviceName(
    _In_ std::wstring& path
) {
    WCHAR deviceName[1024];
    DWORD resultCode;
    auto pos = path.find_first_of(L"\\");
    if (pos == std::wstring::npos) {
        return L"";
    }
    resultCode = QueryDosDeviceW(
        path.substr(0, pos).c_str(),
        deviceName,
        1024
    );
    if (resultCode == 0) {
        return std::wstring();
    }
    return std::wstring(deviceName);
}

// Convert symbolic link to device names.
// e.g. C:\dir\file -> \Device\HarddiskVolume8\dir\file
inline std::wstring DevicePath(
    _In_ std::wstring path
) {
    auto deviceName = PathDeviceName(path);
    if (deviceName.empty()) {
        return deviceName;
    }

    return deviceName + path.substr(path.find_first_of(L"\\"));
}

namespace ntfz {
    using namespace std;
    // Admin.
    Admin::Admin(
        _In_ PCWSTR PortName
    ) {
        if (wcslen(PortName) == 0)
            throw AdminError("Invalid communication port.");

        _port_= INVALID_HANDLE_VALUE;

        // Initialize NTFZCore communication port.
        auto hResult = FilterConnectCommunicationPort(PortName,
                                                      0,
                                                      NULL,
                                                      0,
                                                      NULL,
                                                      &_port_);
        if (IS_ERROR(hResult))
            throw AdminError(hResult, "Connect to core failed, ensure that the core driver is loaded.");    

        NTFZ_COMMAND msg;
        RESPONSE_GET_VERSION respVersion = { 0 };
        DWORD returnBytes;
        msg.MsgType = GetCoreVersion;
        
        hResult = FilterSendMessage(_port_,
                                    &msg, sizeof(NTFZ_COMMAND),
                                    &respVersion, sizeof(RESPONSE_GET_VERSION),
                                    &returnBytes);
        if (IS_ERROR(hResult) || returnBytes != sizeof(RESPONSE_GET_VERSION))
            throw AdminError(hResult, "Get core version failed, admin and core version may not match.");       

        // Different major versions of Admin and Core are incompatible with each other.
        if (respVersion.Major != NTFZ_ADMIN_VERSION_MAJOR)
            throw AdminError("Version mismatch, please select an admin and core version that can match");      

        // If the Admin minor version higher than Core minor version is incompatible.
        if (respVersion.Minor < NTFZ_ADMIN_VERSION_MINOR)
            throw AdminError("ERROR: Admin version too high, please select an admin and core that can match");

        _coreVersion_ = respVersion;
    }

    Admin::~Admin() {
        CloseHandle(_port_);
    }

    template <typename... Args>
    void Admin::LoadCore(
        _In_ const wstring& arg, Args... DevicesSymbolicLink
    ) {
        throw "Implement me";
    }

    std::unique_ptr<NTFZ_CONFIG> Admin::TellCoreQueryConfig(
        _In_ std::wstring Path
    ) {
        REQUEST_QUERY_CONFIG request;
        memcpy(request.Path, Path.c_str(), Path.length() * sizeof(WCHAR));

        NTFZ_COMMAND msg;
        msg.MsgType = QueryConfig;
        msg.Data = (PVOID)&request;
        msg.DataBytes = sizeof(request);

        NTFZ_CONFIG resp;
        DWORD returneBytes;
        auto hResult = FilterSendMessage(_port_,
                                         &msg, sizeof(NTFZ_COMMAND),
                                         &resp, sizeof(NTFZ_CONFIG),
                                         &returneBytes);
        if (IS_ERROR(hResult))
            throw AdminError(hResult, "Query a config failed.");

        return std::move(std::make_unique<NTFZ_CONFIG>(resp));
    }

    // Send a message to the core to add a configuration.
    void Admin::TellCoreAddConfig(
        _In_ wstring ConfigType,
        _In_ wstring Path
    ) {
        REQUEST_ADD_CONFIG request;
        request.FreezeType = FzConfigTypeCode(ConfigType);
        memset(request.Path, '\0', MAX_PATH+1);
        memcpy(request.Path, Path.c_str(), Path.length() * sizeof(WCHAR));

        NTFZ_COMMAND msg;
        msg.MsgType = AddConfig;
        msg.Data = (PVOID)&request;
        msg.DataBytes = sizeof(REQUEST_ADD_CONFIG);

        DWORD returnBytes;
        auto hResult = FilterSendMessage(_port_,
                                         &msg,
                                         sizeof(NTFZ_COMMAND),
                                         NULL,
                                         0,
                                         &returnBytes);
        if (IS_ERROR(hResult))
            throw AdminError(hResult, "Add a config failed.");
    }

    void Admin::TellCoreAddConfig(
        _In_ std::wstring Path
    ) {
        this->TellCoreAddConfig(L"ACCESS_DENIED", Path);
    }

    // Send a message to the core to find and delete a configuration.
    void Admin::TellCoreRemoveConfig(
        _In_ wstring Path
    ) {
        REQUEST_REMOVE_CONFIG request;
        memcpy(request.Path, Path.c_str(), Path.length() * sizeof(WCHAR));

        NTFZ_COMMAND msg;
        msg.MsgType = RemoveConfig;
        msg.Data = (PVOID)&request;
        msg.DataBytes = sizeof(request);

        DWORD returnBytes;
        auto hResult = FilterSendMessage(_port_,
                                         &msg, sizeof(NTFZ_COMMAND),
                                         NULL,
                                         0,
                                         &returnBytes);
        if (IS_ERROR(hResult))
            throw AdminError(hResult, "Remove a config failed.");
    }

    // Send a message to the core to clean up all configurations.
    void Admin::TellCoreCleanupConfigs() {
        NTFZ_COMMAND msg;
        msg.MsgType = CleanupConfig;
        
        DWORD returnBytes;
        auto hResult = FilterSendMessage(_port_,
                                         &msg, sizeof(NTFZ_COMMAND),
                                         NULL,
                                         0,
                                         &returnBytes);
        if (IS_ERROR(hResult))
            throw AdminError(hResult, "Clean up all configs failed.");
    }

    // Print version information of core and admin.
    void Admin::PrintVersion() {
        printf(
            "NTFZAdmin: v%lu.%lu.%lu\n"
            "NTFZCore:  v%lu.%lu.%lu",
            NTFZ_ADMIN_VERSION_MAJOR, NTFZ_ADMIN_VERSION_MINOR, NTFZ_ADMIN_VERSION_PATCH,
            _coreVersion_.Major, _coreVersion_.Minor, _coreVersion_.Patch
        );
    }

    // Admin Error.
    AdminError::AdminError(
        _In_ const HRESULT HResult,
        _In_ const string& Msg
    ):runtime_error(Msg) {
        _hResult_ = HResult;
        _msg_ = Msg;
    }

    AdminError::AdminError(
        _In_ const string& Msg
    ):runtime_error(Msg) {
        _hResult_ = 0;
        _msg_ = Msg;
    }

    AdminError::~AdminError() noexcept {}

    const char* AdminError::what() const noexcept {
        stringstream strstream;
        if (_hResult_ == 0) {
            strstream << "Error: " << _msg_;
        } else {
            strstream << "Error(" << setw(8) << hex << _hResult_ << "): " << _msg_;
        }
        return strstream.str().c_str();
    }

    ostream& operator<<(ostream& stream, const AdminError& error) {
        if (error._hResult_ == 0) {
            stream << "Error: " << error._msg_;
        } else {
            stream << "Error(" << setw(8) << hex << error._hResult_ << "): " << error._msg_;
        }
        return stream;
    }

    HRESULT AdminError::GetHResult() {
        return _hResult_;
    }
}