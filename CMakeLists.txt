cmake_minimum_required(VERSION 3.15)
project(boost_project CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Boost)
find_package(Threads)

add_executable(${PROJECT_NAME} src/server.cpp)

target_link_libraries(${PROJECT_NAME} boost::boost Threads::Threads) 
