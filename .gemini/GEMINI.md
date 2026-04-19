# Mini OS Kernel Simulator — Agent Instructions

## Project Identity
- **Name**: Mini OS Kernel Simulator
- **Type**: Academic CEP (Complex Engineering Problem) for CS-330 Operating Systems
- **Architecture**: C++17 Simulation Engine → REST/WebSocket API Bridge → React+TypeScript Dashboard
- **Status**: In active development

## Authoritative Documents (MUST READ before any code change)
1. `PRD_MiniOS_Simulator.md` — Product requirements, functional specs, success metrics
2. `SDD_MiniOS_Simulator.md` — System design, architecture, interfaces, concurrency model
3. `DataDictionary_MiniOS_Simulator.md` — Every struct, enum, and field. Code MUST match this exactly.

## Tech Stack (Non-Negotiable)
| Component | Technology | Version |
|-----------|-----------|---------|
| Engine | C++17 | C++17 standard |
| Threading | `std::thread`, `std::barrier`, `std::shared_mutex` | C++17/20 |
| HTTP Server | Crow | 1.x |
| WebSocket | websocketpp | 0.8.x |
| JSON | nlohmann/json | 3.x |
| Frontend | React + TypeScript | React 18 |
| Build (frontend) | Vite | 5.x |
| Charts | Recharts | 2.x |
| Build (engine) | CMake | 3.20+ |

## Repository Structure
```
os-simulator/
├── engine/                    # C++ Simulation Engine
│   ├── core/                  # SimulationState, EventBus, ClockController
│   ├── modules/
│   │   ├── process/           # Process Manager Module
│   │   ├── scheduler/         # Scheduler Module + policies/
│   │   ├── sync/              # Sync Manager Module
│   │   └── memory/            # Memory Manager Module + policies/
│   ├── bridge/                # API Bridge (REST + WebSocket)
│   └── CMakeLists.txt
├── dashboard/                 # React Frontend
│   ├── src/
│   │   ├── components/        # ProcessPanel, SchedulerPanel, SyncPanel, MemoryPanel, etc.
│   │   ├── hooks/             # useSimState.ts, useSimControl.ts
│   │   └── App.tsx
│   └── package.json
├── tests/                     # Per-module test suites
└── docs/                      # PRD, SDD, DataDictionary
```

## Build & Run Commands
```bash
# Engine (C++)
cd engine && mkdir build && cd build
cmake .. && cmake --build .
./os_simulator                 # Starts engine on REST:8080, WS:8081

# Dashboard (React)
cd dashboard
npm install
npm run dev                    # Starts on http://localhost:3000

# Run tests
cd engine/build && ctest
cd dashboard && npm test
```
