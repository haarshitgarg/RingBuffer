cmake_minimum_required(VERSION 3.15)
project(RingBuffer VERSION 1.0.0 LANGUAGES CXX)

# Export compile commands for LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
)

# Header files
set(HEADERS
    include/RingBuffer.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

## Create library
#add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
#
## Set include directories for the library
#target_include_directories(${PROJECT_NAME} PUBLIC
#    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#    $<INSTALL_INTERFACE:include>
#)
#
## Installation rules
#install(TARGETS ${PROJECT_NAME}
#    ARCHIVE DESTINATION lib
#    LIBRARY DESTINATION lib
#    RUNTIME DESTINATION bin
#)
#
#install(FILES ${HEADERS}
#    DESTINATION include
#)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
