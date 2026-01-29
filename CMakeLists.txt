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

# ===== BUSCAR RECURSOS =====
message(STATUS "=== Buscando recursos en: ${CMAKE_CURRENT_SOURCE_DIR}/src/view ===")

file(GLOB_RECURSE RESOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.css"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.js"
)

set(RESOURCES_HEADER "${GENERATED_DIR}/resources.h")

# ===== DEBUG: Mostrar recursos encontrados =====
list(LENGTH RESOURCE_FILES RESOURCE_COUNT)
message(STATUS "Recursos encontrados: ${RESOURCE_COUNT}")

if(RESOURCE_COUNT EQUAL 0)
    message(WARNING "⚠️  No se encontraron recursos en src/view/")
    
    # Verificar que existe el directorio
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/view")
        message(STATUS "El directorio src/view/ existe")
        # Listar contenido
        file(GLOB VIEW_CONTENTS "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*")
        foreach(ITEM ${VIEW_CONTENTS})
            message(STATUS "  - ${ITEM}")
        endforeach()
    else()
        message(FATAL_ERROR "❌ El directorio src/view/ NO existe")
    endif()
    
    # Crear header vacío
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
    message(STATUS "=== Recursos encontrados (${RESOURCE_COUNT}) ===")
    foreach(RESOURCE ${RESOURCE_FILES})
        message(STATUS "  📄 ${RESOURCE}")
    endforeach()
    
    # Convertir lista a string separado por espacios (para pasar a script)
    string(REPLACE ";" " " RESOURCE_FILES_STR "${RESOURCE_FILES}")
    
    message(STATUS "Ejecutando embed_resources.cmake...")
    message(STATUS "  RESOURCE_FILES: ${RESOURCE_FILES_STR}")
    message(STATUS "  SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
    message(STATUS "  OUTPUT_FILE: ${RESOURCES_HEADER}")
    
    # Generar header con recursos reales
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

# ===== TARGET PERSONALIZADO para generar resources.h =====
add_custom_target(generate_resources
    DEPENDS ${RESOURCES_HEADER}
)

# ===== Fuentes =====
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*/(build|CMakeFiles)/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*CMakeCXXCompilerId\\.cpp$")

message(STATUS "Archivos fuente encontrados: ${SOURCES}")

file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
list(FILTER HEADERS EXCLUDE REGEX ".*/(build|CMakeFiles)/.*")

# ===== Ejecutable =====
add_executable(${EXECUTABLE_NAME} ${SOURCES} ${HEADERS})

# ===== CRÍTICO: El ejecutable DEPENDE de que resources.h se genere primero =====
add_dependencies(${EXECUTABLE_NAME} generate_resources)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GENERATED_DIR}
)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    Threads::Threads
    pthread
)

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