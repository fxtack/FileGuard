/*
	@File	NtFreezer.h
	@Note	Public include.

	@Mode	Kernel and User
	@Author Fxtack
*/

#ifndef _NTFZ_H_
#define _NTFZ_H_

#define MAX_PATH 260

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

// Filesystem item, file or directory.
#define FS_ITEM_FILE      0x00000001
#define FS_ITEM_DIRECTORY 0x00000002
#define _VALID_FS_ITEM_ FS_ITEM_FILE |\
						FS_ITEM_DIRECTORY
#define VALID_FS_ITEM(_T_) (_T_ & (_VALID_FS_ITEM_))

// Freeze type, detail for wiki.
#define FZ_TYPE_HIDE        ((ULONG) (1 << 1)) 
#define FZ_TYPE_READONLY    ((ULONG) (1 << 2)) 
#define FZ_TYPE_EMPTY_DIR   ((ULONG) (1 << 3))
#define FZ_TYPE_REPARSE_DIR ((ULONG) (1 << 4))
#define _VALID_FZ_TYPE_ FZ_TYPE_HIDE |\
						FZ_TYPE_READONLY |\
						FZ_TYPE_EMPTY_DIR |\
						FZ_TYPE_REPARSE_DIR
#define VALID_FZ_TYPE(_T_) (_T_ & (_VALID_FZ_TYPE_))

// NtFreezer configuration.
typedef struct _NTFZ_CONFIG {
	ULONG FsItemFile;
	ULONG FreezeType;
	WCHAR Path[MAX_PATH + 1];
} NTFZ_CONFIG, *PNTFZ_CONFIG;

#endif