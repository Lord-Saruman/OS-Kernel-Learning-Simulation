#pragma once

/**
 * DecisionLogEntry.h — Decision Log Entry for UI Annotation
 *
 * Reference: SDD Section 5.3 WebSocket packet (decision_log field)
 * Stored in: SimulationState::decisionLog
 *
 * NOTE: This struct is NOT in the Data Dictionary. Per Data Dictionary
 * Rule #2, this document should be updated to include it. This struct
 * is derived from the SDD WebSocket broadcast packet format.
 */

#include <string>
#include <cstdint>

struct DecisionLogEntry {
    uint64_t    tick;       // Clock tick of the decision
    std::string message;    // Plain-English explanation of the decision

    // ── Default Constructor ──────────────────────────────────
    DecisionLogEntry()
        : tick(0)
        , message("")
    {}

    DecisionLogEntry(uint64_t t, const std::string& msg)
        : tick(t)
        , message(msg)
    {}
};
