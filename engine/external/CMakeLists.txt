find_package(Vulkan REQUIRED)

add_library(vk-bootstrap STATIC)

target_sources(vk-bootstrap PRIVATE 
    vk-bootstrap/src/VkBootstrap.h
    vk-bootstrap/src/VkBootstrap.cpp
    )

target_include_directories(vk-bootstrap PUBLIC vk-bootstrap/src)
target_link_libraries(vk-bootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)

add_library(SDL2 INTERFACE)

target_include_directories(SDL2 INTERFACE SDL2/include)
target_link_directories(SDL2 INTERFACE SDL2/lib/x64)
target_link_libraries(SDL2 INTERFACE SDL2.lib SDL2main.lib)

add_library(vma INTERFACE)
add_library(glm INTERFACE)

target_include_directories(vma INTERFACE vma/include)
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

add_library(spdlog INTERFACE)

target_include_directories(spdlog INTERFACE spdlog/include)

add_library(tinyobjloader STATIC)

target_sources(tinyobjloader PRIVATE 
    tinyobjloader/tiny_obj_loader.h
    tinyobjloader/tiny_obj_loader.cc
    )

target_include_directories(tinyobjloader PUBLIC tinyobjloader)
