# Cannot

通过使用 Cannot，你可以通过设置配置，实现对文件与目录访问的动态控制。

Cannot 主要包含两个主要的模块：

* CannotAdmin：与内核驱动模块通讯以实现对配置的管理；
* CannotCore：基于 Minifilter 实现的的 Windows 内核驱动。