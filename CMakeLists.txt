cmake_minimum_required(VERSION 3.20)
project(MEC_Trainer LANGUAGES CXX C)

# Only allow 64-bit generators
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
file(GLOB_RECURSE TRAINER_SOURCES CONFIGURE_DEPENDS
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

# The ImGui demo window is never shown, so don't ship its code
target_compile_definitions(MEC_Trainer PRIVATE IMGUI_DISABLE_DEMO_WINDOWS)

target_link_libraries(MEC_Trainer PRIVATE
    d3d11
    dxgi
)

if(MSVC)
    target_compile_options(MEC_Trainer PRIVATE /guard:cf)
    target_link_options(MEC_Trainer PRIVATE /guard:cf)
endif()

# ==========================================
# 2. Standalone Smart Injector Target
# ==========================================
add_executable(MEC_Injector "Injector/Injector.cpp")
target_compile_features(MEC_Injector PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MEC_Injector PRIVATE /guard:cf)
    # MSVC's default manifest already runs the injector as the current user (asInvoker)
    target_link_options(MEC_Injector PRIVATE /guard:cf)
endif()

# ==========================================
# 3. Unit tests (pure logic that runs without the game)
# ==========================================
option(MEC_BUILD_TESTS "Build the unit tests" ON)
if(MEC_BUILD_TESTS)
    enable_testing()
    add_executable(MEC_Tests
        "tests/UnitTests.cpp"
        "src/Core/Config.cpp"
        "src/Core/Log.cpp"
    )
    target_include_directories(MEC_Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME MEC_Tests COMMAND MEC_Tests)
endif()
