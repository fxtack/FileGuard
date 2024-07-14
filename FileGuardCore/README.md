# FileGuardCore

The FileGuardCore module is a filter driver implemented based on the Windows MiniFilter framework. It is written in C and runs within the Windows operating system kernel.

This module implements the following functionalities:  

- Maintains active file access rules in memory.
- Restricts file IO operations based on file access rules.
- Provides a FilterCommunication interface through which user-mode programs can manage file access rules and control the driver status.