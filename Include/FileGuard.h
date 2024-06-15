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

    Basic declarations of FileGuardCore driver.

Environment:

    Kernel/User mode.

--*/

#ifndef __FILE_GUARD_H__
#define __FILE_GUARD_H__

#pragma warning(push)
#pragma warning(disable:4200 4201)

#define FG_CORE_FILTER_NAME L"FileGuardCore"

#define FG_CORE_CONTROL_PORT_NAME L"\\FileGuardControlPort"
#define FG_MONITOR_PORT_NAME      L"\\FileGuardMonitorPort"

/*-------------------------------------------------------------
    Core communication message
-------------------------------------------------------------*/

typedef enum _FG_MESSAGE_TYPE {
    GetCoreVersion,
    SetUnloadAcceptable,
    SetDetachAcceptable,
    ControlCore,
    AddRules,
    RemoveRules,
    QueryRules,
    CheckMatchedRule,
    CleanupRules
} FG_MESSAGE_TYPE;

typedef struct _FG_CORE_VERSION {
    USHORT Major;
    USHORT Minor;
    USHORT Patch;
    USHORT Build;
} FG_CORE_VERSION, *PFG_CORE_VERSION;

typedef enum _FG_RULE_MAJOR_CODE {
    RuleMajorNone,
    RuleMajorAccessDenined,
    RuleMajorReadOnly,
    RuleMajorMaximum
} FG_RULE_MAJOR_CODE, *PFG_RULE_MAJOR_CODE;

typedef enum _FG_RULE_MINOR_CODE {
    RuleMinorNone,
    RuleMinorMonitored,
    RuleMinorMaximum
} FG_RULE_MINOR_CODE, *PFG_RULE_MINOR_CODE;

#define VALID_MAJOR_RULE_CODE(_major_code_) ((_major_code_) > RuleMajorNone && (_major_code_) < RuleMajorMaximum)
#define VALID_MINOR_RULE_CODE(_minor_code_) ((_minor_code_) > RuleMinorNone && (_minor_code_) < RuleMinorMaximum)
#define VALID_RULE_CODE(_major_code_, _minor_code_) (VALID_MAJOR_RULE_CODE(_major_code_) && VALID_MINOR_RULE_CODE(_minor_code_))

typedef struct _FG_RULE {
    FG_RULE_MAJOR_CODE MajorCode;
    FG_RULE_MINOR_CODE MinorCode;
    USHORT PathExpressionSize; // The bytes size of `FilePathName`, contain null wide char.
    WCHAR PathExpression[];    // End of null.
} FG_RULE, *PFG_RULE;

//
// Message of user application send to core.
//
typedef struct _FG_MESSAGE {
    FG_MESSAGE_TYPE Type;
    ULONG MessageSize;
    union {
        BOOLEAN UnloadAcceptable;
        BOOLEAN DetachAcceptable;
        struct {
            USHORT RulesAmount;
            ULONG RulesSize;
            UCHAR Rules[];
        } DUMMYSTRUCTNAME;
        struct {
            USHORT PathNameSize;
            WCHAR PathName[];
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} FG_MESSAGE, *PFG_MESSAGE;

//
// Message result of core returned.
//
typedef struct _FG_MESSAGE_RESULT {
    ULONG ResultCode;
    ULONG ResultSize;
    union {
        FG_CORE_VERSION CoreVersion;
        ULONG AffectedRulesAmount;
        struct {
            USHORT RulesAmount;
            ULONG RulesSize;
            UCHAR RulesBuffer[];
        } Rules;
        FG_RULE MatchedRule;
    } DUMMYUNIONNAME;
} FG_MESSAGE_RESULT, *PFG_MESSAGE_RESULT;

typedef struct _FG_FILE_ID_DESCRIPTOR {
    
    ULONGLONG VolumeSerialNumber;
    
    union {
        LARGE_INTEGER FileId64;
        FILE_ID_128 FileId128;  
    } FileId;

} FG_FILE_ID_DESCRIPTOR, *PFG_FILE_ID_DESCRIPTOR;

/*-------------------------------------------------------------
    Monitor structures
-------------------------------------------------------------*/

typedef struct _FG_MONITOR_RECORD {
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    ULONG_PTR RequestorPid;
    ULONG_PTR RequestorTid;
    FG_FILE_ID_DESCRIPTOR FileIdDescriptor;
    LARGE_INTEGER RecordTime;
    ULONG_PTR OpInformation;
    NTSTATUS OpStatus;
    USHORT FilePathSize;
    USHORT RenameFilePathSize;
    WCHAR FilePath[];
} FG_MONITOR_RECORD, *PFG_MONITOR_RECORD;

#pragma warning(pop)

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
    FILTER_MESSAGE_HEADER Header;

    //
    // Message body.
    //
    FG_RECORDS_MESSAGE_BODY	Body;

} FG_MONITOR_RECORDS_MESSAGE, *PFG_MONITOR_RECORDS_MESSAGE;

#endif