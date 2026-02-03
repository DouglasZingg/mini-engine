## Requirements
- Windows 10/11
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.20+
- vcpkg (global install)

## Dependencies
- SDL2 (via vcpkg manifest mode)

## Build (Windows)
```bat
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/Users/dougl/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
build\Debug\mini_engine.exe
