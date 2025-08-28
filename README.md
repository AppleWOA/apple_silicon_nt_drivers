Apple Silicon Windows drivers and HAL Extensions

===================================================



\### Disclaimer



These drivers are absolutely not production ready and not guaranteed to be secure at all! Exercise caution when using these drivers. I assume no responsibility or liability for usage of the software and/or any issues that arise because of it.



\### Description



This repository contains the source code (with binary release distributions to come later on) for the Windows drivers and HAL Extensions to be used as part of the AppleWOA project. The driver implementations will usually be based on the Asahi Linux equivalent drivers, with driver structure following the samples Microsoft publishes on the \[Windows Driver Samples](https://github.com/microsoft/Windows-driver-samples) repository whenever possible.



\### Status



Nothing's implemented yet, however can note down the major things that need to be done.



\### License



Unless stated otherwise, code in this repository will usually be licensed under the following licenses:

\- MIT (the default license I'm using for most of the project and should be assumed if not stated)

\- GPL2 (only for drivers that are based on Linux implementations, which by extension are GPL licensed, so drivers that are based on GPL-licensed drivers from Asahi Linux or other Linux/OSS projects will also be licensed as such.)



\### Credits/Acknowledgements

```

\[The Asahi Linux project](https://alx.sh)

\[The WOA Project (LumiaWOA/DuoWOA)](https://github.com/WOA-Project/)

