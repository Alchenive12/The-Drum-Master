# File: dsp/CMakeLists.txt
#
# Pure C++ DSP core — NO JUCE in this folder.
# Builds two ways with the same file:
#   1. Standalone (Claude's sandbox, or any machine):
#        cmake -S dsp -B build-dsp -DPLUGIN_DSP_SANITIZE=ON && cmake --build build-dsp && ctest --test-dir build-dsp
#   2. As a subdirectory of the plugin project:
#        add_subdirectory(dsp)  +  target_link_libraries(<Plugin> PRIVATE plugin_dsp)
#
# Layout:
#   dsp/include/plugin_dsp/*.h   public headers (header-only is fine)
#   dsp/src/*.cpp                optional sources (library becomes STATIC if any exist)
#   dsp/tests/test_*.cpp         each file = one test executable with its own main()

cmake_minimum_required(VERSION 3.22)
project(plugin_dsp LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug)
    endif()
    enable_testing()
endif()

option(PLUGIN_DSP_TESTS    "Build DSP unit tests"                      ON)
option(PLUGIN_DSP_SANITIZE "Build tests with ASan + UBSan (GCC/Clang)" OFF)
option(PLUGIN_DSP_WERROR   "Treat warnings as errors in tests"         ${PROJECT_IS_TOP_LEVEL})

# ---- Library ---------------------------------------------------------------
file(GLOB PLUGIN_DSP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(PLUGIN_DSP_SOURCES)
    add_library(plugin_dsp STATIC ${PLUGIN_DSP_SOURCES})
    set(_dsp_scope PUBLIC)
else()
    add_library(plugin_dsp INTERFACE)
    set(_dsp_scope INTERFACE)
endif()

target_include_directories(plugin_dsp ${_dsp_scope} "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(plugin_dsp ${_dsp_scope} cxx_std_17)

# ---- Warnings (applied to tests; the plugin keeps its own flags) -----------
add_library(plugin_dsp_warnings INTERFACE)
if(MSVC)
    target_compile_options(plugin_dsp_warnings INTERFACE /W4 /permissive-)
    if(PLUGIN_DSP_WERROR)
        target_compile_options(plugin_dsp_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(plugin_dsp_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
        -Wdouble-promotion -Wfloat-equal -Wnull-dereference)
    if(PLUGIN_DSP_WERROR)
        target_compile_options(plugin_dsp_warnings INTERFACE -Werror)
    endif()
endif()

if(PLUGIN_DSP_SANITIZE AND NOT MSVC)
    target_compile_options(plugin_dsp_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(plugin_dsp_warnings INTERFACE -fsanitize=address,undefined)
endif()

if(PLUGIN_DSP_SOURCES)
    target_link_libraries(plugin_dsp PRIVATE plugin_dsp_warnings)
endif()

# ---- Tests -----------------------------------------------------------------
if(PLUGIN_DSP_TESTS)
    file(GLOB PLUGIN_DSP_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")
    foreach(test_src IN LISTS PLUGIN_DSP_TEST_SOURCES)
        get_filename_component(test_name "${test_src}" NAME_WE)
        add_executable(${test_name} "${test_src}")
        target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(${test_name} PRIVATE plugin_dsp plugin_dsp_warnings)
        add_test(NAME ${test_name} COMMAND ${test_name})
        # Sanitizer builds are unoptimised and ~30x slower; CI (Release) keeps the 60 s limit.
        if(PLUGIN_DSP_SANITIZE)
            set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
        else()
            set_tests_properties(${test_name} PROPERTIES TIMEOUT 60)
        endif()
    endforeach()
endif()
