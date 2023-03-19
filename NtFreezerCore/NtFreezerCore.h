/*
    @File   NtFreezerCore.h
    @Note   NtFreezerCore module header file.

    @Mode   Kernel
    @Author Fxtack
*/

#pragma once

#include <fltKernel.h>
#include <dontuse.h>


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