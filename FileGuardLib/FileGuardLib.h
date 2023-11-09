#pragma once

#ifndef __FGLIB_H__
#define __FGLIB_H__

#include <iostream>
#include <cstring>

#include <windows.h>
#include <fltUser.h>

#include "FileGuard.h"

namespace fileguard {
	class Client {
	public:
		Client();
		~Client();
		void get_core_version(FG_CORE_VERSION& core_version);
		HRESULT add_rule(FG_RULE_CLASS rule_class, const std::wstring& file_path_name);
		HRESULT add_rule(const FG_RULE& rule);
		HRESULT cleanup_rules(const std::wstring& volume_name);
		HRESULT cleanup_rules();

	private:
		HANDLE _port;
		FG_CORE_VERSION _core_version;
	};
}

#define THROW_IF_ERROR(_hresult_) if (IS_ERROR(_hresult_)) { throw std::system_error(hr, std::system_category()); };

//#ifdef FILEGUARDLIB_EXPORTS
//#define FGLIB_API __declspec(dllexport)
//#else
//#define FGLIB_API __declspec(dllimport)
//#endif
//
//extern "C" FGLIB_API HRESULT FglGetVersion(FG_CORE_VERSION& core_version);
//extern "C" FGLIB_API HRESULT FglAddRule(const FG_RULE& rule);
//extern "C" FGLIB_API HRESULT FglCleanupRules();

#endif