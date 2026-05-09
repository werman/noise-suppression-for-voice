cmake_minimum_required(VERSION 3.10)
project(RnNoisePluginCommon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(COMMON_SRC
        include/common/RnNoiseCommonPlugin.h
        src/RnNoiseCommonPlugin.cpp)

add_library(RnNoisePluginCommon STATIC ${COMMON_SRC})

set(LIBRARIES RnNoise)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    list(APPEND LIBRARIES atomic)
endif()

target_link_libraries(RnNoisePluginCommon ${LIBRARIES})

target_include_directories(RnNoisePluginCommon PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

if (BUILD_AUDIO_EVAL_TOOL)
    find_package(Threads REQUIRED)
    add_executable(rnnoise_audio_eval
            src/tools/audio_eval.cpp)
    target_link_libraries(rnnoise_audio_eval PRIVATE RnNoisePluginCommon Threads::Threads)
endif ()

if (BUILD_TESTS)
    set(TESTS_SRC
            src/tests/tests.cpp
            ${COMMON_SRC})
    add_executable(common_plugin_tests ${TESTS_SRC})
    target_include_directories(common_plugin_tests PRIVATE
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external/catch2>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(common_plugin_tests PRIVATE ${LIBRARIES})
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    check_cxx_source_compiles("int main() { return 0; }" RNNOISE_HAS_UBSAN)
    unset(CMAKE_REQUIRED_FLAGS)
    if (RNNOISE_HAS_UBSAN)
        target_compile_options(common_plugin_tests PRIVATE -fsanitize=undefined)
        target_link_libraries(common_plugin_tests PRIVATE -fsanitize=undefined)
    endif ()

    include(CTest)
    include(Catch)
    catch_discover_tests(common_plugin_tests)
endif ()
