# Vittalize C++ Server

API REST en C++ (Crow + SQLiteCpp + nlohmann/json) con frontend estático (`web`) para evaluar signos vitales y síntomas.

## Requisitos Generales
- CMake ≥ 3.14
- Compilador C++17 (GCC/Clang en Linux, MSVC 2019+ en Windows)
- Git
- OpenSSL y Boost (system, thread)
- SQLite (generalmente viene con el sistema; SQLiteCpp compila y lo incluye)

## Linux (Ubuntu/Debian)
```bash
# Dependencias del sistema
sudo apt update
sudo apt install -y build-essential cmake libssl-dev libboost-system-dev libboost-thread-dev git

# Opcional: ninja para builds más rápidas
sudo apt install -y ninja-build
```

## Windows
- Instala **Visual Studio 2019/2022** con el componente “Desarrollo de escritorio con C++”.
- Instala **CMake** desde https://cmake.org/download/.
- Instala **Git for Windows**.
- OpenSSL y Boost: las trae vcpkg si prefieres; otra opción es usar los binarios precompilados de vcpkg:
  ```powershell
  git clone https://github.com/microsoft/vcpkg
  .\vcpkg\bootstrap-vcpkg.bat
  .\vcpkg\vcpkg install boost-system boost-thread openssl
  ```
  Luego agrega `-DCMAKE_TOOLCHAIN_FILE=...\vcpkg\scripts\buildsystems\vcpkg.cmake` al comando de CMake.

## Clonar
```bash
git clone https://github.com/Manum03/vittalize_c-
cd vittalize_c++
```

## Configurar y compilar
```bash
# Crear carpeta de build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compilar
cmake --build build --config Release
```

> Nota: CMake con FetchContent descargará Crow, SQLiteCpp y nlohmann/json. Asegura conexión a Internet la primera vez.

## Ejecutar
```bash
cd build
./vittalize_server          # Linux/macOS
# o
vittalize_server.exe        # Windows
```

El servidor se inicia en `http://localhost:18080/`. Abre esa URL en el navegador para usar la página web. La API principal está en `POST /api/evaluar` y el historial en `GET /api/evaluaciones`.

## Datos y base de datos
- La base SQLite se guarda en `data/vittalize.sqlite`. Si la carpeta no existe, el programa la crea.
- El frontend está en la carpeta `web/` y se sirve automáticamente por el servidor.

## Limpieza (opcional)
```bash
rm -rf build
rm -rf data/vittalize.sqlite
```
