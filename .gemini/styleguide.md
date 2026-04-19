# Coding Style Guide — Mini OS Kernel Simulator

## C++ Engine Rules

### Naming Conventions
- **Structs/Classes**: PascalCase → `ProcessManager`, `SimulationState`, `FCFSPolicy`
- **Member variables**: camelCase → `remainingBurst`, `arrivalTick`, `ownerPid`
- **Methods**: camelCase → `onTick()`, `selectNext()`, `getStatus()`
- **Enum values**: ALL_CAPS → `RUNNING`, `ROUND_ROBIN`, `LRU`
- **Constants**: ALL_CAPS with underscores → `MAX_PROCESSES`, `DEFAULT_QUANTUM`
- **File names**: PascalCase for headers/sources matching the class → `PCB.h`, `EventBus.h`, `ClockController.cpp`
- **Enum file**: All enums live in `engine/core/SimEnums.h` — never define enums elsewhere

### Struct/Class Rules
- Every struct field name MUST exactly match the Data Dictionary (`DataDictionary_MiniOS_Simulator.md`)
- Every struct MUST be defined in the Data Dictionary before it appears in code — no undocumented fields
- JSON keys use snake_case conversion of camelCase field names (e.g., `arrivalTick` → `arrival_tick`)
- Enum values serialise as uppercase strings (e.g., `ProcessState::RUNNING` → `"RUNNING"`)

### Module Architecture Rules (CRITICAL)
1. **No module may import or call another module directly.** All inter-module communication goes through `SimulationState` or `EventBus` only.
2. Every OS module MUST implement the `ISimModule` interface: `onTick()`, `reset()`, `getStatus()`, `getModuleName()`
3. Scheduling algorithms implement `ISchedulingPolicy` (strategy pattern). Page replacement algorithms implement `IReplacementPolicy`.
4. Adding a new policy MUST require zero changes outside its own module directory.
5. The `SimulationState` object is the single source of truth — no duplicated state anywhere.

### Threading & Synchronisation
- Use `std::shared_mutex` for SimulationState (multiple readers, one writer)
- Each module acquires scoped write locks only on the subsection it modifies
- EventBus uses a separate `std::mutex` for its subscription list
- Never hold locks across module boundaries
- Never use raw `new`/`delete` — use smart pointers or RAII

### Memory & Safety
- Use `std::unique_ptr` for owned resources, `std::shared_ptr` only when genuinely shared
- No raw pointer ownership
- All containers should be from `<vector>`, `<map>`, `<deque>`, `<string>` — no C-style arrays
- Prefer `const&` for function parameters that don't need mutation

### Error Handling
- No exceptions across module boundaries — use return codes or std::optional
- Log errors to the EventBus as `SimEvent` entries with descriptive messages
- Never silently swallow errors

### Comments & Documentation
- Every public method must have a brief doc comment explaining purpose
- Every struct must reference its Data Dictionary section
- Complex algorithms (scheduling, page replacement) must have inline comments explaining steps

---

## React/TypeScript Dashboard Rules

### Naming Conventions
- **Components**: PascalCase → `ProcessPanel`, `SchedulerPanel`, `SimControlBar`
- **Hooks**: camelCase with `use` prefix → `useSimState`, `useSimControl`
- **Files**: Match component name → `ProcessPanel.tsx`, `useSimState.ts`
- **CSS classes**: kebab-case → `process-card`, `gantt-chart`, `ready-queue`

### State Management
- The frontend holds ZERO simulation state — all state comes from the engine via WebSocket
- Use `useReducer` with a single `STATE_UPDATE` action that replaces the entire display state
- No Redux, Zustand, or external state libraries
- User actions call REST endpoints — they never mutate local state directly

### Component Rules
- Each OS subsystem panel is its own component directory under `src/components/`
- Components receive state slices as props from `App.tsx`
- No component may make direct WebSocket calls — that is App's responsibility
- Every interactive element must have a unique, descriptive `id` attribute

### TypeScript
- Strict mode enabled — no `any` types
- Define interfaces for all data shapes matching the engine's JSON output
- All WebSocket message payloads must be typed

### Styling
- Use vanilla CSS (no Tailwind unless explicitly requested)
- Dark theme preferred — use CSS custom properties for theming
- Animations must be smooth (60fps) — use CSS transitions/transforms, not JS animation
- Responsive layout that works on desktop Chrome and Firefox

---

## General Rules for All Code

### Git Workflow
- One branch per module (e.g., `feat/process-manager`, `feat/scheduler`, `feat/dashboard`)
- Commits must reference the module: `[ProcessMgr] Add PCB creation logic`
- Never commit directly to main — use PRs

### Testing
- Every OS module must have isolated unit tests that run without other modules
- Scheduler metrics must match hand-computed reference values within 1% margin
- Page replacement outputs must match textbook examples exactly
- Test files go in `tests/` with naming pattern `test_<module>.cpp`
