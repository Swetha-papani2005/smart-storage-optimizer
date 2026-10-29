cmake_minimum_required(VERSION 3.15)
project(SmartStorageOptimizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link statically on Windows MinGW so the binary is 100% portable
if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

include_directories(include)

set(SOURCES
    src/sha256.cpp
    src/metadata_collector.cpp
    src/duplicate_detector.cpp
    src/waste_analyzer.cpp
    src/reporter.cpp
    src/trash_manager.cpp
    src/main.cpp
)

add_executable(storage_optimizer ${SOURCES})
