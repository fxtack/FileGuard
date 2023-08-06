/*
    @File   CannotAdmin.h
    @Note   CannotAdmin header file.

    @Mode   User
    @Author Fxtack
*/

#ifndef _CANNOT_ADMIN_H_
#define _CANNOT_ADMIN_H_

#define CANNOT_ADMIN_VERSION_MAJOR 0
#define CANNOT_ADMIN_VERSION_MINOR 1
#define CANNOT_ADMIN_VERSION_PATCH 0

namespace cannot {
    class Admin {
    public:
        Admin(
            _In_ PCWSTR PortName
        );

        ~Admin();

        // Load the core and attach it to a device
        template <typename... Args>
        void LoadCore(
            _In_ const std::wstring& arg, Args... DevicesSymbolicLink
        );

        // Send message to the core to query a configuration.
        std::unique_ptr<CANNOT_CONFIG> TellCoreQueryConfig(
            _In_ std::wstring Path
        );

        // Send a message to the core to add a configuration.
        void TellCoreAddConfig(
            _In_ std::wstring ConfigType,
            _In_ std::wstring Path
        );
        void TellCoreAddConfig(
            _In_ std::wstring Path
        );

        // Send a message to the core to find and delete a configuration.
        void TellCoreRemoveConfig(
            _In_ std::wstring Path
        );

        // Send a message to the core to clean up all configurations.
        void TellCoreCleanupConfigs();

        // Print version information of core and admin.
        void PrintVersion();

    private:
        HANDLE _port_;
        CANNOT_CORE_VERSION _coreVersion_;
    };

    class AdminError : public std::runtime_error {
    public:
        explicit AdminError(
            _In_ const HRESULT HResult,
            _In_ const std::string& Msg
        );
        explicit AdminError(
            _In_ const std::string& Msg
        );
        virtual ~AdminError() noexcept override;
        virtual const char* what() const noexcept override;

        friend std::ostream& operator<<(std::ostream& stream, const AdminError& error);

        HRESULT GetHResult();
    private:
        HRESULT _hResult_;  // HResult code.
        std::string _msg_;  // Error message.
    };
}

#endif