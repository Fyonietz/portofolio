cmake_minimum_required(VERSION 3.10)
project(Phoenix)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Source files
file(GLOB_RECURSE MAIN "main.cpp")
file(GLOB_RECURSE CIVET_C "libs/Civet/civetweb.c")
file(GLOB_RECURSE CIVET_INL "libs/Civet/*.inl")
file(GLOB_RECURSE CORE_SOURCES "core/*.cpp")

# CivetWeb static library
add_library(civetweb STATIC ${CIVET_C} ${CIVET_INL})
target_include_directories(civetweb PUBLIC libs)
add_definitions(-DNO_SSL)

# Core static library
add_library(core_static STATIC ${CORE_SOURCES})
set_target_properties(core_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(core_static 
    PRIVATE core 
    PRIVATE libs
)
target_link_libraries(core_static PRIVATE civetweb)

# Add routes dynamic library
add_subdirectory(routes)

# Main executable
add_executable(${PROJECT_NAME} ${MAIN})
target_include_directories(${PROJECT_NAME}
    PRIVATE core
    PRIVATE libs
)

# Link main executable to dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE core_static civetweb pthread dl)

# Platform-specific definitions
if (WIN32)
    add_definitions(-DNO_SSL)
else()
    add_definitions(-DNO_SSL -DLINUX)
endif()