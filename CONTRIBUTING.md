# 🤝 Contributing to MiniOS Kernel Simulator

Thanks for your interest in contributing! MiniOS is an educational OS kernel simulator, and we welcome contributions that improve the learning experience, add new OS algorithms, or enhance the visualization dashboard.

---

## 🏗️ Project Architecture

```
os-simulator/
├── engine/          # C++17 simulation engine
│   ├── core/        # Clock, EventBus, SimulationState
│   ├── modules/     # OS subsystems (process, scheduler, memory, sync)
│   └── bridge/      # REST API + WebSocket server (Crow)
├── dashboard/       # React + TypeScript frontend (Vite)
└── tests/           # Google Test suites + API integration tests
```

The engine is the single source of truth. The dashboard is display-only — it reads state via WebSocket and sends commands via REST.

---

## 🚀 Getting Started

### Prerequisites

| Tool | Version |
|------|---------|
| CMake | 3.20+ |
| MSVC / GCC / Clang | C++17 support |
| Node.js | 18+ |
| Git | Any recent version |

### Build & Run

```bash
# 1. Clone
git clone https://github.com/Lord-Saruman/OS-Kernel-Learning-Simulation.git
cd OS-Kernel-Learning-Simulation

# 2. Build the C++ engine
cd os-simulator && mkdir build && cd build
cmake ..
cmake --build . --config Release

# 3. Start the engine
./engine/Release/os_simulator.exe    # Windows
# or ./engine/os_simulator             # Linux/macOS

# 4. Start the dashboard (new terminal)
cd os-simulator/dashboard
npm install
npm run dev
```

Open **http://localhost:5173** in your browser.

---

## 🧪 Running Tests

Before submitting any PR, make sure all tests pass:

### Engine Tests (Google Test)
```bash
cd os-simulator/build
cmake --build . --config Release
ctest -C Release --output-on-failure
```
All **9** test suites must pass.

### API Integration Tests
With the engine running:
```powershell
cd os-simulator/tests/api
./test_api_workflows.ps1
```
All **39** assertions must pass.

---

## 📐 Code Style Guidelines

### C++ (Engine)
- **Standard**: C++17
- **Naming**: `PascalCase` for classes and methods, `camelCase` for local variables
- **Headers**: Use `.h` for headers, `.cpp` for implementation
- **RAII**: Prefer RAII over manual resource management
- **Const correctness**: Mark everything `const` that should be

### TypeScript (Dashboard)
- **Framework**: React with functional components and hooks
- **Styling**: Vanilla CSS (no Tailwind)
- **State**: React hooks (`useState`, `useEffect`, custom hooks)

---

## 🧩 Extending the Simulator

The Strategy Pattern makes adding new algorithms straightforward:

### Adding a New Scheduling Algorithm

1. Create a new class implementing `ISchedulingPolicy` in `engine/modules/scheduler/`
2. Register it in the scheduler's policy map
3. Add the enum value to `SimEnums.h`
4. The REST API and dashboard will pick it up automatically

### Adding a New Page Replacement Policy

1. Create a new class implementing `IReplacementPolicy` in `engine/modules/memory/`
2. Register it in the memory manager's policy map
3. Add the enum value to `SimEnums.h`

### Adding a New Dashboard Panel

1. Create a new component in `dashboard/src/components/`
2. Wire it to the WebSocket state in `App.tsx`

---

## 📝 Pull Request Process

1. **Fork** the repository and create a feature branch
2. **Write tests** for any new engine functionality
3. **Ensure** all existing tests pass
4. **Document** any new API endpoints or configuration options
5. **Submit** a PR with a clear description of your changes

---

## 💬 Questions?

Open an issue or reach out on GitHub. We're happy to help you get started!
