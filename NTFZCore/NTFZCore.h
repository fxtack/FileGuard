/*
    @File   NTFZCore.h
    @Note   NTFZCore module header file.

    @Mode   Kernel
    @Author Fxtack
*/

#define RTL_USE_AVL_TABLES 0

#ifndef _NTFZ_CORE_H_
#define _NTFZ_CORE_H_

#include <fltKernel.h>
#include <ntddk.h>
#include <dontuse.h>

#include "NTFZ.h"

#define NTFZ_CORE_VERSION_MAJOR 0
#define NTFZ_CORE_VERSION_MINOR 1
#define NTFZ_CORE_VERSION_PATCH 2

#define MEM_NPAGED_POOL_TAG_CONFIG_ENTRY 'fzcg'
#define MEM_NPAGED_POOL_TAG_SHARE_LOCK   'fzsl'

#define MAX_CONFIG_ENTRY_ALLOCATED 1024

// These routine is used by AVL tree (ConfigTable).
RTL_GENERIC_COMPARE_ROUTINE  configEntryCompareRoutine;  // Compare two config entry.
RTL_GENERIC_ALLOCATE_ROUTINE configEntryAllocateRoutine; // Allocate config entry.
RTL_GENERIC_FREE_ROUTINE     configEntryFreeRoutine;     // Free config entry.

/*************************************************************************
    Global variabls.
*************************************************************************/
typedef struct _NTFZ_CORE_GLOBALS {

    PFLT_FILTER Filter;                           // Filter instances.
    PFLT_PORT CorePort;                           // Communication port exported for NTFZAdmin.
    PFLT_PORT AdminPort;                          // Communication port that NTFZAdmin connecting to.

    ULONG ConfigEntryMaxAllocated;                // Maxinum of config entry can be allocated.
    __volatile ULONG ConfigEntryAllocated;        // Amount of allocated config entry.
    NPAGED_LOOKASIDE_LIST ConfigEntryFreeMemPool; // Memory pool of config entry.

    RTL_AVL_TABLE ConfigTable;                    // Config table.
    PERESOURCE ConfigTableShareLock;              // Config table share lock.

} NTFZ_CORE_GLOBALS, *PNTFZ_CORE_GLOBALS;

extern NTFZ_CORE_GLOBALS Globals;

/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
NTFZPreOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
NTFZPostOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
NTFZPreOperationNoPostOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
); 


/*************************************************************************
    Message handler routine.
*************************************************************************/
NTSTATUS NTFZCoreMessageHandlerRoutine(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
);

// Query a config from table by index.
NTSTATUS QueryConfigFromTable(
    _In_  PNTFZ_CONFIG QueryConfigEntry,
    _Out_ PNTFZ_CONFIG ResultConfigEntry
);

// Add a config to table.
NTSTATUS AddConfigToTable(
    _In_ PNTFZ_CONFIG InsertConfigEntry
);

// Find the config from table by index and remove it.
NTSTATUS RemoveConfigFromTable(
    _In_ PNTFZ_CONFIG RemoveConfigEntry
);

// Cleanup config table and release configs memory.
NTSTATUS CleanupConfigTable(
    VOID
);

#endif