/*
	@File   ConfigEntry.c
	@Note   Config entry and operations.

	@Mode   Kernel
	@Author Fxtack
*/

#include "NtFreezerCore.h"

// Generic table routine required.
// Comparing two config entry that save in table and return the compare result.
RTL_GENERIC_COMPARE_RESULTS NTAPI configEntryCompareRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ PVOID LEntry,
	_In_ PVOID REntry
) {
	UNREFERENCED_PARAMETER(Table);

	PNTFZ_CONFIG lEntry = (PNTFZ_CONFIG)LEntry;
	PNTFZ_CONFIG rEntry = (PNTFZ_CONFIG)REntry;
	UNICODE_STRING lEntryPath = { 0 }, rEntryPath = { 0 };

	RtlInitUnicodeString(&lEntryPath, lEntry->Path);
	RtlInitUnicodeString(&rEntryPath, rEntry->Path);

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

// Generic table routine required.
// When table operation include config entry creation, this routine will be called for
// memory allocation.
PVOID NTAPI configEntryAllocateRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ CLONG ByteSize
) {
	UNREFERENCED_PARAMETER(Table);

	ASSERT(ByteSize == (ULONG)(sizeof(RTL_BALANCED_LINKS) + sizeof(NTFZ_CONFIG)));

	return ExAllocateFromNPagedLookasideList(&Globals.ConfigEntryFreeMemPool);
}

// Generic table routine required.
// When table operation include config entry delete, this routine will be called for free
// config entry memory.
VOID NTAPI configEntryFreeRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Entry
) {
	UNREFERENCED_PARAMETER(Table);

	ASSERT(Entry != NULL);

	ExFreeToNPagedLookasideList(&Globals.ConfigEntryFreeMemPool, Entry);
}

// Query config from table and return it by memory copying.
NTSTATUS QueryConfigFromTable(
	_In_  PNTFZ_CONFIG QueryConfigEntry,
	_Out_ PNTFZ_CONFIG ResultConfigEntry
) {
	PNTFZ_CONFIG pResultEntry = NULL;

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, QueryConfigEntry);
	if (pResultEntry == NULL)
		return STATUS_UNSUCCESSFUL;

	// Copy config result to output buffer.
	RtlCopyMemory(ResultConfigEntry, pResultEntry, sizeof(NTFZ_CONFIG));
	return STATUS_SUCCESS;
}

// Add a config to table.
NTSTATUS AddConfigToTable(
	_In_ PNTFZ_CONFIG InsertConfigEntry
) {
	BOOLEAN inserted = FALSE;

	// Insert config entry.
	RtlInsertElementGenericTable(&Globals.ConfigTable,
	                             (PVOID)InsertConfigEntry,
	                             sizeof(NTFZ_CONFIG),
	                             &inserted);

	return inserted ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

// Find up the config by index and remove it.
NTSTATUS RemoveConfigFromTable(
	_In_ PNTFZ_CONFIG RemoveConfigEntry
) {
	return RtlDeleteElementGenericTable(&Globals.ConfigTable, RemoveConfigEntry) ?
		STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

// Clean all configs from table.
NTSTATUS CleanupConfigTable(
	VOID
) {
	PVOID entry;

	while (!RtlIsGenericTableEmpty(&Globals.ConfigTable)) {
		entry = RtlGetElementGenericTable(&Globals.ConfigTable, 0);
		RtlDeleteElementGenericTable(&Globals.ConfigTable, entry);
	}

	return STATUS_SUCCESS;
}