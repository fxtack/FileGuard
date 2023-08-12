/*
	@File	Cannot.h
	@Note	Public include.

	@Mode	Kernel and User
	@Author Fxtack
*/

#ifndef _CANNOT_H_
#define _CANNOT_H_

#define MAX_PATH 260

#define CANNOT_COMMAND_PORT_NAME L"\\CannotCommandPort"
#define CANNOT_LOG_PORT_NAME     L"\\CannotLogPort"

// The type of message that sending from admin to core.
typedef enum _CANNOT_COMMAND_TYPE {

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

} CANNOT_COMMAND_TYPE;


// The message sending from admin to core.
typedef struct _CANNOT_COMMAND {

	// Message type.
	CANNOT_COMMAND_TYPE MsgType;

	// Message metadata.
	_Field_size_opt_(MetadataBytes)
	PVOID Metadata;
	ULONG MetadataBytes;

	// Message data is a pointer to a request structure instance typically.
	_Field_size_opt_(DataBytes)
	PVOID Data;
	ULONG DataBytes;

} CANNOT_COMMAND, *PCANNOT_COMMAND;


// Cannot core version.
typedef struct _CANNOT_CORE_VERSION {
	ULONG Major;
	ULONG Minor;
	ULONG Patch;
} CANNOT_CORE_VERSION, *PCANNOT_CORE_VERSION,
  RESPONSE_GET_VERSION, *PREPONSE_GET_VERSION;


// Freeze type, detail for wiki.
typedef enum _CANNOT_CONFIG_TYPE {
	CannotTypeNothing,
	CannotTypeReadOnly,
	CannotTypeAccessDenied,
	CannotTypeRedirect,
	CannotTypeMaximum
} CANNOT_CONFIG_TYPE;

#define VALID_CANNOT_TYPE(_CANNOT_TYPE_) ((_CANNOT_TYPE_) >= CannotTypeNothing && (_CANNOT_TYPE_ < CannotTypeMaximum))


// Cannot configuration.
typedef struct _CANNOT_CONFIG {
	CANNOT_CONFIG_TYPE CannotType;
	WCHAR Path[MAX_PATH + 1];
} CANNOT_CONFIG, * PCANNOT_CONFIG;

typedef CANNOT_CONFIG REQUEST_QUERY_CONFIG, * QREQUEST_QUERY_CONFIG;
typedef CANNOT_CONFIG RESPONSE_QUERY_CONFIG, * QRESPONSE_QUERY_CONFIG;
typedef CANNOT_CONFIG REQUEST_ADD_CONFIG, * QREQUEST_ADD_CONFIG;
typedef CANNOT_CONFIG REQUEST_REMOVE_CONFIG, *QREQUEST_REMOVE_CONFIG;

#endif