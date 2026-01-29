# Script para embeber múltiples recursos como strings en C++

# DEBUG: Imprimir lo que recibimos
message(STATUS "=== EMBED RESOURCES DEBUG ===")
message(STATUS "RESOURCE_FILES recibido: ${RESOURCE_FILES}")
message(STATUS "SOURCE_DIR: ${SOURCE_DIR}")
message(STATUS "OUTPUT_FILE: ${OUTPUT_FILE}")

# Separar correctamente la lista de archivos
separate_arguments(RESOURCE_LIST UNIX_COMMAND "${RESOURCE_FILES}")

message(STATUS "RESOURCE_LIST después de separar: ${RESOURCE_LIST}")

# Filtrar archivos válidos
set(VALID_FILES "")
foreach(FILE_PATH ${RESOURCE_LIST})
    if(EXISTS "${FILE_PATH}" AND NOT IS_DIRECTORY "${FILE_PATH}")
        list(APPEND VALID_FILES "${FILE_PATH}")
        message(STATUS "  ✓ Archivo válido: ${FILE_PATH}")
    else()
        message(STATUS "  ✗ Archivo inválido o no existe: ${FILE_PATH}")
    endif()
endforeach()

list(LENGTH VALID_FILES FILE_COUNT)
message(STATUS "Total archivos válidos: ${FILE_COUNT}")

if(FILE_COUNT EQUAL 0)
    message(WARNING "⚠️  No se encontraron archivos válidos")
    # Crear header vacío
    file(WRITE "${OUTPUT_FILE}" 
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
    return()
endif()

# Función para convertir path a variable válida en C++
function(path_to_varname filepath outvar)
    string(REPLACE "${SOURCE_DIR}/" "" rel_path "${filepath}")
    string(REPLACE "/" "_" var_name "${rel_path}")
    string(REPLACE "." "_" var_name "${var_name}")
    string(REPLACE "-" "_" var_name "${var_name}")
    string(TOUPPER "${var_name}" var_name)
    set(${outvar} "${var_name}" PARENT_SCOPE)
endfunction()

# Función para obtener tipo MIME
function(get_mime_type filepath outvar)
    if(filepath MATCHES "\\.html$")
        set(${outvar} "text/html; charset=utf-8" PARENT_SCOPE)
    elseif(filepath MATCHES "\\.css$")
        set(${outvar} "text/css; charset=utf-8" PARENT_SCOPE)
    elseif(filepath MATCHES "\\.js$")
        set(${outvar} "application/javascript; charset=utf-8" PARENT_SCOPE)
    elseif(filepath MATCHES "\\.json$")
        set(${outvar} "application/json; charset=utf-8" PARENT_SCOPE)
    else()
        set(${outvar} "text/plain; charset=utf-8" PARENT_SCOPE)
    endif()
endfunction()

# Iniciar archivo header
file(WRITE "${OUTPUT_FILE}" 
"// AUTO-GENERADO POR CMAKE - NO EDITAR MANUALMENTE
#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H

#include <string>
#include <unordered_map>

namespace Resources {

// Estructura para almacenar recursos
struct Resource {
    const char* content;
    const char* mime_type;
};

")

# Procesar cada archivo
foreach(RESOURCE_FILE ${VALID_FILES})
    # Leer contenido
    file(READ "${RESOURCE_FILE}" FILE_CONTENT)
    
    # Obtener nombre de variable y ruta relativa
    path_to_varname("${RESOURCE_FILE}" VAR_NAME)
    string(REPLACE "${SOURCE_DIR}/" "" REL_PATH "${RESOURCE_FILE}")
    
    # Obtener tipo MIME
    get_mime_type("${RESOURCE_FILE}" MIME_TYPE)
    
    message(STATUS "  → ${REL_PATH} -> ${VAR_NAME}")
    
    # Escribir constante con raw string
    file(APPEND "${OUTPUT_FILE}" 
"// ${REL_PATH}
const char* ${VAR_NAME} = R\"EMBED_RESOURCE(${FILE_CONTENT})EMBED_RESOURCE\";

")
endforeach()

# Crear mapa de recursos
file(APPEND "${OUTPUT_FILE}" 
"// Mapa de recursos: ruta -> {contenido, mime_type}
const std::unordered_map<std::string, Resource> RESOURCE_MAP = {
")

set(FIRST_ENTRY TRUE)
foreach(RESOURCE_FILE ${VALID_FILES})
    path_to_varname("${RESOURCE_FILE}" VAR_NAME)
    string(REPLACE "${SOURCE_DIR}/src/view/" "" WEB_PATH "${RESOURCE_FILE}")
    string(REPLACE "${SOURCE_DIR}/src/view" "" WEB_PATH "${WEB_PATH}")
    
    # Normalizar la ruta web
    if(NOT WEB_PATH MATCHES "^/")
        set(WEB_PATH "/${WEB_PATH}")
    endif()
    
    get_mime_type("${RESOURCE_FILE}" MIME_TYPE)
    
    if(NOT FIRST_ENTRY)
        file(APPEND "${OUTPUT_FILE}" ",\n")
    endif()
    set(FIRST_ENTRY FALSE)
    
    message(STATUS "  📍 Ruta web: ${WEB_PATH} -> ${VAR_NAME}")
    file(APPEND "${OUTPUT_FILE}" "    {\"${WEB_PATH}\", {${VAR_NAME}, \"${MIME_TYPE}\"}}")
endforeach()

file(APPEND "${OUTPUT_FILE}" 
"
};

// Función helper para obtener recursos
inline const Resource* getResource(const std::string& path) {
    auto it = RESOURCE_MAP.find(path);
    if (it != RESOURCE_MAP.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace Resources

#endif // EMBEDDED_RESOURCES_H
")

message(STATUS "✅ resources.h generado con ${FILE_COUNT} recursos")