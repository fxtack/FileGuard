# FileGuardLib

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