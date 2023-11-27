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
    Unicode string allocation/freeing routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocateUnicodeString(
    _In_ USHORT Size,
    _Out_ PUNICODE_STRING String
    )
/*++

Routine Description:

    This routine allocates a unicode string.

Arguments:

    String - A pointer to a variable that receives the unicode string.

    Size   - Supplies the size of the unicode string buffer to be allocated.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Buffer' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'Size' parameter is equal to zero.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    if (Size <= 0) return STATUS_INVALID_PARAMETER_1;
    if (NULL == String) return STATUS_INVALID_PARAMETER_2;

    String->Buffer = ExAllocatePool2(POOL_FLAG_PAGED, Size, FG_UNICODE_STRING_PAGED_MEM_TAG);
    String->Length = 0;
    String->MaximumLength = Size;

    return status;
}

VOID
FgFreeUnicodeString(
    _Inout_ PUNICODE_STRING String
    )
/*++

Routine Description:

    This routine frees a unicode string.

Arguments:

    String - Supplies the string to be freed.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FLT_ASSERT(NULL != String);
    FLT_ASSERT(NULL != String->Buffer);

    String->Length = 0;
    String->MaximumLength = 0;
    ExFreePoolWithTag(String->Buffer, FG_UNICODE_STRING_PAGED_MEM_TAG);
}

/*-------------------------------------------------------------
    Buffer allocation/freeing routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocateBuffer(
    _Inout_ PVOID      *Buffer,
    _In_    POOL_FLAGS PoolFlags,
    _In_    SIZE_T     Size
    )
/*++

Routine Description:

    This routine allocates a memory buffer.

Arguments:

    Buffer   - A pointer to a variable that receives the memory buffer.

    PoolType - Type of the pool to allocate memory from.

    Size     - Supplies the size of the buffer to be allocated.

Return Value:

    STATUS_SUCCESS                - Success.
    STATUS_INSUFFICIENT_RESOURCES - Failure. Unable to allocate memory.
    STATUS_INVALID_PARAMETER_1    - Failure. The 'Buffer' parameter is NULL.
    STATUS_INVALID_PARAMETER_2    - Failure. The 'Size' parameter is equal to zero.

--*/
{
    PAGED_CODE();

    if (NULL == Buffer) return STATUS_INVALID_PARAMETER_1;
    if (0    == Size)   return STATUS_INVALID_PARAMETER_2;

    *Buffer = ExAllocatePool2(PoolFlags, Size, FG_BUFFER_PAGED_MEM_TAG);

    return STATUS_SUCCESS;
}

VOID
FgFreeBuffer(
    _Inout_ PVOID Buffer
    )
/*++

Routine Description:

    This routine frees a memory buffer.

Arguments:

    Buffer - Supplies the buffer to be freed.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FLT_ASSERT(NULL != Buffer);

    ExFreePoolWithTag(Buffer, FG_BUFFER_PAGED_MEM_TAG);
}

/*-------------------------------------------------------------
    Push lock routines.
-------------------------------------------------------------*/

_Check_return_
NTSTATUS
FgAllocatePushLock(
    _Inout_ PEX_PUSH_LOCK* PushLock
    )
/*++

Routine Description:

    This routine allocates a new ERESOURCE from the non-paged pool and initializes it.

Arguments:

    Resource - Supplies pointer to the PERESOURCE to be created.

Return value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PEX_PUSH_LOCK pushLock = NULL;

    PAGED_CODE();

    if (NULL == PushLock) return STATUS_INVALID_PARAMETER_1;

    //
    // Allocate resource by non-paged memory.
    //
    pushLock = (PEX_PUSH_LOCK)ExAllocatePool2(POOL_FLAG_NON_PAGED,
                                              sizeof(ERESOURCE),
                                              FG_PUSHLOCK_NON_PAGED_MEM_TAG);
    if (NULL == pushLock)
        return STATUS_INSUFFICIENT_RESOURCES;

    //
    // Initialize resource.
    //
    FltInitializePushLock(pushLock);

    *PushLock = pushLock;

    return status;
}

VOID
FgFreePushLock(
    _Inout_ PEX_PUSH_LOCK PushLock
    )
/*++

Routine Description:

    This routine frees the PERESOURCE previously allocated by 'FgAllocateResource'.

Arguments:

    Resource - Supplies pointer to the ERESOURCE to be freed.
               If it's NULL, this function will do nothing.

Return value:

    None.

--*/
{ 
    PAGED_CODE();

    FLT_ASSERT(NULL != PushLock);

    FltDeletePushLock(PushLock);

    ExFreePoolWithTag((PVOID)PushLock, FG_PUSHLOCK_NON_PAGED_MEM_TAG);
}

/*-------------------------------------------------------------
    Exception routines.
-------------------------------------------------------------*/

LONG AsMessageException(
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