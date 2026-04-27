# tests/CMakeLists.txt
# Unit test configuration using Google Test

include(FetchContent)

# ── Fetch Google Test ────────────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Prevent overriding parent project compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ── Include directories ──────────────────────────────────────
set(ENGINE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/engine)

# ── Test: SimulationState ────────────────────────────────────
add_executable(test_simulation_state test_simulation_state.cpp)
target_include_directories(test_simulation_state PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_simulation_state
    PRIVATE
    GTest::gtest_main
    core_lib
)
add_test(NAME SimulationStateTests COMMAND test_simulation_state)

# ── Test: EventBus ───────────────────────────────────────────
add_executable(test_event_bus test_event_bus.cpp)
target_include_directories(test_event_bus PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_event_bus
    PRIVATE
    GTest::gtest_main
    core_lib
)
add_test(NAME EventBusTests COMMAND test_event_bus)

# ── Test: ProcessManager (Phase 2) ──────────────────────────
add_executable(test_process_manager test_process_manager.cpp)
target_include_directories(test_process_manager PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_process_manager
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
)
add_test(NAME ProcessManagerTests COMMAND test_process_manager)

# ── Test: Scheduler (Phase 3) ───────────────────────────────
add_executable(test_scheduler test_scheduler.cpp)
target_include_directories(test_scheduler PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_scheduler
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    scheduler_lib
)
add_test(NAME SchedulerTests COMMAND test_scheduler)

# ── Test: MemoryManager (Phase 4) ───────────────────────────
add_executable(test_memory_manager test_memory_manager.cpp)
target_include_directories(test_memory_manager PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_memory_manager
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    memory_manager_lib
)
add_test(NAME MemoryManagerTests COMMAND test_memory_manager)

# ── Test: SyncManager (Phase 5) ─────────────────────────────
add_executable(test_sync_manager test_sync_manager.cpp)
target_include_directories(test_sync_manager PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_sync_manager
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    sync_manager_lib
)
add_test(NAME SyncManagerTests COMMAND test_sync_manager)

# Phase 6: ClockController lifecycle and mode behavior
add_executable(test_clock_controller test_clock_controller.cpp)
target_include_directories(test_clock_controller PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_clock_controller
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    scheduler_lib
    memory_manager_lib
    sync_manager_lib
)
add_test(NAME ClockControllerTests COMMAND test_clock_controller)

# Phase 6: End-to-end integration across all 4 modules via ClockController
add_executable(test_engine_integration test_engine_integration.cpp)
target_include_directories(test_engine_integration PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_engine_integration
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    scheduler_lib
    memory_manager_lib
    sync_manager_lib
)
add_test(NAME EngineIntegrationTests COMMAND test_engine_integration)

# ── Test: Memory Compare — engine-only FIFO vs LRU (Phase 4 deep-dive) ──
# Locks in textbook reference-string fault counts and the full-pipeline
# behaviour without touching the API or the React dashboard. If any other
# layer reports a wrong page-fault count, run THIS suite first to confirm
# the engine itself is correct.
add_executable(test_memory_compare test_memory_compare.cpp)
target_include_directories(test_memory_compare PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_memory_compare
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    scheduler_lib
    memory_manager_lib
)
add_test(NAME MemoryCompareTests COMMAND test_memory_compare)
