/*
	@File	FileGuard.h
	@Note	Public include.

	@Mode	Kernel and User
	@Author Fxtack
*/

#ifndef __FILE_GUARD_H__
#define __FILE_GUARD_H__

#define MAX_PATH 260

#define FG_CORE_CONTROL_PORT_NAME L"\\FileGuardControlPort"
#define FG_MONITOR_PORT_NAME      L"\\FileGuardMonitorPort"

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

typedef enum _FG_RULE_CLASS {
	RuleUnknown,
	RuleAccessDenied,
	RuleReadOnly,
	RuleMaximum
} FG_RULE_CLASS, *PFG_RULE_CLASS;

typedef struct _FG_RULE {
	FG_RULE_CLASS Class;

} FG_RULE, *PFG_RULE;

#define FG_RECORDS_MESSAGE_BODY_BUFFER_SIZE (32 * 1024)

typedef struct _FG_RECORD_BODY {
	ULONG DataSize;
	UCHAR DataBuffer[FG_RECORDS_MESSAGE_BODY_BUFFER_SIZE];
} FG_RECORDS_MESSAGE_BODY, *PFG_RECORDS_MESSAGE_BODY;

typedef struct _FG_RECORDS_MESSAGE {
	FILTER_MESSAGE_HEADER	MessageHeader;
	FG_RECORDS_MESSAGE_BODY	MessageBody;
} FG_RECORDS_MESSAGE, *PFG_RECORDS_MESSAGE;


#endif