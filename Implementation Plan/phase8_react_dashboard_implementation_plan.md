# Phase 8 — React Dashboard Implementation Plan

## Overview

Phase 8 builds the React Dashboard (Layer 3 from SDD §6). The frontend holds **zero** simulation state — all state lives in the C++ engine. The React app connects via WebSocket to receive live state, and sends user commands via REST.

**The UI is styled to resemble a Windows 11 desktop** — with draggable/resizable windows, a taskbar, acrylic/glassmorphism effects, and Segoe UI typography.

> [!IMPORTANT]
> The frontend is a **read-only view** of engine state. User actions call REST endpoints — they do not mutate local state directly. (SDD §6.1)

---

## Technology Stack (SDD §8)

| Technology | Version | Purpose |
|-----------|---------|---------|
| React | 18 | Component model mapping to OS subsystem panels |
| TypeScript | 5.x | Type safety for state shape |
| Vite | 5.x | Fast dev server with HMR |
| Recharts | 2.x | Gantt chart, line/bar charts |
| react-rnd | 10.x | Draggable + resizable window panels |
| lucide-react | latest | Fluent-style icons |
| Vanilla CSS | — | Full control for Windows 11 aesthetic |

**No Tailwind, no Redux/Zustand, no Electron.**

---

## State Management (SDD §6.3)

```typescript
// Single display state — replaced wholesale on every WebSocket tick
const [simState, dispatch] = useReducer(simReducer, initialState);

useEffect(() => {
  const ws = new WebSocket('ws://localhost:8080/ws');
  ws.onmessage = (event) => {
    dispatch({ type: 'STATE_UPDATE', payload: JSON.parse(event.data) });
  };
  return () => ws.close();
}, []);
```

No external state library needed — the state is exactly the last WebSocket packet received.

---

## Component Tree (SDD §6.2)

| Component | Responsibility |
|-----------|----------------|
| `App` (Root) | WebSocket connection manager. Holds single display state. Distributes state slices to child panels. |
| `Desktop` | Full-screen dark desktop background. Renders array of `DesktopWindow`. Z-index management. |
| `DesktopWindow` | Windows 11 title bar + minimize/maximize/close. Draggable + resizable via react-rnd. Glassmorphism body. |
| `Taskbar` | Fixed bottom bar ($\approx$ 48px). Start button, window icons, sim status, tick counter, clock. |
| `SimControlBar` | Start/Pause/Step/Reset buttons. Mode toggle. Speed slider. Policy dropdowns. Quantum input. |
| `ProcessPanel` | Process table with state-colored badges. Create/Kill buttons. PCB detail on hover. |
| `SchedulerPanel` | Ready queue visualization. Live Gantt chart (Recharts). Metrics table. |
| `MemoryPanel` | Frame table grid. Per-process page tables. Fault rate chart. |
| `SyncPanel` | Mutex/semaphore status cards. Blocked queues. Race condition demo toggle. |
| `DecisionLogPanel` | Scrollable real-time log. Color-coded by module. Auto-scroll to bottom. |

---

## Hooks

### `useSimState.ts`
- Connects to `ws://localhost:8080/ws`
- Auto-reconnect with exponential backoff (1s, 2s, 4s, max 16s)
- Returns typed `SimState` object matching the JSON packet from Phase 7
- Single `useReducer` dispatch replaces entire state on each tick (SDD §6.3)

### `useSimControl.ts`
- Thin `fetch()` wrappers for all 12 REST endpoints
- Returns object with methods: `start()`, `pause()`, `reset()`, `step()`, `setMode()`, `setSpeed()`, `createProcess()`, `killProcess()`, `setSchedulerPolicy()`, `setQuantum()`, `setMemoryPolicy()`, `loadWorkload()`
- Base URL: `http://localhost:8080`

---

## Windows 11 Design System (CSS)

### Design Tokens
```css
:root {
  /* Mica / Acrylic materials */
  --bg-desktop: #1a1a2e;
  --bg-window: rgba(32, 32, 48, 0.85);
  --bg-titlebar: rgba(40, 40, 60, 0.9);
  --bg-taskbar: rgba(28, 28, 44, 0.92);

  /* Accent */
  --accent: #60a5fa;
  --accent-hover: #93c5fd;

  /* Text */
  --text-primary: #f0f0f0;
  --text-secondary: #a0a0b8;

  /* Borders & Shadows */
  --border-subtle: rgba(255, 255, 255, 0.08);
  --shadow-window: 0 8px 32px rgba(0, 0, 0, 0.4);

  /* Radius */
  --radius-window: 8px;
  --radius-button: 6px;

  /* Font */
  --font-family: 'Segoe UI Variable', 'Segoe UI', system-ui, sans-serif;
}
```

### Key Visual Effects
- `backdrop-filter: blur(20px)` on windows and taskbar
- Subtle border glow on focused window
- Smooth transitions (200ms ease) on state changes
- Process state badges with pulse animation for RUNNING
- Dark mode throughout

---

## Files

### New Directory: `os-simulator/dashboard/`

| Path | Purpose |
|------|---------|
| `src/App.tsx` | Root — WebSocket connection, state distribution |
| `src/index.css` | Design system tokens + global styles |
| `src/types/SimState.ts` | TypeScript interfaces matching JSON packet |
| `src/hooks/useSimState.ts` | WebSocket connection + state reducer |
| `src/hooks/useSimControl.ts` | REST command wrappers |
| `src/components/Desktop.tsx` | Desktop background + window manager |
| `src/components/DesktopWindow.tsx` | Draggable/resizable window chrome |
| `src/components/Taskbar.tsx` | Bottom taskbar |
| `src/components/SimControlBar.tsx` | Simulation controls panel |
| `src/components/ProcessPanel.tsx` | Process table + create/kill |
| `src/components/SchedulerPanel.tsx` | Ready queue + Gantt chart + metrics |
| `src/components/MemoryPanel.tsx` | Frame table + page tables + fault chart |
| `src/components/SyncPanel.tsx` | Mutex/semaphore status + blocked queues |
| `src/components/DecisionLogPanel.tsx` | Real-time decision log |

### Unchanged Files
All C++ engine code remains untouched.

---

## Data Flow

```
User clicks "Step" button in SimControlBar
  → useSimControl.step() sends POST /sim/step
  → C++ RestServer routes to clock.requestStep()
  → ClockController advances one tick
  → All modules run onTick()
  → EventBus publishes TICK_ADVANCED
  → RestServer WebSocket handler serializes state
  → WebSocket broadcasts to all clients
  → useSimState receives packet, dispatches STATE_UPDATE
  → React re-renders all panels with new state
```

---

## Verification Plan

### Dev Server
```powershell
cd os-simulator/dashboard
npm run dev
# Opens http://localhost:5173
```

### Test Checklist
1. Start sim → status changes to RUNNING
2. Step 10 ticks → Gantt chart shows 10 bars
3. Switch to AUTO → live tick stream
4. Change scheduling policy → immediate effect
5. Create new process → appears in process table
6. Kill a process → removed from ready queue
7. Load workload (mixed) → 8 processes appear
8. Reset → everything clears
9. WebSocket reconnects after engine restart
10. UI renders correctly in Chrome and Firefox
