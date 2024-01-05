#!/bin/bash
cd ../NativeTimelapseGenerator.Console
dotnet publish -r linux-x64 -c Release
cd ../NativeTimelapseGenerator
cp -r ../NativeTimelapseGenerator.Console/bin/Release/net8.0/linux-x64/publish/* .
if [[ "$*" == *"-debug"* ]]
then
    gcc -o NativeTimelapseGenerator -lpng -lcurl -lm main.c main-thread.c main-thread.h image-generator.c image-generator.h canvas-downloader.c canvas-downloader.h console.c console.h canvas-saver.c canvas-saver.h worker-structs.h -g -DDEBUG
    # gdb ./NativeTimelapseGenerator
    # tty /dev/pts/4 # example, redirect to random other console
    # run
    ./NativeTimelapseGenerator -nocli
else
    gcc -o NativeTimelapseGenerator -lpng -lcurl -lm main.c main-thread.c main-thread.h image-generator.c image-generator.h canvas-downloader.c canvas-downloader.h console.c console.h canvas-saver.c canvas-saver.h worker-structs.h
    ./NativeTimelapseGenerator
fi