#pragma once

#ifndef __FG_ADMIN_H__
#define __FG_ADMIN_H__

#include "FileGuardLib.h"

#define FG_ADMIN_MAJOR_VERSION 0
#define FG_ADMIN_MINOR_VERSION 0
#define FG_ADMIN_PATCH_VERSION 0
#define FG_ADMIN_BUILD_VERSION 0

namespace fileguard {

	class Admin: public Client {
	public:
		Admin();
		~Admin();

	private:
		void show_version();
		void show_usage();
		void add_rule();
		void cleanup_rule();
	};
}

#endif