/*
    @File   NtFreezerCore.h
    @Note   NtFreezerCore module header file.

    @Mode   Kernel
    @Author Fxtack
*/

#pragma once

#include <fltKernel.h>
#include <dontuse.h>

#include "NtFreezer.h"

#define NTFZ_CORE_VERSION_MAJOR 0
#define NTFZ_CORE_VERSION_MINOR 1
#define NTFZ_CORE_VERSION_PATCH 0

#define NPAGED_MEMPOOL_TAG_CONFIG_ENTRY 'fzcg'

#define MAX_CONFIG_ENTRY_ALLOCATED 1024

/*************************************************************************
    Global variabls.
*************************************************************************/
typedef struct _NTFZ_CORE_GLOBALS {
    PFLT_FILTER Filter;
    PFLT_PORT CorePort;
    PFLT_PORT AdminPort;
    ULONG ConfigEntryMaxAllocated;
    __volatile ULONG ConfigEntryAllocated;
    NPAGED_LOOKASIDE_LIST ConfigEntryFreeMemPool;
    KSPIN_LOCK ConfigTableLock;
} NTFZ_CORE_GLOBALS, *PNTFZ_CORE_GLOBALS;

extern NTFZ_CORE_GLOBALS Globals;

/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
NtFreezerPreOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);


FLT_POSTOP_CALLBACK_STATUS
NtFreezerPostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
NtFreezerPreOperationNoPostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
); 


/*************************************************************************
    Message handler routine.
*************************************************************************/
NTSTATUS NTFZCoreMessageHandlerRoutine(
    _In_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnBytes) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnBytes
);