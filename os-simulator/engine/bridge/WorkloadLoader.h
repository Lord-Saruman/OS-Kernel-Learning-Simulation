#pragma once

/**
 * WorkloadLoader.h — Prebuilt Workload Scenario Loader
 *
 * Reference: DataDictionary §8.3 (WorkloadScenario)
 *            SDD §5.2 (POST /workload/load)
 *
 * Generates a set of ProcessSpecs for predefined scenarios.
 * Used by the API Bridge to populate the simulation with a
 * realistic mix of processes on a single REST call.
 */

#include <string>
#include <vector>
#include <stdexcept>

#include "modules/process/ProcessSpec.h"
#include "core/SimEnums.h"

namespace WorkloadLoader {

/**
 * Generate ProcessSpecs for the given scenario name.
 *
 * Scenarios (from DataDictionary §8.3):
 *   "cpu_bound" — 5 CPU_BOUND processes
 *   "io_bound"  — 5 IO_BOUND processes
 *   "mixed"     — 3 CPU + 3 IO + 2 MIXED (8 total)
 *
 * @param scenario  Scenario name (case-sensitive)
 * @return Vector of ProcessSpecs ready to pass to ProcessManager::createProcess
 * @throws std::invalid_argument if scenario name is unknown
 */
inline std::vector<ProcessSpec> loadScenario(const std::string& scenario) {
    std::vector<ProcessSpec> specs;

    if (scenario == "cpu_bound") {
        // 5 CPU_BOUND processes — stresses the scheduler
        for (int i = 0; i < 5; ++i) {
            ProcessSpec s;
            s.name = "cpu_" + std::to_string(i + 1);
            s.type = ProcessType::CPU_BOUND;
            s.priority = (i % 5) + 1;  // priorities 1-5
            s.cpuBurst = 8 + (i * 2);  // 8, 10, 12, 14, 16
            s.ioBurstDuration = 0;
            s.memoryRequirement = 2 + i; // 2, 3, 4, 5, 6
            specs.push_back(s);
        }
    }
    else if (scenario == "io_bound") {
        // 5 IO_BOUND processes — high wait times, low CPU util
        for (int i = 0; i < 5; ++i) {
            ProcessSpec s;
            s.name = "io_" + std::to_string(i + 1);
            s.type = ProcessType::IO_BOUND;
            s.priority = (i % 5) + 1;
            s.cpuBurst = 4 + i;          // 4, 5, 6, 7, 8
            s.ioBurstDuration = 3 + i;   // 3, 4, 5, 6, 7
            s.memoryRequirement = 2 + i;
            specs.push_back(s);
        }
    }
    else if (scenario == "mixed") {
        // 3 CPU_BOUND
        for (int i = 0; i < 3; ++i) {
            ProcessSpec s;
            s.name = "cpu_" + std::to_string(i + 1);
            s.type = ProcessType::CPU_BOUND;
            s.priority = i + 2;           // 2, 3, 4
            s.cpuBurst = 10 + (i * 3);   // 10, 13, 16
            s.ioBurstDuration = 0;
            s.memoryRequirement = 3 + i;
            specs.push_back(s);
        }
        // 3 IO_BOUND
        for (int i = 0; i < 3; ++i) {
            ProcessSpec s;
            s.name = "io_" + std::to_string(i + 1);
            s.type = ProcessType::IO_BOUND;
            s.priority = i + 3;           // 3, 4, 5
            s.cpuBurst = 5 + i;          // 5, 6, 7
            s.ioBurstDuration = 3 + i;   // 3, 4, 5
            s.memoryRequirement = 2 + i;
            specs.push_back(s);
        }
        // 2 MIXED
        for (int i = 0; i < 2; ++i) {
            ProcessSpec s;
            s.name = "mix_" + std::to_string(i + 1);
            s.type = ProcessType::MIXED;
            s.priority = i + 4;           // 4, 5
            s.cpuBurst = 8 + (i * 2);    // 8, 10
            s.ioBurstDuration = 2 + i;   // 2, 3
            s.memoryRequirement = 3 + i;
            specs.push_back(s);
        }
    }
    else {
        throw std::invalid_argument("Unknown workload scenario: " + scenario);
    }

    return specs;
}

} // namespace WorkloadLoader
