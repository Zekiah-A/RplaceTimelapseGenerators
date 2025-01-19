# Define project and standard settings
cmake_minimum_required(VERSION 3.31)
project(NativeTimelapseGenerator)

# Set C standard to 23
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

# Enable MS extensions
add_compile_options(-fms-extensions)

# Set default build type for single-config generators
if(CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_GENERATOR STREQUAL "Unix Makefiles")
	if(NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
	endif()
endif()

# Add preprocessor definition for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Add subdirectories for libraries
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/parson)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/libnanobuf)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/libdill)

# Manually add source files
set(SOURCE_FILES
	${CMAKE_SOURCE_DIR}/main.c
	${CMAKE_SOURCE_DIR}/memory_utils.c
	${CMAKE_SOURCE_DIR}/main_thread.c
	${CMAKE_SOURCE_DIR}/workers/download_worker.c
	${CMAKE_SOURCE_DIR}/workers/save_worker.c
	${CMAKE_SOURCE_DIR}/workers/render_worker.c
	${CMAKE_SOURCE_DIR}/console.c
	${CMAKE_SOURCE_DIR}/database.c
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE png curl m readline dill nanobuf parson ffcall ${LIBGIT2_LIBRARIES} SQLite::SQLite3 ${CAIRO_LIBRARIES})

# Set web build directory variable
set(WEB_BUILD_DIR ${CMAKE_SOURCE_DIR}/web/dist)

# Detect web source files
file(GLOB_RECURSE WEB_SOURCES ${CMAKE_SOURCE_DIR}/web/**/*)
list(FILTER WEB_SOURCES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/web/dist/.*")
list(FILTER WEB_SOURCES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/web/node_modules/.*")

# Custom command to build web project
add_custom_command(
	OUTPUT ${WEB_BUILD_DIR}
	COMMAND bun run build
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/web
	DEPENDS ${WEB_SOURCES} 
	COMMENT "Building web frontend"
)

# Custom target to build web project
add_custom_target(build_web ALL
	DEPENDS ${WEB_BUILD_DIR}
)

# Copy web publish files after build
add_custom_command(
	TARGET build_web POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${WEB_BUILD_DIR}
	${CMAKE_BINARY_DIR}
	COMMENT "Copying web dist files to build directory"
)

# Copy schema.sql to the build directory
add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_SOURCE_DIR}/schema.sql
	${CMAKE_BINARY_DIR}/schema.sql
	COMMENT "Copying schema.sql to build directory"
)

# Copy example_commit_hashes.txt to the build directory
add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_SOURCE_DIR}/example_commit_hashes.txt
	${CMAKE_BINARY_DIR}/example_commit_hashes.txt
	COMMENT "Copying example_commit_hashes.txt to build directory"
)

# Run the program
add_custom_target(run
	COMMAND ${CMAKE_BINARY_DIR}/${PROJECT_NAME}
	DEPENDS ${PROJECT_NAME} build_web
)

# Run the program with debug
add_custom_target(run_debug
	COMMAND ${CMAKE_BINARY_DIR}/${PROJECT_NAME}
	DEPENDS ${PROJECT_NAME} build_web
)

# Clean build artifacts
add_custom_target(clean_artifacts
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
)
