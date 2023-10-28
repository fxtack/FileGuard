# FileGuard

通过使用 FileGuard，你可以通过设置配置，实现对文件与目录访问的动态控制。

FileGuard 主要包含两个主要的模块：

* FileGuardCore：基于 Minifilter 实现的的 Windows 内核驱动；
* FileGuardLib：与 FileGuardCore 模块通讯的 API 库；
* FileGuardAdmin：与内核驱动模块通讯以实现对配置的管理。

## FileGuardCore 

FileGuardCore 支持以下规则匹配模式：

- 路径前缀匹配规则：基于路径前缀对文件操作进行规则匹配；
- 文件名匹配规则：基于文件名对文件操作进行规则匹配；
- 基于用户态程序的规则匹配：需要进行规则匹配时，将发送消息询问用户态程序规则是否被匹配。

支持以下规则类型：

- Access Denied。支持对文件、目录设置，拒绝任何对文件的访问；
- Read Only。支持对文件设置，无法对文件进行修改、删除操作；
- Hide。支持对文件、目录设置，匹配规则的条目将被隐藏。

## FileGuardLib

通过使用 FileGuardLib，开发者可以与 FileGuardCore 模块通讯实现二次开发。

| FileGuard API | 描述 | supported |
| ------------- | ---- | --------- |
| GetVersion    |      |           |
|               |      |           |

## FileGuardAdmin

FileGuardAdmin 使用 FileGuardLib 实现，你可以将其理解为一个示例，包含以下功能实现：

- 规则管理（规则新增、修改、移除）；
- 基于用户态程序的规则匹配；
- 规则命中监控。