cmake_minimum_required(VERSION 3.0.0)

project(ImGui-SharedDrawData LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(
    modules
    third_party
    third_party/imgui
    third_party/imgui/examples/libs/glfw/include/
)

# Scan imgui sources
aux_source_directory(third_party/imgui IMGUI_SOURCES)
# Make imgui backends sources
set(
    IMGUI_BACKENDS_SOURCES
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

# Build canvas program
add_executable(canvas src/canvas.cc ${IMGUI_SOURCES} ${IMGUI_BACKENDS_SOURCES})
target_link_directories(canvas PRIVATE third_party/imgui/examples/libs/glfw/lib-vc2010-64)
target_link_libraries(canvas glfw3 opengl32 legacy_stdio_definitions ws2_32)

# Build render program
add_executable(render src/render.cc ${IMGUI_SOURCES} ${IMGUI_BACKENDS_SOURCES})
target_link_directories(render PRIVATE third_party/imgui/examples/libs/glfw/lib-vc2010-64)
target_link_libraries(render glfw3 opengl32 legacy_stdio_definitions ws2_32)
