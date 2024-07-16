# FileGuard

通过使用 FileGuard，你可以通过设置规则，实现对 Windows NTFS 文件系统中文件与目录访问的动态控制。

FileGuard 主要包含以下模块。

## FileGuardCore

FileGuardCore 模块是基于 Windows MiniFilter 框架实现的过滤器驱动，使用 C 编写，运行在 Windows 操作系统内核中。

该模块实现了以下功能：  
- 在内存中维护生效的文件访问规则；
- 根据文件访问规则限制文件 IO 操作；
- 提供 FilterCommunication 接口，用户态程序通该接口管理文件访问规则、控制驱动状态。

## FileGuardAdmin

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

## FileGuardLib

FileGuardLib 是对 FileGuard FilterCommunication 接口的库封装，你可以通过 FileGuardLib 更为方便的与 FileGuardCore 模块驱动通信，完成二次开发。

FileGuardLib 提供以下接口：

- `FglConnectCore`：创建与 FileGuardCore 驱动的通信连接；
- `FglDisconnectCore`：断开与 FileGuardCore 驱动的通信连接；
- `FglReceiveMonitorRecords`：设置规则生效记录处理回调；
- `FglGetCoreVersion`：获取 FileGuardCore 版本信息；
- `FglSetUnloadAcceptable`：设置 FileGuardCore 驱动是否可卸载；
- `FglSetDetachAcceptable`：设置 FileGuardCore 驱动实例是否可分离；
- `FglAddBulkRules`：批量添加多个文件访问规则；
- `FglAddSingleRule`：添加一条文件访问规则；
- `FglRemoveBulkRules`：批量一出多个文件访问规则；
- `FglRemoveSingleRule`：移除一条文件访问规则；
- `FglCheckMatchedRules`：检查一个路径是否会被某条文件访问规则影响；
- `FglQueryRules`：查询多条文件访问规则；
- `FglCleanupRules`：清空所有文件访问规则。

详细的 FileGuardLib 库接口文档参见项目 wiki。