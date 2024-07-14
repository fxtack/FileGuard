# FileGuardAdmin

FileGuardAdmin 模块是使用 C++ 编写的 Windows 用户态命令行工具。

FileGuardAdmin 通过 FilterCommunication 接口与 FileGuardCore 通信，实现方便快捷的文件访问规则管理、驱动控制功能。

使用 `FileGuardAdmin.exe --help` 查看使用方法：

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
  check-matched               Check which rules will matched for path
  monitor                     Receive monitoring records
  cleanup                     Cleanup all rules
```

