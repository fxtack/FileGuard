/*
	@File	NtFreezer.h
	@Note	Public include.

	@Mode	Kernel and User
	@Author Fxtack
*/

#ifndef _NTFZ_H_
#define _NTFZ_H_

#define MAX_PATH 260

#define NTFZ_COMMAND_PORT_NAME L"\\NTFZCommandPort"
#define NTFZ_LOG_PORT_NAME     L"\\NTFZLogPort"

// The type of message that sending from admin to core.
typedef enum _NTFZ_COMMAND_TYPE {

	// Get core version.
	GetCoreVersion,

	// Query a configuration.
	QueryConfig,

	// Add a configuration.
	AddConfig,
	
	// Find and remove a configuration.
	RemoveConfig,

	// Cleanup all configurations.
	CleanupConfig

} NTFZ_COMMAND_TYPE;


// The message sending from admin to core.
typedef struct _NTFZ_COMMAND {

	// Message type.
	NTFZ_COMMAND_TYPE MsgType;

	// Message metadata.
	_Field_size_opt_(MetadataBytes)
	PVOID Metadata;
	ULONG MetadataBytes;

	// Message data is a pointer to a request structure instance typically.
	_Field_size_opt_(DataBytes)
	PVOID Data;
	ULONG DataBytes;

} NTFZ_COMMAND, *PNTFZ_COMMAND;


// NtFreezer core version.
typedef struct _NTFZ_CORE_VERSION {
	ULONG Major;
	ULONG Minor;
	ULONG Patch;
} NTFZ_CORE_VERSION, *PNTFZ_CORE_VERSION,
  RESPONSE_GET_VERSION, *PREPONSE_GET_VERSION;


// Freeze type, detail for wiki.
typedef enum _NTFZ_CONFIG_TYPE {
	FzTypeUndefined     = -1,
	FzTypeNothing       = 0,
	FzTypeAccessDenied  = 1 << 1,
	FzTypeHide          = 1 << 2,
	FzTypeStaticReparse = 1 << 3
} NTFZ_CONFIG_TYPE;

#define _VALID_FZ_TYPE_ FzTypeAccessDenied |\
                        FzTypeHide |\
                        FzTypeStaticReparse
#define VALID_FZ_TYPE(_T_) ((_T_) & (_VALID_FZ_TYPE_))


// NtFreezer configuration.
typedef struct _NTFZ_CONFIG {
	NTFZ_CONFIG_TYPE FreezeType;
	WCHAR Path[MAX_PATH + 1];
} NTFZ_CONFIG, * PNTFZ_CONFIG;

typedef NTFZ_CONFIG REQUEST_QUERY_CONFIG, * QREQUEST_QUERY_CONFIG;
typedef NTFZ_CONFIG RESPONSE_QUERY_CONFIG, * QRESPONSE_QUERY_CONFIG;
typedef NTFZ_CONFIG REQUEST_ADD_CONFIG, * QREQUEST_ADD_CONFIG;
typedef NTFZ_CONFIG REQUEST_REMOVE_CONFIG, *QREQUEST_REMOVE_CONFIG;

#endif