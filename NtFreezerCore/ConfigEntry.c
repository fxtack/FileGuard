/*
	@File   ConfigEntry.c
	@Note   Config entry and operations.

	@Mode   Kernel
	@Author Fxtack
*/

#include "NtFreezerCore.h"


RTL_GENERIC_COMPARE_RESULTS
NTAPI
configEntryCompareRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ PVOID LEntry,
	_In_ PVOID REntry
) {
	PNTFZ_CONFIG_ENTRY lEntry = (PNTFZ_CONFIG_ENTRY)LEntry;
	PNTFZ_CONFIG_ENTRY rEntry = (PNTFZ_CONFIG_ENTRY)REntry;
	UNICODE_STRING lEntryPath = { 0 }, rEntryPath = { 0 };

	UNREFERENCED_PARAMETER(Table);

	RtlInitUnicodeString(&lEntryPath, lEntry->Index);
	RtlInitUnicodeString(&rEntryPath, rEntry->Index);

	KdPrint(("[NtFreeCore!%s] comparing: [%wZ] [%wZ].", __func__, lEntryPath, rEntryPath));

	if (RtlPrefixUnicodeString(&rEntryPath, &lEntryPath, FALSE)) {
		if (lEntryPath.Length == rEntryPath.Length) {

			// When file or directory (lEntryPath) equal config path (rEntryPath).
			return GenericEqual;
		} else if (lEntryPath.Buffer[rEntryPath.Length / sizeof(WCHAR)] == OBJ_NAME_PATH_SEPARATOR) {

			// When the file or directory (lEntryPath) under the config path (rEntryPath).
			return GenericEqual;
		}
	}
	return RtlCompareUnicodeString(&lEntryPath, &rEntryPath, FALSE) > 0 ?
		GenericGreaterThan : GenericLessThan;
}


PVOID
NTAPI
configEntryAllocateRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ CLONG ByteSize
) {
	UNREFERENCED_PARAMETER(Table);

	ASSERT(ByteSize == NTFZ_CONFIG_ENTRY_SIZE);

	return ExAllocateFromNPagedLookasideList(
		&Globals.ConfigEntryFreeMemPool
	);
}


VOID
NTAPI
configEntryFreeRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Entry
) {
	UNREFERENCED_PARAMETER(Table);

	ASSERT(Entry != NULL);

	ExFreeToNPagedLookasideList(&Globals.ConfigEntryFreeMemPool, Entry);
}


NTSTATUS QueryConfigFromTable(
	_In_ PCWSTR ConfigIndex,
	_Out_ PNTFZ_CONFIG ResultConfig
) {
	PNTFZ_CONFIG_ENTRY pResultEntry = NULL;
	NTFZ_CONFIG_ENTRY queryEntry = { 0 };
	queryEntry.Index = ConfigIndex;

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, &queryEntry);
	if (pResultEntry == NULL) {
		ResultConfig = NULL;
		return STATUS_UNSUCCESSFUL;
	}

	ResultConfig = &pResultEntry->Config;
	return STATUS_SUCCESS;
}


NTSTATUS AddConfigToTable(
	_In_ PNTFZ_CONFIG_ENTRY InsertConfigEntry
) {
	BOOLEAN inserted;

	// Set config entry index.
	InsertConfigEntry->Index = InsertConfigEntry->Config.Path;

	// Insert config entry.
	RtlInsertElementGenericTable(&Globals.ConfigTable,
	                             (PVOID)InsertConfigEntry,
	                             sizeof(NTFZ_CONFIG_ENTRY),
	                             &inserted);
	if (inserted == NULL || !inserted) 
		return STATUS_UNSUCCESSFUL;

	return STATUS_SUCCESS;
}


NTSTATUS RemoveConfigFromTable(
	_In_ PCWSTR ConfigIndex
) {
	PNTFZ_CONFIG_ENTRY pResultEntry = NULL;
	NTFZ_CONFIG_ENTRY queryEntry = { 0 };
	BOOLEAN deleted = FALSE;
	queryEntry.Index = ConfigIndex;

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, &queryEntry);
	if (pResultEntry == NULL) {
		return STATUS_SUCCESS;
	}

	deleted = RtlDeleteElementGenericTable(&Globals.ConfigTable, pResultEntry);
	if (!deleted) {
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}


VOID CleanupConfigTable(
	VOID
) {
	PVOID entry;

	while (!RtlIsGenericTableEmpty(&Globals.ConfigTable)) {
		entry = RtlGetElementGenericTable(&Globals.ConfigTable, 0);
		RtlDeleteElementGenericTable(&Globals.ConfigTable, entry);
	}
}