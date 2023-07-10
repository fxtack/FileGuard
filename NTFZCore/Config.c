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

	RTL_GENERIC_COMPARE_RESULTS configCompareResult;
	INT pathCompareResult;
	PCWSTR lPath = ((PNTFZ_CONFIG)LEntry)->Path;
	PCWSTR rPath = ((PNTFZ_CONFIG)REntry)->Path;

	SIZE_T lPathLen = wcslen(lPath);
	SIZE_T rPathLen = wcslen(rPath);

	ASSERT(lPath != NULL && lPathLen != 0);
	ASSERT(rPath != NULL && rPathLen != 0);

	pathCompareResult = wcsncmp(lPath, rPath, rPathLen);

	KdPrint(("NTFZCore!%s: '%ws' ? '%ws' = %d", __func__, lPath, rPath, pathCompareResult));

	if (pathCompareResult == 0) {
		
		if (lPathLen == rPathLen) {
			/*
				example:
				Left path:  \Device\HarddiskVolume7\dir
				Right path: \Device\HarddiskVolume7\dir
			*/
			configCompareResult = GenericEqual;
			goto compareFinish;
		}

		if (lPath[rPathLen] == OBJ_NAME_PATH_SEPARATOR) {
			/*
				example:
				Left path:  \Device\HarddiskVolume7\dir\file
				Right path: \Device\HarddiskVolume7\dir
			*/
			configCompareResult = GenericEqual;
			goto compareFinish;
		}
	}

	configCompareResult = pathCompareResult > 0 ? GenericGreaterThan : GenericLessThan;

compareFinish:

	return configCompareResult;
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
PNTFZ_CONFIG NewConfig(
	VOID
) {
	ASSERT(&Globals.ConfigObjectMemoryPool != NULL);

	PNTFZ_CONFIG configMemory = (PNTFZ_CONFIG)ExAllocateFromNPagedLookasideList(&Globals.ConfigObjectMemoryPool);

	if (configMemory != NULL)
		RtlZeroMemory(configMemory, sizeof(PNTFZ_CONFIG));

	KdPrint(("NTFZCore!%s: Allocate config object memory: %p\n", __func__, configMemory));

	return configMemory;
}

// Release memory to drop config object.
VOID DropConfig(
	_In_ PNTFZ_CONFIG configObject
) {
	ASSERT(&Globals.ConfigObjectMemoryPool != NULL);
	
	if (configObject != NULL) {

		ExFreeToNPagedLookasideList(&Globals.ConfigObjectMemoryPool, configObject);

		KdPrint(("NTFZCore!%s: Config object memory released\n", __func__));
	}
}

// Query config from table and return it by memory copying.
NTSTATUS QueryConfigFromTable(
	_In_  PNTFZ_CONFIG QueryConfig,
	_Out_ PNTFZ_CONFIG ResultConfig
) {
	NTSTATUS status = STATUS_SUCCESS;
	//KIRQL originalIRQL;
	PNTFZ_CONFIG pQueryEntry = NewConfig();
	PNTFZ_CONFIG pResultEntry = NULL;
	
	RtlCopyMemory(pQueryEntry, QueryConfig, sizeof(NTFZ_CONFIG));

	//KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, (PVOID)pQueryEntry);
	if (pResultEntry == NULL) {
		status = STATUS_UNSUCCESSFUL;
		goto returnWithUnlock;
	}
		
	// Copy config result to output buffer.
	RtlCopyMemory(ResultConfig, pResultEntry, sizeof(NTFZ_CONFIG));

returnWithUnlock:

	//KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	DropConfig(pQueryEntry);

	return status;
}

// Add a config to table.
NTSTATUS AddConfigToTable(
	_In_ PNTFZ_CONFIG InsertConfig
) {
	//KIRQL originalIRQL;
	BOOLEAN inserted = FALSE;
	PNTFZ_CONFIG pAddConfigEntry = NewConfig();

	RtlCopyMemory(pAddConfigEntry, InsertConfig, sizeof(NTFZ_CONFIG));

	//KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	KdPrint(("NTFZCore!%s: '%ws', '%ws'\n", __func__, InsertConfig->Path, pAddConfigEntry->Path));

	// Insert config entry.
	RtlInsertElementGenericTable(&Globals.ConfigTable,
	                             (PVOID)pAddConfigEntry,
	                             sizeof(NTFZ_CONFIG),
	                             &inserted);

	//KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	DropConfig(pAddConfigEntry);

	return inserted ? STATUS_SUCCESS : STATUS_DUPLICATE_OBJECTID;
}

// Find up the config by index and remove it.
NTSTATUS RemoveConfigFromTable(
	_In_ PNTFZ_CONFIG RemoveConfig
) {
	NTSTATUS status;
	//KIRQL originalIRQL;
	PNTFZ_CONFIG pRemoveConfigEntry = NewConfig();

	RtlCopyMemory(pRemoveConfigEntry, RemoveConfig, sizeof(NTFZ_CONFIG));

	//KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	status = RtlDeleteElementGenericTable(&Globals.ConfigTable, pRemoveConfigEntry) ?
		STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

	//KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	DropConfig(pRemoveConfigEntry);

	return status;
}

// Clean all configs from table.
NTSTATUS CleanupConfigTable(
	VOID
) {
	//KIRQL originalIRQL;
	PVOID entry;

	//KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL);

	while (!RtlIsGenericTableEmpty(&Globals.ConfigTable)) {
		entry = RtlGetElementGenericTable(&Globals.ConfigTable, 0);
		RtlDeleteElementGenericTable(&Globals.ConfigTable, entry);
	}
	//KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	return STATUS_SUCCESS;
}

NTFZ_CONFIG_TYPE MatchConfig(
	_In_ PUNICODE_STRING Path
) {
	//KIRQL originalIRQL;
	NTFZ_CONFIG_TYPE matchConfigType = FzTypeNothing;
	PNTFZ_CONFIG pQueryEntry = NewConfig();
	PNTFZ_CONFIG pResultEntry;
	
	RtlCopyMemory(pQueryEntry->Path, Path->Buffer, Path->Length);

	//KeAcquireSpinLock(&Globals.ConfigTableLock, &originalIRQL)

	pResultEntry = RtlLookupElementGenericTable(&Globals.ConfigTable, pQueryEntry);
	if (pResultEntry != NULL) {
		matchConfigType = pResultEntry->FreezeType;
	}

	//KeReleaseSpinLock(&Globals.ConfigTableLock, originalIRQL);

	DropConfig(pQueryEntry);

	KdPrint(("NTFZCore!%s: Try match path: '%wZ', match result: %d\n",
		__func__, Path, matchConfigType));

	return matchConfigType;
}