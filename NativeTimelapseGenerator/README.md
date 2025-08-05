# NativeTimelapseGenerator

## Basic overview:
- Main thread creates download worker, reads file incrementally, giving each download worker
   the corresponding canvas to download to memory.
- Download workers run, push curl results to the canvas queue,
- These are then processed by the render workers, which render out the canvases to image frames
- These are finally passed to save workers, which pull the results from the render workers and save to disk
- The final timelapse video is then able to be generated with ffmpeg, using commands such as the following: 
   `ffmpeg -framerate 24 -pattern_type glob -i "backups/*.png" -c:v libx264 -pix_fmt yuv420p -vf "pad=2000:2000:(ow-iw)/2:(oh-ih)/2" timelapse.mp4`

### Worker flow:
Each worker performs the following steps:
1. Receive work from the main thread, in the form of a `*Job` struct.
2. Check what kind of job it is, and based on the job type handle the work that needs to be done accordingly.
3. Return the result of the work, in the form of a `*Result` struct.

**For example:**
1. Download worker receives a DownloadJob from the main thread in the form of:
    ```c
    (DownloadJob) {
        // Inherited from WorkerJob base struct
        .commit_id = 6900,
        .commit_hash = "401a8e16d0477bbb80c5111b5cefdca15931f8ff",
        
        // Members
        .type = DOWNLOAD_CANVAS,
        .url = "https://raw.githubusercontent.com/rplacetk/canvas1/401a8e16d0477bbb80c5111b5cefdca15931f8ff/place"
    }
    ```
2. Download worker invokes download_canvas, which return a DownloadResult containing the following data:
    ```c
    (DownloadResult) {
        // Inherited from DownloadResult base struct
        .error = DOWNLOAD_ERROR_NONE,
        .error_msg = NULL,
        
        //  Members
        .render_job = (RenderJob) {
            .commit_id = 6900,
            .commit_hash = "401a8e16d0477bbb80c5111b5cefdca15931f8ff",
            .type = RENDER_CANVAS,
            .canvas = (RenderJobCanvas) {
                .width = 2000,
                .height = 2000,
                .palette_size = 32,l
                .palette = NULL,
                .canvas_size = 4000000,
                .canvas = (uint8_t*) 0x7fffffff0000
            }
        }
    }
3. Download worker calls push_render_stack with the render job.
4. The cycle repeats.

**Chain of operations:**
A canvas download will cause a `CANVAS_DOWNLOAD` and `CANVAS_RENDER` save to be produced, 
a placer download will cause a `PLACERS_DOWNLOAD`, `TOP_PLACERS_RENDER` and `CANVAS_CONTROL_RENDER`
save to be produced, and a date render will cause a `DATE_RENDER` save to be produced.


## Building and Running:
> [!NOTE]
> The functionality of this program has only been tested on linux. Operation on any
> other system is unsupported and not guaranteed to work!

### Prerequisites

- CMake 3.10 or higher
- GCC
- BunJS

### Dependencies:
1. This project depends on [libffcall](https://www.gnu.org/software/libffcall/). For example,
    [ffcall](https://archlinux.org/packages/extra/x86_64/ffcall/) on arch.
2. This project depends on [readline](https://www.gnu.org/software/bash/manual/html_node/Readline-Interaction.html)
3. This project depends on [libpng](http://www.libpng.org/pub/png/libpng.html)
4. This project depends on [cairo](https://www.cairographics.org/) 
5. This project depends on [openssl](https://openssl-library.org/)

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

### Debugging notes:
Extreme debugging can be performed with asan, see the following:
```
cmake -B build -DENABLE_ASAN=ON .
```
```bash
workspaceFolder="/path/to/project/folder"
gdb -ex "set environment ASAN_OPTIONS=halt_on_error=1:log_path=asan.log:suppressions=${workspaceFolder}/NativeTimelapseGenerator/asan.supp:verbosity=3" \
    -ex "set environment UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:suppressions=${workspaceFolder}/NativeTimelapseGenerator/ubsan.supp" \
    -ex "run" ./NativeTimelapseGenerator
```
Backup generation can be tested with the following:
```
start_generation https://github.com/rplacetk/canvas1 https://raw.githubusercontent.com/rplacetk/canvas1 https://server.rplace.live commit_hashes.txt 10
```
In the case that the TCP socket is occupied after an unexpected shutdown, `fuser`
can be used to automatically kill any existing process making use of that socket:
```
fuser -k 5555/tcp
```