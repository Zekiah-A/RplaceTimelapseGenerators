# NativeTimelapseGenerator

## Basic overview:
- Main thread creates download worker, reads file incrementally, giving each download worker
  the corresponding canvas to download to memory.
- Download workers run, push curl results to the canvas queue,
- These are then processed by the render workers, which render out the canvases to image frames
- These are finally passed to save workers, which pull the results from the render workers and save to disk
- The final timelapse video is then able to be generated with ffmpeg, using commands such as the following: 
  `ffmpeg -framerate 24 -pattern_type glob -i "backups/*.png" -c:v libx264 -pix_fmt yuv420p -vf "pad=2000:2000:(ow-iw)/2:(oh-ih)/2" timelapse.mp4`

## Building and Running:

### Prerequisites

- CMake 3.10 or higher
- GCC
- .NET SDK

### Dependencies:
1. This project depends on [libfcall](https://www.gnu.org/software/libffcall/), which is a nightmare to build,
  so it's sources have not been included as a submodule in this project. It is reccomended you use the
  provided ffcall library for your distro for ffcall to work correctly. For example,
  [ffcall](https://archlinux.org/packages/extra/x86_64/ffcall/) on arch.

### Build and Run

1. Navigate to the project directory:
    ```sh
    cd NativeTimelapseGenerator
    ```

2. Create a build directory and navigate into it:
    ```sh
    mkdir build && cd build
    ```

3. Build the project:
    ```sh
    cmake ..
    cmake --build .
    ```

4. Run the project:
    ```sh
    cmake --build . --target run
    ```

### Build and Run with Debug

1. Navigate to the project directory:
    ```sh
    cd NativeTimelapseGenerator
    ```

2. Create a build directory and navigate into it:
    ```sh
    mkdir build && cd build
    ```

3. Build the project with debug flags:
    ```sh
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    cmake --build .
    ```

4. Run the project with debug:
    ```sh
    cmake --build . --target run_debug
    ```

### Clean Build Artifacts

1. Navigate to the build directory:
    ```sh
    cd build
    ```

2. Clean the build artifacts:
    ```sh
    cmake --build . --target clean
    ```