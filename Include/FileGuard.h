/*
    @File	FileGuard.h
    @Note	Public include.

    @Mode	Kernel and User
    @Author Fxtack
*/

#ifndef __FILE_GUARD_H__
#define __FILE_GUARD_H__

#define MAX_PATH 260

#define FG_MAX_PATH_LENGTH 261

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

//
// FileGuard core version.
//
typedef struct _FG_CORE_VERSION {
    ULONG Major;
    ULONG Minor;
    ULONG Patch;
} FG_CORE_VERSION, * PFG_CORE_VERSION;

typedef struct _FG_FILE_ID_DESCRIPTOR {
    
    //
    // The GUID name of volume that file belong to.
    //
    WCHAR VolumeGuidName;
    
    //
    // The ID of the file on the file system volume.
    //
    union {

        //
        // NTFS support 64 bits file ID.
        //
        LARGE_INTEGER FileId64;

        //
        // Other file systems may support 128 bits file ID.
        //
        FILE_ID_128 FileId128;  

    } FileId;

} FG_FILE_ID_DESCRIPTOR, *PFG_FILE_ID_DESCRIPTOR;

typedef ULONG FG_RULE_CLASS ;
typedef ULONG* PFG_RULE_CLASS;

#define RULE_CONSTRAINT_MASK ((ULONG)0x00ff)
#define RULE_UNKNOWN         ((ULONG)0x0001)
#define RULE_ACCESS_DENIED   ((ULONG)0x0002)
#define RULE_READONLY        ((ULONG)0x0003)

#define RULE_CONSTRAINT(_rule_, _constraint_) ((_rule_) & RULE_CONSTRAINT_MASK)

#define RULE_MATCH_MASK               ((ULONG)0xff00)
#define RULE_MATCH_FILE_ID_DESCRIPTOR ((ULONG)0x0100)
#define RULE_MATCH_FILE_NAME          ((ULONG)0x0200)

#define RULE_MATCH(_rule_, _match_) ((_rule_) & RULE_MATCH_MASK)

typedef struct _FG_RULE {

    //
    // FileGuard rule class.
    //
    FG_RULE_CLASS Class;

    //
    // File ID descriptor.
    //
    FG_FILE_ID_DESCRIPTOR FileIdDescriptor;

    //
    // File path name.
    //
    WCHAR FilePathName[FG_MAX_PATH_LENGTH];

} FG_RULE, *PFG_RULE;

typedef struct _FG_MONITOR_RECORD {

    //
    // Class of triggering rule.
    //
    FG_RULE_CLASS Class;

    //
    // File ID descriptor.
    //
    FG_FILE_ID_DESCRIPTOR FileIdDescriptor;

    //
    // File device path name.
    //
    WCHAR FilePathName[FG_MAX_PATH_LENGTH];

} FG_MONITOR_RECORD, *PFG_MONITOR_RECORD;

#define MONITOR_RECORDS_MESSAGE_BODY_BUFFER_SIZE (32 * 1024)

typedef struct _FG_RECORD_MESSAGE_BODY {

    //
    // Bytes size of data.
    //
    ULONG DataSize;

    //
    // Data buffer.
    //
    UCHAR DataBuffer[MONITOR_RECORDS_MESSAGE_BODY_BUFFER_SIZE];

} FG_RECORDS_MESSAGE_BODY, *PFG_RECORDS_MESSAGE_BODY;

typedef struct _FG_MONITOR_RECORDS_MESSAGE {

    //
    // Message header.
    //
    FILTER_MESSAGE_HEADER	MessageHeader;

    //
    // Message body.
    //
    FG_RECORDS_MESSAGE_BODY	MessageBody;

} FG_MONITOR_RECORDS_MESSAGE, *PFG_MONITOR_RECORDS_MESSAGE;


#endif