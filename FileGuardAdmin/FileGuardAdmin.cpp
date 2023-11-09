#include "FileGuardAdmin.h"

namespace fileguard {

	Admin::Admin() { /* Do nothing */ }

	Admin::~Admin() { /* Do nothing */ }

	void Admin::show_version() {

		FG_CORE_VERSION core_version;
		Client::get_core_version(core_version);

		printf("FileGuard Core: v%lu.%lu.%lu.%lu\nFileGuard Admin: v%lu.%lu.%lu.%lu\n",
			core_version.Major, core_version.Minor, core_version.Patch, core_version.Build,
			FG_ADMIN_MAJOR_VERSION, FG_ADMIN_MINOR_VERSION, FG_ADMIN_PATCH_VERSION, FG_ADMIN_BUILD_VERSION);
	}

	void Admin::show_usage() {

		printf(
			"FileGuardAdmin usage: \n"
			"  --help      Show usage information.\n"
			"  --version   Show FileGuard version information.\n"
			"Subcommand:   \n"
			"  rule        Rule manage commands.\n"
			"    --add     Add a rule.\n"
			"    --cleanup Cleanup all rule.\n"
		);
	}

	void Admin::add_rule() {}

	void Admin::cleanup_rule() {}
}