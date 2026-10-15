# File: research/loop-learning/CMakeLists.txt
# Builds the research tools (not the plugin) with any compiler, incl. Visual Studio on Windows:
#   cmake -S research/loop-learning -B build-research
#   cmake --build build-research --config Release
# -> extract_long (F4 full-length pictures), extract (fast-model features), eval (per-type scores)
cmake_minimum_required(VERSION 3.22)
project(DrumMasterResearch LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
foreach(tool extract_long ../classifier-train/extract ../classifier-eval/eval)
    get_filename_component(name ${tool} NAME)
    add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${tool}.cpp)
    target_include_directories(${name} PRIVATE ${ROOT}/dsp/include ${ROOT}/research/classifier-eval)
    if(MSVC)
        target_compile_options(${name} PRIVATE /O2 /bigobj /D_CRT_SECURE_NO_WARNINGS)
    endif()
endforeach()
