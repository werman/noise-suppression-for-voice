cmake_minimum_required(VERSION 3.6)
project(RnNoisePluginCommon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(COMMON_SRC
        include/common/RnNoiseCommonPlugin.h
        src/RnNoiseCommonPlugin.cpp)

add_library(RnNoisePluginCommon STATIC ${COMMON_SRC})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SAMPLERATE REQUIRED samplerate)
target_link_libraries(RnNoisePluginCommon ${SAMPLERATE_LIBRARIES})
target_include_directories(RnNoisePluginCommon PUBLIC ${SAMPLERATE_INCLUDE_DIRS})
target_compile_options(RnNoisePluginCommon PUBLIC ${SAMPLERATE_CFLAGS_OTHER})

target_link_libraries(RnNoisePluginCommon RnNoise)

target_include_directories(RnNoisePluginCommon PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)
