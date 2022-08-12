cmake_minimum_required(VERSION 3.5)
project(runic_engine VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(engine)

##file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")
##file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "src/*.h")
##add_executable(${PROJECT_NAME} ${SRC_FILES})

##target_link_libraries(${PROJECT_NAME} runic_engine)
##target_include_directories(${PROJECT_NAME} PRIVATE ${HEADER_FILES}) 
##target_compile_definitions(${PROJECT_NAME} PRIVATE RNC_PLATFORM_WINDOWS)
