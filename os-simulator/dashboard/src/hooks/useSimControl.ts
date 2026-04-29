/**
 * useSimControl.ts — REST command hooks for simulation control
 *
 * Reference: SDD §5.2 (REST API Command Endpoints)
 *
 * Thin fetch() wrappers for all 12 REST endpoints.
 * The frontend sends commands via REST — never via WebSocket.
 *
 * All requests go to relative paths so the Vite dev-proxy can
 * forward them to the C++ engine on port 8080.
 */

const BASE = '';

async function post(path: string, body: Record<string, unknown> = {}) {
  try {
    const resp = await fetch(`${BASE}${path}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });

    if (!resp.ok) {
      const err = await resp.json().catch(() => ({ error: resp.statusText }));
      console.error(`[API] POST ${path} → ${resp.status}:`, err);
      return { ok: false, ...err };
    }

    return resp.json();
  } catch (e) {
    console.error(`[API] POST ${path} network error:`, e);
    return { ok: false, error: String(e) };
  }
}

async function get(path: string) {
  try {
    const resp = await fetch(`${BASE}${path}`);
    if (!resp.ok) {
      const err = await resp.json().catch(() => ({ error: resp.statusText }));
      console.error(`[API] GET ${path} → ${resp.status}:`, err);
      return { ok: false, ...err };
    }
    return resp.json();
  } catch (e) {
    console.error(`[API] GET ${path} network error:`, e);
    return { ok: false, error: String(e) };
  }
}

export function useSimControl() {
  return {
    start: () => post('/sim/start'),
    pause: () => post('/sim/pause'),
    reset: () => post('/sim/reset'),
    step: () => post('/sim/step'),
    setMode: (mode: string) => post('/sim/mode', { mode }),
    setSpeed: (ms: number) => post('/sim/speed', { ms }),

    createProcess: (spec: {
      name?: string;
      type?: string;
      priority?: number;
      cpu_burst?: number;
      io_burst_duration?: number;
      memory_requirement?: number;
    }) => post('/process/create', spec),

    killProcess: (pid: number) => post('/process/kill', { pid }),

    setSchedulerPolicy: (policy: string) =>
      post('/scheduler/policy', { policy }),

    setQuantum: (quantum: number) =>
      post('/scheduler/quantum', { quantum }),

    setMemoryPolicy: (policy: string) =>
      post('/memory/policy', { policy }),

    setFrameCount: (frameCount: number) =>
      post('/memory/frames', { frame_count: frameCount }),

    setAccessSequence: (vpns: number[]) =>
      post('/memory/access_sequence', { vpns }),

    loadWorkload: (scenario: string) =>
      post('/workload/load', { scenario }),

    startSyncDemo: (sync: boolean) =>
      post('/sync/demo', { sync }),

    getSnapshot: () => get('/state/snapshot'),
  };
}
