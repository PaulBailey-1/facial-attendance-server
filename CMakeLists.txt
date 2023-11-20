cmake_minimum_required(VERSION 3.18)

project(Facial-Attendence-Server)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(SRCS
    src/main.cpp
    src/DBConnection.cpp
)

include_directories(include/)

find_package(Boost REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )
find_package(OpenSSL REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_executable(faserver ${SRCS})
target_link_libraries( faserver OpenSSL::SSL fmt::fmt)
target_compile_features( faserver PRIVATE cxx_std_17)
