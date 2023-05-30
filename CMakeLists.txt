cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(GMBUSLibDemo LANGUAGES CXX)

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

message ("cxx Flags:" ${CMAKE_CXX_FLAGS})

set(OSBitness 32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OSBitness 64)
endif()

set (FullOutputDir "${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}${OSBitness}/${CMAKE_BUILD_TYPE}")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${FullOutputDir}/static libs")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FullOutputDir}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FullOutputDir}")

include_directories(.)
add_Library(GMBUSLib STATIC ModbusDriver.cpp ModBusBuffer.cpp ModbusDriverTypes.cpp)
if(WIN32)
  target_link_libraries(GMBUSLib wsock32 ws2_32)
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(GMBUSLibDemo GMBUSLib)
