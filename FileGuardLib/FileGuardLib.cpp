#include "FileGuardLib.h"

namespace fileguard {

	Client::Client() {
		_port = INVALID_HANDLE_VALUE;

		auto hr = FilterConnectCommunicationPort(FG_CORE_CONTROL_PORT_NAME, 0, NULL, 0, NULL, &_port);
		THROW_IF_ERROR(hr);

		FG_MESSAGE msg = { FG_MESSAGE_TYPE::GetCoreVersion };
		FG_MESSAGE_RESULT result = { 0, sizeof(FG_MESSAGE_RESULT)};
		DWORD returnedSize = 0ul;
		hr = FilterSendMessage(_port, &msg, sizeof(FG_MESSAGE), &result, result.ResultSize, &returnedSize);
		THROW_IF_ERROR(hr);

		std::memcpy(&_core_version, &result.CoreVersion, sizeof(FG_CORE_VERSION));
	}

	Client::~Client() {
		CloseHandle(_port);
	}

	void Client::get_core_version(FG_CORE_VERSION& core_version) {
		std::memcpy(&core_version, &_core_version, sizeof(FG_CORE_VERSION));
	}

	HRESULT Client::add_rule(FG_RULE_CLASS rule_class, const std::wstring& file_path_name) {
		return S_OK;
	}
	
	HRESULT Client::add_rule(const FG_RULE& rule) {
		return S_OK;
	}

	HRESULT Client::cleanup_rules(const std::wstring& volume_name) {
		return S_OK;
	}

	HRESULT Client::cleanup_rules() {
		return S_OK;
	}
}