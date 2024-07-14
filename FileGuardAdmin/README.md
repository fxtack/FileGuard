# FileGuardAdmin

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
