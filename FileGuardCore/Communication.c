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

    Definition of communication with user-mode application.

Environment:

    Kernel mode.

--*/

#include "FileGuardCore.h"
#include "Communication.h"

/*-------------------------------------------------------------
    Core control communication routines.
-------------------------------------------------------------*/

NTSTATUS
FgcControlPortConnectCallback(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID ServerPortCookie,
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

    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(ContextBytes);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    FLT_ASSERT(Globals.ControlClientPort == NULL);

    Globals.ControlClientPort = ClientPort;

    LOG_INFO("Control port connected");

    return STATUS_SUCCESS;
}

VOID
FgcControlPortDisconnectCallback(
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
    UNREFERENCED_PARAMETER(ConnectionCookie);

    PAGED_CODE();

    FLT_ASSERT(Globals.ControlClientPort != NULL);

    FltCloseClientPort(Globals.Filter, &Globals.ControlClientPort);

    LOG_INFO("Control port disconnected");
}

NTSTATUS
FgcControlMessageNotifyCallback(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputSize) PVOID Input,
    _In_ ULONG InputSize,
    _Out_writes_bytes_to_opt_(OutputSize, *ReturnSize) PVOID Output,
    _In_ ULONG OutputSize,
    _Out_ PULONG ReturnSize
    )
{
    NTSTATUS status = STATUS_SUCCESS, resultStatus = STATUS_SUCCESS;
    ULONG resultVariableSize = 0ul;
    FG_MESSAGE_TYPE commandType = 0;
    PFG_MESSAGE message = NULL;
    PFG_MESSAGE_RESULT result = NULL;
    USHORT ruleAmount = 0;
    UNICODE_STRING pathName = { 0 };

    UNREFERENCED_PARAMETER(ConnectionCookie);

    if (NULL == Input) return STATUS_INVALID_PARAMETER_2;
    if (InputSize < sizeof(FG_MESSAGE)) status = STATUS_INVALID_PARAMETER_3;
    if (NULL == ReturnSize) return STATUS_INVALID_PARAMETER_6;

    message = (PFG_MESSAGE)Input;
    result = (PFG_MESSAGE_RESULT)Output;
    commandType = message->Type;

    *ReturnSize = 0;

    switch (commandType) {
    case GetCoreVersion:

        //
        // Get core version information.
        //

        if (NULL == Output) status = STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) status = STATUS_INVALID_PARAMETER_5;
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, message invalid parameter", status);
            break;
        }

        result->CoreVersion.Major = FG_CORE_VERSION_MAJOR;
        result->CoreVersion.Minor = FG_CORE_VERSION_MINOR;
        result->CoreVersion.Patch = FG_CORE_VERSION_PATCH;
        result->CoreVersion.Build = FG_CORE_VERSION_BUILD;
        break;

    case AddRules:
    case RemoveRules:
        
        if (NULL == Output) status = STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) status = STATUS_INVALID_PARAMETER_5;
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, message invalid parameter", status);
            break;
        }

        try {
            if (AddRules == commandType) {
                resultStatus = FgcAddRules(&Globals.RulesList,
                                          Globals.RulesListLock,
                                          message->RulesAmount,
                                          (FG_RULE*)message->Rules,
                                          &ruleAmount);
                if (!NT_SUCCESS(status)) {
                    LOG_ERROR("NTSTATUS: 0x%08x, add rules failed", status);
                    break;
                }
                LOG_INFO("Attempt to add %hu rule(s), %hu rules added successfully", message->RulesAmount, ruleAmount);

            } else {

                resultStatus = FgcFindAndRemoveRule(&Globals.RulesList,
                                                   Globals.RulesListLock,
                                                   message->RulesAmount,
                                                   (FG_RULE*)message->Rules,
                                                   &ruleAmount);
                if (!NT_SUCCESS(status)) {
                    LOG_ERROR("NTSTATUS: 0x%08x, remove rules failed", status);
                    break;
                }
                LOG_INFO("Attempt to remove %hu rule(s), %hu rules removed successfully", message->RulesAmount, ruleAmount);
            }

            result->AffectedRulesAmount = ruleAmount;

        } except(EXCEPTION_EXECUTE_HANDLER) {
            resultStatus = GetExceptionCode();
            LOG_ERROR("NTSTATUS: 0x%08x, add rules failed", status);
            break;
        }

        break;

    case QueryRules:

        if (NULL == Output) status = STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) status = STATUS_INVALID_PARAMETER_5;
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, message invalid parameter", status);
            break;
        }

        resultStatus = FgcGetRules(&Globals.RulesList, 
                                  Globals.RulesListLock,
                                  (FG_RULE*)result->Rules.RulesBuffer, 
                                  OutputSize - sizeof(FG_MESSAGE_RESULT),
                                  &result->Rules.RulesAmount, 
                                  &result->Rules.RulesSize);
        if (STATUS_BUFFER_TOO_SMALL != resultStatus) {
            if (!NT_SUCCESS(resultStatus)) {
                LOG_ERROR("NTSTATUS: 0x%08x, get rules failed", resultStatus);
                break;
            } else {
                resultVariableSize = result->Rules.RulesSize;
            }
        }

        break;

    case CheckMatchedRule:

        if (NULL == Output) status = STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) status = STATUS_INVALID_PARAMETER_5;
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, message invalid parameter", status);
            break;
        }

        pathName.Length = message->PathNameSize;
        pathName.MaximumLength = message->PathNameSize;
        pathName.Buffer = message->PathName;
        resultStatus = FgcMatchRulesEx(&Globals.RulesList,
                                      Globals.RulesListLock,
                                      &pathName,
                                      (FG_RULE*)result->Rules.RulesBuffer,
                                      OutputSize - sizeof(FG_MESSAGE_RESULT),
                                      &result->Rules.RulesAmount,
                                      &result->Rules.RulesSize);
        if (STATUS_BUFFER_TOO_SMALL != resultStatus) {
            if (!NT_SUCCESS(resultStatus)) {
                LOG_ERROR("NTSTATUS: 0x%08x, get matched rules failed", resultStatus);
                break;
            } else {
                resultVariableSize = result->Rules.RulesSize;
            }
        }

        break;

    case CleanupRules:

        //
        // Clear all rules in all instance rule tables or all rules in an instance rule table.
        //

        if (NULL == Output) status = STATUS_INVALID_PARAMETER_4;
        if (OutputSize < sizeof(FG_MESSAGE_RESULT)) status = STATUS_INVALID_PARAMETER_5;
        if (!NT_SUCCESS(status)) {
            LOG_ERROR("NTSTATUS: 0x%08x, message invalid parameter", status);
            break;
        }
        
        result->AffectedRulesAmount = FgcCleanupRuleEntriesList(Globals.RulesListLock, &Globals.RulesList);
        break;
        
    default:

        DBG_WARNING("Unknown command type: '%d'", commandType);
        status = STATUS_NOT_SUPPORTED;
    }
    
    if(NULL != result) {
        result->ResultCode = RtlNtStatusToDosError(resultStatus);
        result->ResultSize = sizeof(FG_MESSAGE_RESULT) + resultVariableSize;
        *ReturnSize = sizeof(FG_MESSAGE_RESULT) + resultVariableSize;
    } else {
        *ReturnSize = 0;
    }

    return status;
}

/*-------------------------------------------------------------
    Monitor communication routines.
-------------------------------------------------------------*/

NTSTATUS
FgcMonitorPortConnectCallback(
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
FgcMonitorPortDisconnectCallback(
    _In_opt_ PVOID ConnectionCookie
    ) 
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
}
