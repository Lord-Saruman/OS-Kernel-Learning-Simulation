import React, { useState } from 'react';
import { SimState } from '../types/SimState';
import {
  Play, Pause, RotateCcw, SkipForward,
  Zap, FolderOpen,
} from 'lucide-react';

interface Props {
  state: SimState;
  control: ReturnType<typeof import('../hooks/useSimControl').useSimControl>;
}

export const SimControlBar: React.FC<Props> = ({ state, control }) => {
  const isRunning = state.status === 'RUNNING';
  const isPaused = state.status === 'PAUSED';
  const isIdle = state.status === 'IDLE';
  const [workloadValue, setWorkloadValue] = useState('');
  const textbookLruWinSequence = [
    7, 0, 1, 2, 0, 3, 0, 4, 2, 3,
    0, 3, 2, 1, 2, 0, 1, 7, 0, 1,
  ];

  const handleStart = async () => {
    await control.start();
  };

  const handlePause = async () => {
    await control.pause();
  };

  const handleStep = async () => {
    // If IDLE, start the engine first so the step is accepted
    if (isIdle) {
      await control.start();
    }
    await control.step();
  };

  const handleReset = async () => {
    await control.reset();
  };

  const handleModeChange = async (newMode: string) => {
    await control.setMode(newMode);
    // If switching to AUTO and simulation is not running, auto-start it
    if (newMode === 'AUTO' && state.status !== 'RUNNING') {
      await control.start();
    }
  };

  const handleWorkloadChange = async (scenario: string) => {
    if (!scenario) return;
    setWorkloadValue(scenario);
    await control.loadWorkload(scenario);
    // Reset dropdown back to placeholder after 500ms
    setTimeout(() => setWorkloadValue(''), 500);
  };

  const handleFrameCountChange = async (frameCount: number) => {
    await control.setFrameCount(frameCount);
  };

  const handleTextbookLruWin = async () => {
    // This is F7 from tests/TESTING.md: with 4 frames, FIFO=10 and LRU=8.
    const policy = state.active_replacement;
    await control.setFrameCount(4); // Resets the simulation.
    await control.setMode('STEP');
    await control.setSchedulerPolicy('FCFS');
    await control.setMemoryPolicy(policy);
    await control.setAccessSequence(textbookLruWinSequence);
    await control.createProcess({
      name: `Textbook_${policy}`,
      type: 'CPU_BOUND',
      priority: 5,
      cpu_burst: 100,
      memory_requirement: 8,
    });
  };

  return (
    <div className="control-bar">
      {/* ── Play / Pause / Step / Reset ── */}
      <div className="control-group">
        {isRunning ? (
          <button className="btn" onClick={handlePause} title="Pause simulation">
            <Pause size={14} /> Pause
          </button>
        ) : (
          <button className="btn primary" onClick={handleStart} title="Start simulation">
            <Play size={14} /> {isPaused ? 'Resume' : 'Start'}
          </button>
        )}
        <button className="btn" onClick={handleStep} title="Advance one tick (auto-starts if idle)">
          <SkipForward size={14} /> Step
        </button>
        <button className="btn danger" onClick={handleReset} title="Reset everything to tick 0">
          <RotateCcw size={14} /> Reset
        </button>
      </div>

      <div className="control-divider" />

      {/* ── Mode ── */}
      <div className="control-group">
        <label>Mode</label>
        <select
          className="select"
          value={state.mode}
          onChange={(e) => handleModeChange(e.target.value)}
        >
          <option value="STEP">Step</option>
          <option value="AUTO">Auto</option>
        </select>
      </div>

      {/* ── Speed (only visible in AUTO mode) ── */}
      {state.mode === 'AUTO' && (
        <div className="control-group">
          <label>Speed</label>
          <input
            className="input"
            type="range"
            min="50"
            max="2000"
            step="50"
            value={state.auto_speed_ms}
            onChange={(e) => control.setSpeed(Number(e.target.value))}
            style={{ width: 80 }}
          />
          <span style={{ fontSize: 11, color: 'var(--text-secondary)', minWidth: 42 }}>
            {state.auto_speed_ms}ms
          </span>
        </div>
      )}

      <div className="control-divider" />

      {/* ── Scheduling Policy ── */}
      <div className="control-group">
        <label>Scheduler</label>
        <select
          className="select"
          value={state.active_policy}
          onChange={(e) => control.setSchedulerPolicy(e.target.value)}
        >
          <option value="FCFS">FCFS</option>
          <option value="ROUND_ROBIN">Round Robin</option>
          <option value="PRIORITY_NP">Priority (NP)</option>
          <option value="PRIORITY_P">Priority (P)</option>
        </select>
      </div>

      {/* ── Time Quantum (only visible with Round Robin) ── */}
      {(state.active_policy === 'ROUND_ROBIN') && (
        <div className="control-group">
          <label>Quantum</label>
          <input
            className="input"
            type="number"
            min="1"
            max="20"
            value={state.time_quantum}
            onChange={(e) => control.setQuantum(Number(e.target.value))}
            style={{ width: 50 }}
          />
        </div>
      )}

      <div className="control-divider" />

      {/* ── Memory Policy ── */}
      <div className="control-group">
        <label>Memory</label>
        <select
          className="select"
          value={state.active_replacement}
          onChange={(e) => control.setMemoryPolicy(e.target.value)}
        >
          <option value="FIFO">FIFO</option>
          <option value="LRU">LRU</option>
        </select>
      </div>

      {/* ── Physical Frames ── */}
      <div className="control-group">
        <label>Frames</label>
        <select
          className="select"
          value={state.mem_metrics.total_frames}
          onChange={(e) => handleFrameCountChange(Number(e.target.value))}
          title="Changing frames resets simulation"
        >
          <option value={4}>4</option>
          <option value={6}>6</option>
          <option value={8}>8</option>
          <option value={16}>16</option>
        </select>
      </div>

      <div className="control-divider" />

      {/* ── Workload Loader (buttons instead of broken select) ── */}
      <div className="control-group">
        <label>Load</label>
        <button className="btn sm" onClick={() => handleWorkloadChange('cpu_bound')} title="Load 5 CPU-bound processes">
          <FolderOpen size={12} /> CPU (5)
        </button>
        <button className="btn sm" onClick={() => handleWorkloadChange('io_bound')} title="Load 5 I/O-bound processes">
          <FolderOpen size={12} /> I/O (5)
        </button>
        <button className="btn sm" onClick={() => handleWorkloadChange('mixed')} title="Load 8 mixed processes">
          <FolderOpen size={12} /> Mixed (8)
        </button>
        <button
          className="btn sm"
          onClick={handleTextbookLruWin}
          title="Load Silberschatz sequence from TESTING.md. Step 20 ticks: FIFO=10, LRU=8."
        >
          <FolderOpen size={12} /> LRU vs FIFO
        </button>
        <div className="control-divider" style={{ margin: '0 4px' }} />
        <button className="btn sm" onClick={() => control.startSyncDemo(true)} title="Run Race Condition Demo with Mutex">
          <Zap size={12} /> Sync Demo
        </button>
      </div>

      {/* ── Status Display (right side) ── */}
      <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: 8 }}>
        <Zap size={12} color="var(--accent)" />
        <span style={{ fontFamily: 'var(--font-mono)', fontSize: 12, color: 'var(--text-secondary)' }}>
          Tick {state.tick}
        </span>
        <span className={`badge ${state.status.toLowerCase()}`}>{state.status}</span>
      </div>
    </div>
  );
};
