# FileGuard

通过使用 FileGuard，你可以通过设置配置，实现对文件与目录访问的动态控制。

FileGuard 主要包含两个主要的模块：

* FileGuardAdmin：与内核驱动模块通讯以实现对配置的管理；
* FileGuardCore：基于 Minifilter 实现的的 Windows 内核驱动。