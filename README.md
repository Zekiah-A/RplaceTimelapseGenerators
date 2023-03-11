# RplaceTimelapseGenerators
A collection of backup/timelapse generators for the site https://rplace.tk.

 - See the rplace server software @[RplaceServer](https://github.com/Zekiah-A/RplaceServer.git) for info on hosting your own rplace instance. 
 - See the main site codebase @[rslashplace2](https://github.com/rslashplace2/rslashplace2.github.io) for helping with rplace development.

### Within this repo there are currently 3 generators.
 - *TimelapseGenerator indicates that the current generator has
the functionality to produce a series of frames, either from server backuplists, or git logs.
 - *BackupGenerator indicates that this generator is only able to produce an image from an rplace canvas backup.


As it stands, the easiest to run generator is the node (javascript-based) generator, which can be found in [NodeBackupGenerator/](NodeBackupGenerator/downloader.js).

For a highly advanced generator, cabable of generating an animation of the dates from backups, and other edge-case functionality, but also harder to set up (will require source code modification), see the dotnet generator at [DotnetTimelapseGenerator/](DotnetTimelapseGenerator/)

For a highly performant, and relitabely primative generator, see the C native generator at [NativeTimelapseGenerator/](NativeTimelapseGenerator/main.c)


Instructions on how to use each generator can be found within their respective main source files. Dependencies may and will be needed to run certain generators here.
