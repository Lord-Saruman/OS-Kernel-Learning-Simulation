cmake_minimum_required(VERSION 3.20)
project(MiniOSSimulator
    VERSION 1.0.0
    DESCRIPTION "Mini OS Kernel Simulator — CS-330 CEP"
    LANGUAGES CXX
)

# ── C++ Standard ──────────────────────────────────────────────
# Using C++20 for std::barrier (needed in Phase 6 for tick synchronisation)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler Warnings ────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ── Build Options ────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)

# ── Subdirectories ───────────────────────────────────────────
add_subdirectory(engine)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
