cmake_minimum_required(VERSION 3.6)
project(RnNoisePluginCommon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(COMMON_SRC
        include/common/RnNoiseCommonPlugin.h
        src/RnNoiseCommonPlugin.cpp)

add_library(RnNoisePluginCommon STATIC ${COMMON_SRC})

target_link_libraries(RnNoisePluginCommon RnNoise)

target_include_directories(RnNoisePluginCommon PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

install(TARGETS RnNoisePluginCommon
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})