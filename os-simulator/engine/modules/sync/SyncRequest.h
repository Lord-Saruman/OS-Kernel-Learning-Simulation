#pragma once

/**
 * SyncRequest.h — Synchronization Operation Request
 *
 * Represents a pending synchronization operation (acquire, release,
 * wait, signal) submitted by a process. Requests are queued in
 * SimulationState::pendingSyncRequests and drained each tick by the
 * SyncManager.
 *
 * In Phase 7, the API Bridge will enqueue these via REST endpoints.
 * In Phase 5, they are injected programmatically via SyncManager API
 * or directly by tests.
 *
 * DEFERRED: Thread-level sync (TCB.blockedOnSyncId) is not handled
 * in Phase 5. A future `tid` field (default -1) would extend this
 * to thread granularity. See implementation_plan.md Deferred Items §1.
 */

#include <string>
#include "core/SimEnums.h"

struct SyncRequest {
    int               pid;          // PID of the requesting process
    std::string       operation;    // "ACQUIRE", "RELEASE", "WAIT", "SIGNAL"
    SyncPrimitiveType type;         // MUTEX, SEMAPHORE_BINARY, SEMAPHORE_COUNTING
    int               resourceId;   // mutexId or semId

    // ── Default Constructor ──────────────────────────────────
    SyncRequest()
        : pid(-1)
        , operation("")
        , type(SyncPrimitiveType::MUTEX)
        , resourceId(-1)
    {}

    SyncRequest(int p, const std::string& op, SyncPrimitiveType t, int resId)
        : pid(p)
        , operation(op)
        , type(t)
        , resourceId(resId)
    {}
};
