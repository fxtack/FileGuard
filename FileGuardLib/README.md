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

For detailed documentation on the FileGuardLib library interfaces, refer to the project wiki.