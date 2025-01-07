cmake_minimum_required(VERSION 3.31)
project(NativeTimelapseGenerator)

# Set C standard to 23
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

# Enable MS extensions
if(MSVC)
	add_compile_options(-fpermissive)
else()
	add_compile_options(-fms-extensions)
endif()

# Default to debug build
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

# Add preprocessor definition for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Manually add source files
set(SOURCE_FILES
	${CMAKE_SOURCE_DIR}/main.c
	${CMAKE_SOURCE_DIR}/memory_utils.c
	${CMAKE_SOURCE_DIR}/main_thread.c
	${CMAKE_SOURCE_DIR}/workers/download_worker.c
	${CMAKE_SOURCE_DIR}/workers/save_worker.c
	${CMAKE_SOURCE_DIR}/workers/render_worker.c
	${CMAKE_SOURCE_DIR}/console.c
)

# Add subdirectory for parson library
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/parson)

# Add stb
include_directories(${CMAKE_SOURCE_DIR}/lib/stb)

# Find libgit2 package
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

# Find Cairo package
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)

# Find SQLite3 package
find_package(SQLite3 REQUIRED)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Link libraries
target_link_libraries(${PROJECT_NAME} png curl m parson ${LIBGIT2_LIBRARIES} SQLite::SQLite3 ${CAIRO_LIBRARIES})

# Detect C# source files
#file(GLOB_RECURSE CSHARP_SOURCES ${CMAKE_SOURCE_DIR}/../NativeTimelapseGenerator.Console/**/*.cs)

# Custom command to build C# project
add_custom_command(
	OUTPUT ${CMAKE_SOURCE_DIR}/../NativeTimelapseGenerator.Console/bin/Release/net9.0/linux-x64/publish/NativeTimelapseGenerator.Console.so
	COMMAND dotnet publish -r linux-x64 -c Release
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../NativeTimelapseGenerator.Console
	#DEPENDS ${CSHARP_SOURCES} # Add C# sources as dependencies
	COMMENT "Building C# project"
)

# Custom target to build C# project
add_custom_target(build_csharp ALL
	DEPENDS ${CMAKE_SOURCE_DIR}/../NativeTimelapseGenerator.Console/bin/Release/net9.0/linux-x64/publish/NativeTimelapseGenerator.Console.so
)

# Custom command to copy C# publish files
add_custom_command(
	TARGET build_csharp POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/../NativeTimelapseGenerator.Console/bin/Release/net9.0/linux-x64/publish
	${CMAKE_BINARY_DIR}
	COMMENT "Copying C# publish files"
)

# Custom command to copy schema.sql
add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_SOURCE_DIR}/schema.sql
	${CMAKE_BINARY_DIR}/schema.sql
	COMMENT "Copying schema.sql to build directory"
)

# Custom command to copy example_commit_hashes.txt
add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_SOURCE_DIR}/example_commit_hashes.txt
	${CMAKE_BINARY_DIR}/example_commit_hashes.txt
	COMMENT "Copying example_commit_hashes.txt to build directory"
)

# Build all targets
add_custom_target(build_all
	DEPENDS build_csharp ${PROJECT_NAME}
)

# Build all with debug target
add_custom_target(build_all_debug
	COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR}
	COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
	DEPENDS build_csharp
)

# Run the program
add_custom_target(run
	COMMAND ${CMAKE_BINARY_DIR}/${PROJECT_NAME}
	DEPENDS build_all
)

# Run the program with debug
add_custom_target(run_debug
	COMMAND ${CMAKE_BINARY_DIR}/${PROJECT_NAME} --nocli
	DEPENDS build_all_debug
)

# Clean build artifacts
add_custom_target(clean_artifacts
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
)
