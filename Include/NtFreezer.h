/*
	@File	NtFreezer.h
	@Note	Public include.

	@Mode	Kernel and User
	@Author Fxtack
*/

#define NTFZ_PORT_NAME L"\\NtFreezerPort"

#define NTFZ_A2CMSG_SIZE          sizeof(NTFZ_A2CMSG)
#define NTFZ_CORE_VERSION_SIZE    sizeof(NTFZ_CORE_VERSION)
#define RESPONSE_GET_VERSION_SIZE sizeof(RESPONSE_GET_VERSION)

// The type of message that sending from admin to core.
typedef enum _NTFZ_A2CMSG_TYPE {

	// Get core version.
	GetCoreVersion,

	// Add a config.
	AddConfig,
	
	// Find and remove a config.
	RemoveConfig,

	// Cleanup all configs.
	CleanupConfig

} NTFZ_A2CMSG_TYPE;


// The message sending from admin to core.
typedef struct _NTFZ_A2CMSG {

	// Message type.
	NTFZ_A2CMSG_TYPE MsgType;

	// Message metadata.
	_Field_size_opt_(MetadataBytes)
	PVOID Metadata;
	ULONG MetadataBytes;

	// Message data is a pointer to a request structure instance typically.
	_Field_size_opt_(DataBytes) 
	PVOID Data;
	ULONG DataBytes;

} NTFZ_A2CMSG, *PNTFZ_A2CMSG;


// NtFreezer core version.
typedef struct _NTFZ_CORE_VERSION {
	ULONG Major;
	ULONG Minor;
	ULONG Patch;
} NTFZ_CORE_VERSION, *PNTFZ_CORE_VERSION,
  RESPONSE_GET_VERSION, *PREPONSE_GET_VERSION;