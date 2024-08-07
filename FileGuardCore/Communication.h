/*++

    The MIT License (MIT)

    Copyright (c) 2023 Fxtack

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Module Name:

    Communication.c

Abstract:

    Declarations of communication with user-mode application.

Environment:

    Kernel mode.

--*/

#pragma once

#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

/*-------------------------------------------------------------
    Core control communication declarations.
-------------------------------------------------------------*/

NTSTATUS
FgcControlPortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
    );

VOID
FgcControlPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
    );

NTSTATUS
FgcControlMessageNotifyCallback(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBytes) PVOID Input,
    _In_ ULONG InputBytes,
    _Out_writes_bytes_to_opt_(OutputBytes, *ReturnSize) PVOID Output,
    _In_ ULONG OutputBytes,
    _Out_ PULONG ReturnSize
    );

/*-------------------------------------------------------------
    Monitor communication declarations.
-------------------------------------------------------------*/

NTSTATUS
FgcMonitorPortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
    );

VOID
FgcMonitorPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FgcControlPortConnectCallback)
#pragma alloc_text(PAGE, FgcControlPortDisconnectCallback)
#pragma alloc_text(PAGE, FgcControlMessageNotifyCallback)
#pragma alloc_text(PAGE, FgcMonitorPortConnectCallback)
#pragma alloc_text(PAGE, FgcMonitorPortDisconnectCallback)
#endif

#endif