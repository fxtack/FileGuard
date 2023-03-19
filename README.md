# NtFreezer

By using NtFreezer, you can dynamically set files or directories under NTFS to read-only status by specifying configurations. Files or directories set as read-only by NtFreezer cannot be modified at all, even if the operator has the highest permissions.



NtFreezer has two parts：

* NtFreezerAdmin: Communicate with driver to manage configuration.
* NtFreezerCore: Driver running at windows  kernel implemented by minifilter.