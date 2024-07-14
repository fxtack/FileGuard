# FileGuardCore

FileGuardCore 模块是基于 Windows MiniFilter 框架实现的过滤器驱动，使用 C 编写，运行在 Windows 操作系统内核中。

该模块实现了以下功能：  
- 在内存中维护生效的文件访问规则；
- 根据文件访问规则限制文件 IO 操作；
- 提供 FilterCommunication 接口，用户态程序通该接口管理文件访问规则、控制驱动状态。