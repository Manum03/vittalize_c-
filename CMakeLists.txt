cmake_minimum_required(VERSION 3.14)
project(VITTALIZE_SERVER VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) 

# =======================================================
# 1. DESCARGA Y CONFIGURACIÓN DE DEPENDENCIAS
# =======================================================
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON) 

# --- Dependencias de FetchContent (Descarga) ---
FetchContent_Declare(json_stuff GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.2)
FetchContent_MakeAvailable(json_stuff)

FetchContent_Declare(sqlite_cpp_stuff GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git GIT_TAG 3.3.0)
FetchContent_MakeAvailable(sqlite_cpp_stuff)

# Crow en master (valor previo)
FetchContent_Declare(Crow_stuff GIT_REPOSITORY https://github.com/CrowCpp/Crow.git GIT_TAG master) # Usar master por si el tag específico falla
FetchContent_MakeAvailable(Crow_stuff)

# --- Dependencias del Sistema (Búsqueda) ---
find_package(Boost 1.52 REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
# Opcional: find_package(SQLite3 REQUIRED) -- Se puede omitir si SQLiteCpp lo maneja

# =======================================================
# 2. DEFINICIÓN DEL EJECUTABLE Y ENLACE
# =======================================================

set(SOURCE_DIR "src")
set(EXECUTABLE_NAME "vittalize_server")

add_executable(${EXECUTABLE_NAME}
    ${SOURCE_DIR}/vittalize_server.cpp
)

# 2.1. Incluir directorios de cabeceras de las librerías descargadas
target_include_directories(${EXECUTABLE_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${crow_stuff_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${json_stuff_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${sqlite_cpp_stuff_SOURCE_DIR}/include>
)

# 2.2. Enlazar librerías usando targets modernos
target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
        Boost::system
        Boost::thread
        OpenSSL::SSL
        # OpenSSL::Crypto # Descomentar si hay errores de criptografía

        # Targets de FetchContent (CORRECCIÓN CLAVE AQUÍ)
        Crow::Crow
        SQLiteCpp
        nlohmann_json::nlohmann_json
        
        # Librería base del sistema (para cubrir dependencias si SQLiteCpp no lo hace)
        sqlite3 # Usamos el nombre de la librería, si el target SQLite3::SQLite3 falla
)

add_compile_options(-Wno-error)
