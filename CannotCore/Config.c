/*
	@File   Config.c
	@Note   Config entry and operations.

	@Mode   Kernel
	@Author Fxtack
*/

#include "CannotCore.h"

// Generic table routine required.
// Comparing two config entry that save in table and return the compare result.
RTL_GENERIC_COMPARE_RESULTS NTAPI ConfigEntryCompareRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ PVOID LEntry,
	_In_ PVOID REntry
) {
	UNREFERENCED_PARAMETER(Table);

	UNICODE_STRING lPath = { 0 }, rPath = { 0 };

	RtlInitUnicodeString(&lPath, ((PCANNOT_CONFIG)LEntry)->Path);
	RtlInitUnicodeString(&rPath, ((PCANNOT_CONFIG)REntry)->Path);
	
	ASSERT(lPath.Buffer != NULL && lPath.Length != 0);
	ASSERT(rPath.Buffer != NULL && rPath.Length != 0);

	KdPrint(("CannotCore!%s: '%wZ' ? '%wZ'\n", __func__, lPath, rPath));
	
	if (RtlPrefixUnicodeString(&rPath, &lPath, TRUE)) {
		if (lPath.Length == rPath.Length) {
			return GenericEqual;
		} else if (lPath.Buffer != NULL && lPath.Buffer[(rPath.Length / sizeof(WCHAR))] == OBJ_NAME_PATH_SEPARATOR) {
			return GenericEqual;
		}
	}

	return RtlCompareUnicodeString(&lPath, &rPath, TRUE) > 0 ? GenericGreaterThan : GenericLessThan;
}

// Generic table routine required.
// When table operation include config entry creation, this routine will be called for
// memory allocation.
PVOID NTAPI ConfigEntryAllocateRoutine(
	_In_ PRTL_GENERIC_TABLE Table,
	_In_ CLONG ByteSize
) {
	UNREFERENCED_PARAMETER(Table);

	ASSERT(ByteSize == (ULONG)(sizeof(RTL_BALANCED_LINKS) + sizeof(CANNOT_CONFIG)));

	PVOID mem = ExAllocateFromNPagedLookasideList(&Globals.ConfigEntryMemoryPool);

	if (mem != NULL)
		RtlZeroMemory(mem, ByteSize);

	return mem;
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

	ExFreeToNPagedLookasideList(&Globals.ConfigEntryMemoryPool, Entry);
}

// Allocate memory to new a config object.
PCANNOT_CONFIG NewConfig(
	VOID
) {
	ASSERT(&Globals.ConfigObjectMemoryPool != NULL);

	PCANNOT_CONFIG configMemory = (PCANNOT_CONFIG)ExAllocateFromNPagedLookasideList(&Globals.ConfigObjectMemoryPool);

	if (configMemory != NULL)
		RtlZeroMemory(configMemory, sizeof(CANNOT_CONFIG));

	KdPrint(("CannotCore!%s: Allocate config object memory: %p\n", __func__, configMemory));

	return configMemory;
}

// Release memory to drop config object.
VOID DropConfig(
	_In_ PCANNOT_CONFIG configObject
) {
	ASSERT(&Globals.ConfigObjectMemoryPool != NULL);
	
	if (configObject != NULL) {

		ExFreeToNPagedLookasideList(&Globals.ConfigObjectMemoryPool, configObject);

		KdPrint(("CannotCore!%s: Config object memory released\n", __func__));
	}
}

// Query config from table and return it by memory copying.
NTSTATUS QueryConfigFromTable(
	_In_  PCANNOT_CONFIG QueryConfig,
	_Out_ PCANNOT_CONFIG ResultConfig
) {
	NTSTATUS status = STATUS_SUCCESS;

	PCANNOT_CONFIG pQueryEntry = NewConfig();
	PCANNOT_CONFIG pResultEntry = NULL;
	
	RtlCopyMemory(pQueryEntry, QueryConfig, sizeof(CANNOT_CONFIG));

	ExAcquireFastMutex(&Globals.ConfigTableLock);

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, (PVOID)pQueryEntry);
	if (pResultEntry == NULL) {
		status = STATUS_UNSUCCESSFUL;
		goto returnWithUnlock;
	}
		
	// Copy config result to output buffer.
	RtlCopyMemory(ResultConfig, pResultEntry, sizeof(CANNOT_CONFIG));

returnWithUnlock:

	ExReleaseFastMutex(&Globals.ConfigTableLock);

	DropConfig(pQueryEntry);

	return status;
}

// Add a config to table.
NTSTATUS AddConfigToTable(
	_In_ PCANNOT_CONFIG InsertConfig
) {
	BOOLEAN inserted = FALSE;
	PCANNOT_CONFIG pAddConfigEntry = NewConfig();

	RtlCopyMemory(pAddConfigEntry, InsertConfig, sizeof(CANNOT_CONFIG));

	ExAcquireFastMutex(&Globals.ConfigTableLock);

	KdPrint(("CannotCore!%s: '%ws', '%ws'\n", __func__, InsertConfig->Path, pAddConfigEntry->Path));

	// Insert config entry.
	RtlInsertElementGenericTable(&Globals.ConfigTable,
	                             (PVOID)pAddConfigEntry,
	                             sizeof(CANNOT_CONFIG),
	                             &inserted);

	ExReleaseFastMutex(&Globals.ConfigTableLock);

	DropConfig(pAddConfigEntry);

	return inserted ? STATUS_SUCCESS : STATUS_DUPLICATE_OBJECTID;
}

// Find up the config by index and remove it.
NTSTATUS RemoveConfigFromTable(
	_In_ PCANNOT_CONFIG RemoveConfig
) {
	NTSTATUS status;
	PCANNOT_CONFIG pRemoveConfigEntry = NewConfig();

	RtlCopyMemory(pRemoveConfigEntry, RemoveConfig, sizeof(CANNOT_CONFIG));

	ExAcquireFastMutex(&Globals.ConfigTableLock);

	status = RtlDeleteElementGenericTable(&Globals.ConfigTable, pRemoveConfigEntry) ?
		STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

	ExReleaseFastMutex(&Globals.ConfigTableLock);

	DropConfig(pRemoveConfigEntry);

	return status;
}

// Clean all configs from table.
NTSTATUS CleanupConfigTable(
	VOID
) {
	PVOID entry;

	ExAcquireFastMutex(&Globals.ConfigTableLock);

	while (!RtlIsGenericTableEmpty(&Globals.ConfigTable)) {
		entry = RtlGetElementGenericTable(&Globals.ConfigTable, 0);
		RtlDeleteElementGenericTable(&Globals.ConfigTable, entry);
	}

	ExReleaseFastMutex(&Globals.ConfigTableLock);

	return STATUS_SUCCESS;
}

CANNOT_CONFIG_TYPE MatchConfig(
	_In_ PUNICODE_STRING Path
) {
	CANNOT_CONFIG_TYPE matchConfigType = CannotTypeNothing;
	PCANNOT_CONFIG pQueryEntry = NewConfig();
	PCANNOT_CONFIG pResultEntry;
	
	RtlCopyMemory(pQueryEntry->Path, Path->Buffer, Path->Length);

	ExAcquireFastMutex(&Globals.ConfigTableLock);

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, pQueryEntry);
	if (pResultEntry != NULL) {
		matchConfigType = pResultEntry->CannotType;
	}

	ExReleaseFastMutex(&Globals.ConfigTableLock);

	DropConfig(pQueryEntry);

	KdPrint(("CannotCore!%s: Try match path: '%wZ', match result: %d\n",
		__func__, Path, matchConfigType));

	return matchConfigType;
}