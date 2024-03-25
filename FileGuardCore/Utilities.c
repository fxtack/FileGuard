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

    Utilities.c

Abstract:

    Definitions of utility routines are contained.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Utilities.h"

/*-------------------------------------------------------------
    Buffer allocation/freeing routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocateBufferEx(
    _Inout_ PVOID* Buffer,
    _In_ POOL_FLAGS Flags,
    _In_ SIZE_T Size,
    _In_ ULONG Tag
)
/*++

Routine Description:

    This routine allocates a memory buffer.

Arguments:

    Buffer - A pointer to a variable that receives the memory buffer.
    Flags  - Type of the pool to allocate memory from.
    Size   - Supplies the size of the buffer to be allocated.
    Tag    - Memory pool tag.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Buffer' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'Size' parameter is equal to zero.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID buffer = NULL;

    if (NULL == Buffer) return STATUS_INVALID_PARAMETER_1;
    if (0 == Size) return STATUS_INVALID_PARAMETER_2;

    buffer = ExAllocatePool2(Flags, Size, Tag);
    if (NULL == buffer) status = STATUS_INSUFFICIENT_RESOURCES;

    return status;
}

/*-------------------------------------------------------------
    Unicode string allocation/freeing routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocateUnicodeString(
    _In_ USHORT Size,
    _Out_ PUNICODE_STRING* String
    )
/*++

Routine Description:

    This routine allocates a unicode string.

Arguments:

    Size   - Supplies the size of the unicode string buffer to be allocated.
    String - A pointer to a variable that receives the unicode string.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Buffer' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'Size' parameter is equal to zero.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PUNICODE_STRING stringBuffer = NULL;

    if (Size <= 0) return STATUS_INVALID_PARAMETER_1;
    if (NULL == String) return STATUS_INVALID_PARAMETER_2;

    status = FgAllocateBufferEx(&stringBuffer,
                                POOL_FLAG_NON_PAGED, 
                                sizeof(UNICODE_STRING) + Size,
                                FG_UNICODE_STRING_NON_PAGED_TAG);
    if (!NT_SUCCESS(status)) return status;
    
    stringBuffer->Length = 0;
    stringBuffer->MaximumLength = Size;
    stringBuffer->Buffer = Add2Ptr(stringBuffer, sizeof(UNICODE_STRING));

    *String = stringBuffer;

    return status;
}

/*-------------------------------------------------------------
    Push lock routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgCreatePushLock(
    _Inout_ PEX_PUSH_LOCK* Lock
    )
/*++

Routine Description:

    This routine allocates a new PEX_PUSH_LOCK from the non-paged pool and initializes it.

Arguments:

    Lock - Supplies pointer to the PEX_PUSH_LOCK to be created.

Return value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PEX_PUSH_LOCK lock = NULL;

    if (NULL == Lock) return STATUS_INVALID_PARAMETER_1;

    //
    // Allocate resource by non-paged memory.
    //
    status = FgAllocateBufferEx(&lock,
                                POOL_FLAG_NON_PAGED,
                                sizeof(EX_PUSH_LOCK),
                                FG_PUSHLOCK_NON_PAGED_TAG);
    if (!NT_SUCCESS(status)) return status;

    //
    // Initialize resource.
    //
    FltInitializePushLock(lock);

    *Lock = lock;

    return status;
}

/*-------------------------------------------------------------
    Exception routines.
-------------------------------------------------------------*/

LONG 
AsMessageException(
    _In_ PEXCEPTION_POINTERS ExceptionPointer,
    _In_ BOOLEAN AccessingUserBuffer
    )
/*++

Routine Description:

    Exception filter to catch errors touching user buffers.

Arguments:

    ExceptionPointer    - The exception record.
    AccessingUserBuffer - If TRUE, overrides FsRtlIsNtStatusExpected to allow
                          the caller to munge the error to a desired status.

Return Value:

    EXCEPTION_EXECUTE_HANDLER - If the exception handler should be run.
    EXCEPTION_CONTINUE_SEARCH - If a higher exception handler should take care of
                                this exception.

--*/
{
    NTSTATUS status = ExceptionPointer->ExceptionRecord->ExceptionCode;

    if (!FsRtlIsNtstatusExpected(status) && !AccessingUserBuffer) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}