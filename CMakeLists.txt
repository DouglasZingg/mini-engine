cmake_minimum_required(VERSION 3.20)
project(mini_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_engine
    src/main.cpp
    src/core/App.cpp
    src/platform/SdlPlatform.cpp
    src/platform/SdlTexture.cpp
    src/engine/Assets.cpp
    src/engine/Config.cpp
    src/engine/Input.cpp
    src/game/Game.cpp
    src/engine/DebugUI.cpp
    third_party/imgui/imgui_impl_sdl2.cpp
    third_party/imgui/imgui_impl_sdlrenderer2.cpp
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_impl_sdl2.cpp
    third_party/imgui/imgui_impl_sdlrenderer2.cpp
    third_party/imgui/imstb_rectpack.h
    third_party/imgui/imstb_textedit.h
    third_party/imgui/imstb_truetype.h

)

target_include_directories(mini_engine PRIVATE src)

# SDL2 via vcpkg (manifest mode)
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(mini_engine PRIVATE SDL2::SDL2 SDL2::SDL2main)

target_include_directories(mini_engine PRIVATE third_party/imgui)

# Nice warnings
if (MSVC)
  target_compile_options(mini_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(mini_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
