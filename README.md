# OS Kernel Learning Simulation

An interactive, visual simulation of an Operating System Kernel built with a C++ backend engine and a React/TypeScript frontend dashboard.

This project was built to visually demonstrate core OS kernel concepts, including Process Scheduling, Memory Management, Synchronization (Locks/Semaphores), and more.

## Features Completed
- **Phase 1-2: Process Manager** (PCB, States, Ready Queue)
- **Phase 3: CPU Scheduler** (FCFS, Round Robin, Priority, Gantt Chart)
- **Phase 4: Memory Manager** (Virtual Memory, Paging, FIFO/LRU replacement)
- **Phase 5: Sync Manager** (Mutexes, Semaphores, Race Condition Demos)
- **Phase 6: Clock Controller** (Tick-based simulation engine)
- **Phase 7: REST API Bridge** (Crow C++ WebServer with WebSockets)
- **Phase 8: React Dashboard** (Live visualizer of all OS internals)

## Prerequisites
- **C++ Build Tools**: CMake, MSVC (Visual Studio) or GCC/Clang.
- **Node.js**: v18+ to run the React dashboard.
- **Git**: To clone the repository.

## 🚀 How to Run the Project (Phase 7 & 8)

You need to run **two** separate components: the C++ backend engine and the React frontend dashboard. 

### Step 1: Build and Run the C++ Engine

Open a terminal and navigate to the project root:

```powershell
# Navigate to the engine build directory
cd os-simulator
mkdir build
cd build

# Generate build files and compile (Windows MSVC example)
cmake ..
cmake --build . --config Release

# Start the simulation backend
.\engine\Release\os_simulator.exe
```
> **Note:** The engine will start a REST server and WebSocket listener on `http://0.0.0.0:8080`. Leave this terminal running!

### Step 2: Run the React Dashboard

Open a **second** terminal and navigate to the dashboard directory:

```powershell
cd os-simulator\dashboard

# Install dependencies (first time only)
npm install

# Start the Vite development server
npm run dev
```

### Step 3: View the Simulation
Open your browser and navigate to **[http://localhost:5173](http://localhost:5173)**. 

From the dashboard, you can:
1. Load workloads (e.g., CPU bound, I/O bound, Mixed).
2. Change scheduling policies (FCFS, Round Robin, Priority).
3. Change memory policies (FIFO, LRU).
4. Run a live Race Condition / Synchronization demo.
5. Manually step through the CPU clock tick by tick, or set it to Auto.

---

## 🧪 Running Tests

The C++ engine is fully tested using **Google Test (gtest)**. The tests verify all modules, including the Process Manager, Scheduler, Memory Manager, Sync Manager, and the Clock Controller.

To run the test suite:

```powershell
# Navigate to the build directory
cd os-simulator\build

# Rebuild all targets (including tests)
cmake --build . --config Release

# Run CTest
ctest -C Release --output-on-failure
```

All 8 test suites should pass cleanly (100% success rate).

---

## Course Information

**Subject:** Operating Systems  
**Semester:** 4 — Spring 2026

## Author
Ameer — [GitHub](https://github.com/Lord-Saruman)
