cmake_minimum_required(VERSION 3.20)
project(MEC_Trainer LANGUAGES CXX C)

# Mirror's Edge Catalyst is a 64-bit process. Building an x86 DLL (or an x86
# injector) produces a binary that may look valid to the injector but cannot
# execute the trainer inside the game.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "MEC_Trainer must be configured for a 64-bit generator (for example: cmake -A x64 ...)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static MSVC runtime (/MT) for zero redistributable dependencies
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ==========================================
# 1. Trainer DLL Target
# ==========================================
file(GLOB_RECURSE TRAINER_SOURCES 
    "src/*.cpp" 
    "src/*.c"
)

add_library(MEC_Trainer SHARED ${TRAINER_SOURCES})

target_include_directories(MEC_Trainer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kiero
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MinHook
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kiero/minhook/include
)

target_link_libraries(MEC_Trainer PRIVATE
    d3d11
    dxgi
)

# ==========================================
# 2. Standalone Smart Injector Target
# ==========================================
add_executable(MEC_Injector "Injector/Injector.cpp")
target_compile_features(MEC_Injector PRIVATE cxx_std_20)
