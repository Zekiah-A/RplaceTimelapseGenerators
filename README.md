# RplaceTimelapseGenerators
A collection of backup and timelapse generators for the site [rplace.live](https://rplace.live).
 - For information on hosting your own rplace instance, see the rplace server software at [RplaceServer](https://github.com/Zekiah-A/RplaceServer.git).
 - To contribute to rplace development, visit the main site codebase at [rslashplace2](https://github.com/rslashplace2/rslashplace2.github.io).

### Generators in this repository
 - **TimelapseGenerator**: Produces a series of frames from server backup lists or git logs.
 - **BackupGenerator**: Generates an image from an rplace canvas backup.

### Node Backup Generator
A simple, cross-platform Node.js (JavaScript) generator, located in [NodeBackupGenerator/](NodeBackupGenerator/downloader.js). Also, check out [colour-utils](https://github.com/rplacetk/colour-utils) for additional utilities.

### Native Timelapse Generator
A highly performant, UNIX-only, multi-threaded generator written in C and C#. Find it in [NativeTimelapseGenerator/](NativeTimelapseGenerator/).

![NativeTimelapseGenerator structure diagram](NativeTimelapseGenerator/structure.png)

Dependencies and usage instructions for each generator are available in their respective directories.
