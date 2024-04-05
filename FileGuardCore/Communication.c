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

    FileGuardCore.c

Abstract:

    Definition of communication with user-mode application.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Communication.h"

/*-------------------------------------------------------------
    Core control communication routines.
-------------------------------------------------------------*/

NTSTATUS FgCoreControlPortConnectCallback(
    _In_ PFLT_PORT AdminPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
    )
/*++

Routine Description

    Communication connection callback routine.
    This is called when user-mode connects to the server port.

Arguments

    ClientPort        - This is the client connection port that will be used to send messages from the filter

    ServerPortCookie  - Unused

    ConnectionContext - The connection context passed from the user. This is to recognize which type
                        connection the user is trying to connect.

    SizeofContext     - The size of the connection context.

    ConnectionCookie  - Propagation of the connection context to disconnection callback.

Return Value

    STATUS_SUCCESS                - to accept the connection
    STATUS_INSUFFICIENT_RESOURCES - if memory is not enough
    STATUS_INVALID_PARAMETER_3    - Connection context is not valid.

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(AdminPort);
    UNREFERENCED_PARAMETER(CorePortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ContextBytes);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    FLT_ASSERT(Globals.ControlClientPort == NULL);

    Globals.ControlClientPort = AdminPort;

    DbgPrint("CannotCore!%s: Communicate port connected.\n", __FUNCTION__);

    return STATUS_SUCCESS;
}

VOID FgCoreControlPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
)
/*++

Routine Description

    Communication disconnection callback routine.
    This is called when user-mode disconnects the server port.

Arguments

    ConnectionCookie - The cookie set in connect notify callback. It is connection context.

Return Value

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);

    FLT_ASSERT(Globals.ControlClientPort != NULL);

    FltCloseClientPort(Globals.Filter, &Globals.ControlClientPort);

    DbgPrint("CannotCore!%s: Communicate port disconnected.\n", __FUNCTION__);
}

NTSTATUS
FgCoreControlMessageNotifyCallback(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputSize) PVOID Input,
    _In_ ULONG InputSize,
    _Out_writes_bytes_to_opt_(OutputSize, *ReturnSize) PVOID Output,
    _In_ ULONG OutputSize,
    _Out_ PULONG ReturnSize
) {
    NTSTATUS status = STATUS_SUCCESS;
    ULONG variableSize = 0ul;
    FG_MESSAGE_TYPE commandType = 0;
    PFG_MESSAGE message = NULL;
    PFG_MESSAGE_RESULT result = NULL;
    USHORT ruleAmount = 0;

    UNREFERENCED_PARAMETER(ConnectionCookie);

    if (NULL == ReturnSize) return STATUS_INVALID_PARAMETER_6;

    message = (PFG_MESSAGE)Input;
    result = (PFG_MESSAGE_RESULT)Output;

    *ReturnSize = 0;

    switch (commandType) {
    case GetCoreVersion:

        //
        // Get core version information.
        //

        if (NULL == Output) return STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) return STATUS_INVALID_PARAMETER_5;

        result->CoreVersion.Major = FG_CORE_VERSION_MAJOR;
        result->CoreVersion.Minor = FG_CORE_VERSION_MINOR;
        result->CoreVersion.Patch = FG_CORE_VERSION_PATCH;
        result->CoreVersion.Build = FG_CORE_VERSION_BUILD;
        break;

    case AddRules:
    case RemoveRules:
        
        if (NULL == Input) return STATUS_INVALID_PARAMETER_2;
        if (InputSize <= sizeof(FG_MESSAGE)) return STATUS_INVALID_PARAMETER_3;
        if (NULL == Output) return STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) return STATUS_INVALID_PARAMETER_5;

        try {
            if (AddRules == commandType) {
                status = FgAddRules(&Globals.RulesList,
                                    Globals.RulesListLock,
                                    message->RulesNumber,
                                    (FG_RULE*)message->Rules,
                                    &ruleAmount);
                if (!NT_SUCCESS(status)) {
                    LOG_ERROR("NTSTATUS: 0x%08x, add rules failed", status);
                    break;
                }
            } else {
                status = FgFindAndRemoveRule(&Globals.RulesList,
                                             Globals.RulesListLock,
                                             message->RulesNumber,
                                             (FG_RULE*)message->Rules,
                                             &ruleAmount);
                if (!NT_SUCCESS(status)) {
                    LOG_ERROR("NTSTATUS: 0x%08x, remove rules failed", status);
                    break;
                }
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
            LOG_ERROR("NTSTATUS: 0x%08x, add rules failed", status);
            break;
        }

        break;

    case CleanupRules:

        //
        // Clear all rules in all instance rule tables or all rules in an instance rule table.
        //

        if (NULL == Output) return STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) return STATUS_INVALID_PARAMETER_5;
        
        result->RulesAmount = (USHORT)FgCleanupRuleEntriesList(Globals.RulesListLock, &Globals.RulesList);
        break;

    default:

        DBG_WARNING("Unknown command type: '%d'", commandType);
        status = STATUS_NOT_SUPPORTED;
    }

    result->Status = status;
    *ReturnSize = sizeof(FG_MESSAGE_RESULT) + variableSize;

    return STATUS_SUCCESS;
}

/*-------------------------------------------------------------
    Monitor communication routines.
-------------------------------------------------------------*/

NTSTATUS
FgMonitorPortConnectCallback(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID CorePortCookie,
    _In_reads_bytes_(ContextBytes) PVOID ConnectionContext,
    _In_ ULONG ContextBytes,
    _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
    )
{
    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(CorePortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ContextBytes);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    return STATUS_NOT_IMPLEMENTED;
}

VOID
FgMonitorPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
    ) 
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
}
