# Build routes as a shared library for runtime update
file(GLOB_RECURSE PAGE_LIST "../pages/*.cpp")
file(GLOB_RECURSE LIBS "../libs/Phoenix/*.cpp")
add_library(routes SHARED
    ${PAGE_LIST}
    ${LIBS}
    register.cpp
)

target_include_directories(routes
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_SOURCE_DIR}/pages
        ${CMAKE_SOURCE_DIR}/core
)

# Link routes to all required dependencies
target_link_libraries(routes PRIVATE 
    core_static 
    civetweb 
    pthread 
    dl
    sqlite3
)

set_target_properties(routes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/core"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/core"
    VERSION 1.0
    SOVERSION 1
)

add_definitions(-DNO_SSL)

# Optional: Copy command to ensure the library is in the right place
add_custom_command(TARGET routes POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:routes>
        ${CMAKE_SOURCE_DIR}/core/
    COMMENT "Copying routes library to core directory"
)
