cmake_minimum_required(VERSION 3.20)
project(mini_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_engine
    src/main.cpp
    src/core/App.cpp
    src/platform/SdlPlatform.cpp
)

target_include_directories(mini_engine PRIVATE src)

# SDL2 via vcpkg
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(mini_engine PRIVATE SDL2::SDL2 SDL2::SDL2main)

# Nice warnings (MSVC)
if (MSVC)
  target_compile_options(mini_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(mini_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
