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

    Utilities.h

Abstract:

    Declarations of utility routines are contained.

Environment:

    Kernel mode.

--*/

#pragma once

#ifndef __UTILITIES_H__
#define __UTILITIES_H__

/*-------------------------------------------------------------
    Logger macro.
-------------------------------------------------------------*/

//
// Log level.
//
#define LOG_LEVEL_TRACE   ((ULONG)0x01)
#define LOG_LEVEL_INFO    ((ULONG)0x02)
#define LOG_LEVEL_WARNING ((ULONG)0x04)
#define LOG_LEVEL_ERROR   ((ULONG)0x08)
#define LOG_LEVEL_DEFAULT (LOG_LEVEL_WARNING | LOG_LEVEL_ERROR)

#define __FILENAME__ (strchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define LOG(_type_, _format_, ...) DbgPrint("[" _type_ "] FileGuardCore!%s::%s@%d: " _format_ ".\n", \
                                       __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define LOG_TRACE(_format_, ...)   if (FlagOn(Globals.LogLevel, LOG_LEVEL_TRACE)) \
                                       LOG("TRACE", _format_, __VA_ARGS__)

#define LOG_INFO(_format_, ...)    if (FlagOn(Globals.LogLevel, LOG_LEVEL_INFO)) \
                                       LOG("INFO", _format_, __VA_ARGS__)

#define LOG_WARNING(_format_, ...) if (FlagOn(Globals.LogLevel, LOG_LEVEL_WARNING)) \
                                       LOG("WARNING", _format_, __VA_ARGS__)
                                   
#define LOG_ERROR(_format_, ...)   if (FlagOn(Globals.LogLevel, LOG_LEVEL_ERROR)) \
                                       LOG("ERROR", _format_, __VA_ARGS__)

#ifdef DBG

//
// Debug log.
//
#define DBG_TRACE(_format_, ...)   LOG_TRACE(_format_, __VA_ARGS__)
#define DBG_INFO(_format_, ...)    LOG_INFO(_format_, __VA_ARGS__)
#define DBG_WARNING(_format_, ...) LOG_WARNING(_format_, __VA_ARGS__)                                   
#define DBG_ERROR(_format_, ...)   LOG_ERROR(_format_, __VA_ARGS__)

#else

#define DBG_TRACE(_format_, ...)
#define DBG_INFO(_format_, ...)
#define DBG_WARNING(_format_, ...)
#define DBG_ERROR(_format_, ...)

#endif

/*-------------------------------------------------------------
    Unicode string allocation/freeing routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocateUnicodeString(
    _Inout_ PUNICODE_STRING String,
    _In_    USHORT Size
);

VOID
FgFreeUnicodeString(
    _Inout_ PUNICODE_STRING String
);

/*-------------------------------------------------------------
    Buffer allocation/freeing routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocateBuffer(
    _Inout_ PVOID      *Buffer,
    _In_    POOL_FLAGS PoolFlags,
    _In_    SIZE_T     Size
);

VOID
FgFreeBuffer(
    _Inout_ PVOID Buffer
);

/*-------------------------------------------------------------
    Push lock routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocatePushLock(
    _Inout_ PEX_PUSH_LOCK *PushLock
);

VOID
FgFreePushLock(
    _Inout_ PEX_PUSH_LOCK PushLock
);

/*-------------------------------------------------------------
    Other tool routines.
-------------------------------------------------------------*/

#define LIST_FOR_EACH_SAFE(curr, n, head) \
        for (curr = (head)->Flink, n = curr->Flink; curr != (head); \
             curr = n, n = curr->Flink)

#define InterlockedExchangeBoolean(_bool_, _val_) InterlockedExchange8((__volatile char*)(_bool_), _val_)

/*-------------------------------------------------------------
    Exception routines.
-------------------------------------------------------------*/

LONG AsMessageException(
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
);

#endif