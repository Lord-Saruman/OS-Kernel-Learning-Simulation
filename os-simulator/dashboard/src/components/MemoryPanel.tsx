import React from 'react';
import { SimState } from '../types/SimState';
import { HardDrive } from 'lucide-react';

interface Props {
  state: SimState;
}

const PID_COLORS = [
  '#60a5fa', '#a78bfa', '#34d399', '#f472b6', '#fbbf24',
  '#fb923c', '#38bdf8', '#c084fc', '#4ade80', '#f87171',
];

function pidColor(pid: number): string {
  if (pid < 0) return 'transparent';
  return PID_COLORS[pid % PID_COLORS.length];
}

export const MemoryPanel: React.FC<Props> = ({ state }) => {
  const { frame_table, mem_metrics } = state;

  return (
    <div>
      {/* Metrics Row */}
      <div className="section-header">
        <HardDrive size={14} color="var(--module-memory)" />
        <h3>Memory</h3>
        <span style={{ fontSize: 11, color: 'var(--text-secondary)', marginLeft: 'auto' }}>
          {mem_metrics.occupied_frames}/{mem_metrics.total_frames} frames used
        </span>
      </div>

      <div className="metric-grid" style={{ marginBottom: 12 }}>
        <div className="metric-card">
          <div className="metric-card__value">{mem_metrics.total_page_faults}</div>
          <div className="metric-card__label">Page Faults</div>
        </div>
        <div className="metric-card">
          <div className="metric-card__value">{mem_metrics.page_fault_rate.toFixed(1)}%</div>
          <div className="metric-card__label">Fault Rate</div>
        </div>
        <div className="metric-card">
          <div className="metric-card__value">{mem_metrics.total_replacements}</div>
          <div className="metric-card__label">Replacements</div>
        </div>
        <div className="metric-card">
          <div className="metric-card__value">{mem_metrics.frames_free}</div>
          <div className="metric-card__label">Free Frames</div>
        </div>
      </div>

      {/* Frame Table Grid */}
      <div className="section-header"><h3>Frame Table</h3></div>
      <div className="frame-grid">
        {frame_table.map(f => (
          <div
            key={f.frame_number}
            className={`frame-cell ${f.occupied ? 'occupied' : ''}`}
            style={f.occupied ? { borderColor: pidColor(f.owner_pid) + '80', background: pidColor(f.owner_pid) + '18' } : {}}
            title={f.occupied
              ? `Frame ${f.frame_number}: PID ${f.owner_pid}, VPN ${f.virtual_page_number}`
              : `Frame ${f.frame_number}: Free`}
          >
            <span className="frame-id">F{f.frame_number}</span>
            {f.occupied ? (
              <span style={{ color: pidColor(f.owner_pid), fontSize: 11, fontWeight: 600 }}>P{f.owner_pid}</span>
            ) : (
              <span style={{ color: 'var(--text-secondary)', fontSize: 9 }}>—</span>
            )}
          </div>
        ))}
      </div>

      {frame_table.length === 0 && (
        <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-secondary)', fontSize: 12 }}>
          Frame table not initialized.
        </div>
      )}
    </div>
  );
};
