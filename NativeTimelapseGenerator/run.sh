#!/bin/bash
cd ../NativeTimelapseGenerator.Console
dotnet publish -r linux-x64 -c Release
cd ../NativeTimelapseGenerator
cp -r ../NativeTimelapseGenerator.Console/bin/Release/net8.0/linux-x64/publish/* .
gcc -o NativeTimelapseGenerator -lpng -lcurl main.c main-thread.c main-thread.h image-generator.c image-generator.h canvas-downloader.c canvas-downloader.h console.c console.h canvas-saver.c canvas-saver.h worker-structs.h
./NativeTimelapseGenerator