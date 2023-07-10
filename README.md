# NTFZ

By using NTFZ, files or directories under NTFS can be dynamically set to no permissions, read-only, or hidden state by specifying configurations.

NTFZ consists of two parts:

* NTFZAdmin: Communicate with driver to manage configuration.
* NTFZCore: Driver running at windows  kernel implemented by minifilter.