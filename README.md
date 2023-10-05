# FileGuard

FileGuard is implemented using a minifilter driver. By using FileGuard, you can set custom permission management under Windows' NTFS.

The FileGuard contains two major modules:

* FileGuardAdmin: Communicate with driver(core) to manage configuration.
* FileGuardCore: Driver running at Windows kernel implemented by minifilter.