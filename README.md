# FileGuard

By using FileGuard, you can achieve dynamic control over access to files and directories in the Windows NTFS file system by setting rules.

## FileGuardCore

The FileGuardCore module is a filter driver implemented based on the Windows MiniFilter framework. It is written in C and runs within the Windows operating system kernel.

This module implements the following functionalities:  

- Maintains active file access rules in memory.
- Restricts file IO operations based on file access rules.
- Provides a FilterCommunication interface through which user-mode programs can manage file access rules and control the driver status.

## FileGuardAdmin

The FileGuardAdmin module is a Windows user-mode command-line tool written in C++.

FileGuardAdmin communicates with FileGuardCore through the FilterCommunication interface, providing an efficient and convenient way to manage file access rules and control the driver.

Use `FileGuardAdmin.exe --help` to see the usage instructions:

```shell
This tool is used to manage file access rules and control the driver.
Usage: E:\FileGuard\Output\Debug-x64\FileGuardAdmin\FileGuardAdmin.exe [OPTIONS] SUBCOMMAND

Options:
  --help                      Print this help message and exit
  --version                   Display program version information and exit

Subcommands:
  unload                      Unload core driver
  detach                      Detach core instance
  add                         Add a rule
  remove                      Remove a rule
  query                       Query all rules and output it
  check-matched               Check which rules will be matched for path
  monitor                     Receive monitoring records
  cleanup                     Cleanup all rules
```

## FileGuardLib

# FileGuardLib

FileGuardLib is a library wrapper for the FileGuard FilterCommunication interface. You can use FileGuardLib to communicate more conveniently with the FileGuardCore module driver and complete secondary development.

FileGuardLib provides the following interfaces:

- `FglConnectCore`: Create a communicate connection with the FileGuardCore driver;
- `FglDisconnectCore`: Close the communicate connection with the FileGuardCore driver;
- `FglReceiveMonitorRecords`: Set the callback function for processing rule enforcement records(monitor records);
- `FglGetCoreVersion`: Get the version information of FileGuardCore;
- `FglSetUnloadAcceptable`: Set the acceptability of unloading the FileGuardCore driver;
- `FglSetDetachAcceptable`: Set the acceptability of detaching the FileGuardCore driver instance;
- `FglAddBulkRules`: Add multiple rules in bulk;
- `FglAddSingleRule`: Add a single rule;
- `FglRemoveBulkRules`: Remove multiple rules in bulk;
- `FglRemoveSingleRule`: Remove a single rule;
- `FglCheckMatchedRules`: Check if a path will be affected by any rule;
- `FglQueryRules`: Query multiple rules;
- `FglCleanupRules`: Clear all file rules.

For detailed documentation on the FileGuardLib library interfaces, refer to the project wiki (TODD).