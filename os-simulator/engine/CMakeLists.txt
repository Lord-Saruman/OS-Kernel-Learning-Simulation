# engine/CMakeLists.txt
# Build configuration for the C++ Simulation Engine

# ── Include directories ──────────────────────────────────────
# All headers use paths relative to engine/ root
# e.g. #include "core/SimEnums.h", #include "modules/process/PCB.h"
set(ENGINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ── Core Library ─────────────────────────────────────────────
# Contains: SimulationState, EventBus, ClockController, SimEnums, 
#           SimEvent, DecisionLogEntry, ISimModule
add_library(core_lib
    core/EventBus.cpp
    core/ClockController.cpp
)
target_include_directories(core_lib PUBLIC ${ENGINE_INCLUDE_DIR})

# ── Thread support ───────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(core_lib PUBLIC Threads::Threads)

# ── Process Manager Library (Phase 2) ────────────────────────
# Contains: ProcessManager, ProcessSpec, PCB, TCB
add_library(process_manager_lib
    modules/process/ProcessManager.cpp
)
target_include_directories(process_manager_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(process_manager_lib PUBLIC core_lib)

# ── Main Executable ──────────────────────────────────────────
add_executable(os_simulator main.cpp)
target_link_libraries(os_simulator PRIVATE core_lib process_manager_lib scheduler_lib)

# ── Scheduler Library (Phase 3) ──────────────────────────────
# Contains: Scheduler, ISchedulingPolicy, FCFSPolicy, RoundRobinPolicy, PriorityPolicy
add_library(scheduler_lib
    modules/scheduler/Scheduler.cpp
)
target_include_directories(scheduler_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(scheduler_lib PUBLIC core_lib)

# ── Future: Module Libraries ─────────────────────────────────
# Phase 4: add_library(memory_manager_lib modules/memory/MemoryManager.cpp)
# Phase 5: add_library(sync_manager_lib modules/sync/SyncManager.cpp)
# Phase 7: add_library(bridge_lib bridge/RestServer.cpp bridge/WsServer.cpp)

# ── Future: External Dependencies (Phase 7) ──────────────────
# include(FetchContent)
#
# FetchContent_Declare(
#     crow
#     GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
#     GIT_TAG        v1.2.0
# )
#
# FetchContent_Declare(
#     nlohmann_json
#     GIT_REPOSITORY https://github.com/nlohmann/json.git
#     GIT_TAG        v3.11.3
# )
