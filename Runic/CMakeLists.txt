##cmake_minimum_required(VERSION 3.5)
##project(runic_engine VERSION 0.0.1 LANGUAGES CXX)
##
##set(CMAKE_CXX_STANDARD 20)

# library find functions
find_package(Vulkan REQUIRED)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "src/*.h")
file(GLOB_RECURSE RNC_FILES CONFIGURE_DEPENDS "*.h")

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.comp"
    )

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES} ${RNC_FILES}  ${GLSL_SOURCE_FILES} main.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE RNC_PLATFORM_WINDOWS RNC_BUILD_DLL)
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SRC_FILES} ${HEADER_FILES} ${RNC_FILES} main.cpp)
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${GLSL_SOURCE_FILES})

# add external libraries
add_subdirectory(external)
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(external/tinygltf)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${Vulkan_INCLUDE_DIRS})
##target_link_libraries(${PROJECT_NAME} PUBLIC glm vk-bootstrap vma imgui stb_image spdlog tinyobjloader ${Vulkan_LIBRARIES} SDL2 TracyClient)
target_link_libraries(${PROJECT_NAME}  vk-bootstrap vma glm imgui stb_image spdlog tinyobjloader tinygltf EnTT)
target_link_libraries(${PROJECT_NAME}  Vulkan::Vulkan SDL2 Tracy::TracyClient)

## Shader compiler CMAKE code thanks to VBlanco: https://vkguide.dev/
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

add_dependencies(${PROJECT_NAME} Shaders)


   