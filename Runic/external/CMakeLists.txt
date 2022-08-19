find_package(Vulkan REQUIRED)

add_subdirectory(vk-bootstrap)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE vma/include)

add_library(SDL2 INTERFACE)

target_include_directories(SDL2 INTERFACE SDL2/include)
target_link_directories(SDL2 INTERFACE SDL2/lib/x64)
target_link_libraries(SDL2 INTERFACE SDL2.lib SDL2main.lib)

add_library(glm INTERFACE)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

target_include_directories(glm INTERFACE glm/glm)

add_subdirectory(tracy) # target: TracyClient or alias Tracy::TracyClient

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_sdl.cpp"
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2)

add_library(stb_image INTERFACE)

target_include_directories(stb_image INTERFACE stb_image)

add_subdirectory(spdlog)

add_library(tinyobjloader STATIC)

target_sources(tinyobjloader PRIVATE 
    tinyobjloader/tiny_obj_loader.h
    tinyobjloader/tiny_obj_loader.cc
    )

target_include_directories(tinyobjloader PUBLIC tinyobjloader)
