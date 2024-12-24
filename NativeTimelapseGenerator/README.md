# NativeTimelapseGenerator

## Basic overview:
- Main thread creates download worker, reads file incrementally, giving each download worker
  the corresponding canvas to download to memory.
- Download workers run, push curl results to the canvas queue,
- These are then processed by the render workers, which render out the canvases to image frames
- These are finally passed to save workers, which pull the results from the render workers and save to disk

## Building and Running:

### Prerequisites

- CMake 3.10 or higher
- GCC
- .NET SDK

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