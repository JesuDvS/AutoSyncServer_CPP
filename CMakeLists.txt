cmake_minimum_required(VERSION 3.10)
project(GameObjectSystem VERSION 1.0 LANGUAGES CXX)

# ===== NOMBRE DEL EJECUTABLE =====
set(EXECUTABLE_NAME auto_sync_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ===== Directorios =====
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${GENERATED_DIR})

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)


# ===== BUSCAR RECURSOS =====
message(STATUS "=== Buscando recursos en: ${CMAKE_CURRENT_SOURCE_DIR}/src/view ===")

file(GLOB_RECURSE RESOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.css"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.js"
)

set(RESOURCES_HEADER "${GENERATED_DIR}/resources.h")

list(LENGTH RESOURCE_FILES RESOURCE_COUNT)
message(STATUS "Recursos encontrados: ${RESOURCE_COUNT}")

if(RESOURCE_COUNT EQUAL 0)
    message(WARNING "⚠️  No se encontraron recursos en src/view/")
    file(WRITE "${RESOURCES_HEADER}"
"#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H
#include <string>
#include <unordered_map>
namespace Resources {
    struct Resource { const char* content; const char* mime_type; };
    const std::unordered_map<std::string, Resource> RESOURCE_MAP = {};
    inline const Resource* getResource(const std::string&) { return nullptr; }
}
#endif
")
else()
    string(REPLACE ";" " " RESOURCE_FILES_STR "${RESOURCE_FILES}")

    add_custom_command(
        OUTPUT ${RESOURCES_HEADER}
        COMMAND ${CMAKE_COMMAND}
            -DRESOURCE_FILES=${RESOURCE_FILES_STR}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DOUTPUT_FILE=${RESOURCES_HEADER}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_resources.cmake"
        DEPENDS ${RESOURCE_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/embed_resources.cmake"
        COMMENT "📦 Embebiendo ${RESOURCE_COUNT} recursos en resources.h..."
        VERBATIM
    )
endif()

add_custom_target(generate_resources
    DEPENDS ${RESOURCES_HEADER}
)

# ===== Fuentes =====
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*/(build|CMakeFiles)/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*CMakeCXXCompilerId\\.cpp$")

file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
list(FILTER HEADERS EXCLUDE REGEX ".*/(build|CMakeFiles)/.*")

add_executable(${EXECUTABLE_NAME} ${SOURCES} ${HEADERS})

add_dependencies(${EXECUTABLE_NAME} generate_resources)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GENERATED_DIR}
)

# ===== LINKER =====
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    Threads::Threads
    CURL::libcurl
)


# 🔥 FIX CRÍTICO PARA std::experimental::filesystem 🔥
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE stdc++fs)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE c++fs)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

target_compile_options(${EXECUTABLE_NAME} PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

message(STATUS "===================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Executable name: ${EXECUTABLE_NAME}")
message(STATUS "Resources header: ${RESOURCES_HEADER}")
message(STATUS "Recursos a embeber: ${RESOURCE_COUNT}")
message(STATUS "===================================")
