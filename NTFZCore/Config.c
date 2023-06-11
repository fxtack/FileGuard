/*
	@File   ConfigEntry.c
	@Note   Config entry and operations.

	@Mode   Kernel
	@Author Fxtack
*/

#include "NTFZCore.h"

// Generic table routine required.
// Comparing two config entry that save in table and return the compare result.
RTL_GENERIC_COMPARE_RESULTS NTAPI ConfigEntryCompareRoutine(
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

	KdPrint(("NTFZCore!%s: comparing: [%wZ] [%wZ].", __func__, lEntryPath, rEntryPath));

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
PVOID NTAPI ConfigEntryAllocateRoutine(
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
VOID NTAPI ConfigEntryFreeRoutine(
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
	NTSTATUS status = STATUS_SUCCESS;
	KIRQL originalIRQL;
	PNTFZ_CONFIG pResultEntry = NULL;

	KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, QueryConfigEntry);
	if (pResultEntry == NULL) {
		status = STATUS_UNSUCCESSFUL;
		goto returnWithUnlock;
	}
		
	// Copy config result to output buffer.
	RtlCopyMemory(ResultConfigEntry, pResultEntry, sizeof(NTFZ_CONFIG));

returnWithUnlock:

	KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	return status;
}

// Add a config to table.
NTSTATUS AddConfigToTable(
	_In_ PNTFZ_CONFIG InsertConfigEntry
) {
	KIRQL originalIRQL;
	BOOLEAN inserted = FALSE;

	KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	// Insert config entry.
	RtlInsertElementGenericTable(&Globals.ConfigTable,
	                             (PVOID)InsertConfigEntry,
	                             sizeof(NTFZ_CONFIG),
	                             &inserted);

	KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	return inserted ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

// Find up the config by index and remove it.
NTSTATUS RemoveConfigFromTable(
	_In_ PNTFZ_CONFIG RemoveConfigEntry
) {
	KIRQL originalIRQL;

	KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	NTSTATUS status = RtlDeleteElementGenericTable(&Globals.ConfigTable, RemoveConfigEntry) ?
		STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

	KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	return status;
}

// Clean all configs from table.
NTSTATUS CleanupConfigTable(
	VOID
) {
	KIRQL originalIRQL;
	PVOID entry;

	KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	while (!RtlIsGenericTableEmpty(&Globals.ConfigTable)) {
		entry = RtlGetElementGenericTable(&Globals.ConfigTable, 0);
		RtlDeleteElementGenericTable(&Globals.ConfigTable, entry);
	}
	KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	return STATUS_SUCCESS;
}

NTFZ_CONFIG_TYPE MatchConfig(
	_In_ FS_ITEM_TYPE FsItemType,
	_In_ PUNICODE_STRING Path
) {
	// TODO
}