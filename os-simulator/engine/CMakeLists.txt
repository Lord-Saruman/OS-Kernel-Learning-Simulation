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

# ── Scheduler Library (Phase 3) ──────────────────────────────
# Contains: Scheduler, ISchedulingPolicy, FCFSPolicy, RoundRobinPolicy, PriorityPolicy
add_library(scheduler_lib
    modules/scheduler/Scheduler.cpp
)
target_include_directories(scheduler_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(scheduler_lib PUBLIC core_lib)

# ── Memory Manager Library (Phase 4) ─────────────────────────
# Contains: MemoryManager, IReplacementPolicy, FIFOPolicy, LRUPolicy
add_library(memory_manager_lib
    modules/memory/MemoryManager.cpp
)
target_include_directories(memory_manager_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(memory_manager_lib PUBLIC core_lib)

# ── Sync Manager Library (Phase 5) ───────────────────────────
# Contains: SyncManager, Mutex, Semaphore, SyncRequest
add_library(sync_manager_lib
    modules/sync/SyncManager.cpp
)
target_include_directories(sync_manager_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(sync_manager_lib PUBLIC core_lib)

# ── External Dependencies (Phase 7) ─────────────────────────
include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-28-0
)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

# Fetch asio first so we can point Crow to it
FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)

# Disable Crow extras to speed up build
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(crow nlohmann_json)

# ── Bridge Library (Phase 7) ─────────────────────────────────
# Contains: RestServer (REST endpoints + WebSocket broadcaster)
add_library(bridge_lib
    bridge/RestServer.cpp
)
target_include_directories(bridge_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(bridge_lib PUBLIC
    core_lib
    process_manager_lib
    scheduler_lib
    memory_manager_lib
    sync_manager_lib
    Crow::Crow
    nlohmann_json::nlohmann_json
)

# ── Main Executable ──────────────────────────────────────────
add_executable(os_simulator main.cpp)
target_link_libraries(os_simulator PRIVATE bridge_lib)
