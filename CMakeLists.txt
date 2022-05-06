cmake_minimum_required(VERSION 3.15)
project(time_server CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(${CMAKE_BINARY_DIR}/cmake-build-release/conanbuildinfo.cmake)
conan_basic_setup()


add_executable(${PROJECT_NAME} src/server.cpp)

target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS}) 
