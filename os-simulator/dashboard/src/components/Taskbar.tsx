import React from 'react';
import { SimState } from '../types/SimState';
import { Monitor, Cpu, Calendar, HardDrive, ShieldAlert, ScrollText } from 'lucide-react';

interface Props {
  state: SimState;
  connected: boolean;
  activePanel: string;
  onPanelClick: (panel: string) => void;
}

const panels = [
  { id: 'process', icon: Cpu, label: 'Processes' },
  { id: 'scheduler', icon: Calendar, label: 'Scheduler' },
  { id: 'memory', icon: HardDrive, label: 'Memory' },
  { id: 'sync', icon: ShieldAlert, label: 'Sync' },
  { id: 'log', icon: ScrollText, label: 'Log' },
];

export const Taskbar: React.FC<Props> = ({ state, connected, activePanel, onPanelClick }) => {
  const statusClass = state.status.toLowerCase();

  return (
    <div className="taskbar">
      <div className="taskbar__left">
        <button className="taskbar__btn" title="Mini OS Kernel Simulator">
          <Monitor size={18} />
        </button>
      </div>

      <div className="taskbar__center">
        {panels.map(p => (
          <button
            key={p.id}
            className={`taskbar__btn ${activePanel === p.id ? 'active' : ''}`}
            onClick={() => onPanelClick(p.id)}
            title={p.label}
          >
            <p.icon size={16} />
          </button>
        ))}
      </div>

      <div className="taskbar__right">
        <div className="taskbar__status">
          <div className={`taskbar__status-dot ${statusClass}`} />
          <span>{state.status}</span>
        </div>
        <span style={{ fontFamily: 'var(--font-mono)' }}>T{state.tick}</span>
        <span style={{ color: connected ? 'var(--state-running)' : 'var(--state-waiting)', fontSize: 11 }}>
          {connected ? '● Online' : '○ Offline'}
        </span>
      </div>
    </div>
  );
};
