import React from 'react';
import { SimState } from '../types/SimState';
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, Cell } from 'recharts';
import { Calendar, ChevronRight } from 'lucide-react';

interface Props {
  state: SimState;
}

// Stable color palette for PIDs
const PID_COLORS = [
  '#60a5fa', '#a78bfa', '#34d399', '#f472b6', '#fbbf24',
  '#fb923c', '#38bdf8', '#c084fc', '#4ade80', '#f87171',
  '#22d3ee', '#e879f9', '#a3e635', '#fb7185',
];

function pidColor(pid: number): string {
  if (pid < 0) return '#333';
  return PID_COLORS[pid % PID_COLORS.length];
}

export const SchedulerPanel: React.FC<Props> = ({ state }) => {
  const { ready_queue, gantt_log, metrics, running_pid } = state;

  // Last 40 gantt entries for the chart
  const ganttSlice = gantt_log.slice(-40);

  return (
    <div>
      {/* Ready Queue */}
      <div className="section-header">
        <Calendar size={14} color="var(--module-scheduler)" />
        <h3>Ready Queue</h3>
        <span style={{ fontSize: 11, color: 'var(--text-secondary)', marginLeft: 'auto' }}>
          CPU: {running_pid >= 0 ? `PID ${running_pid}` : 'Idle'}
        </span>
      </div>

      <div className="ready-queue">
        {ready_queue.length === 0 ? (
          <span className="ready-queue__empty">Empty</span>
        ) : (
          ready_queue.map((pid, i) => (
            <React.Fragment key={`${pid}-${i}`}>
              {i > 0 && <ChevronRight size={12} className="ready-queue__arrow" />}
              <div className="ready-queue__item" style={{ borderColor: pidColor(pid) + '60', background: pidColor(pid) + '15', color: pidColor(pid) }}>
                P{pid}
              </div>
            </React.Fragment>
          ))
        )}
      </div>

      {/* Gantt Chart */}
      <div style={{ marginTop: 16 }}>
        <div className="section-header">
          <h3>Gantt Chart</h3>
          <span style={{ fontSize: 11, color: 'var(--text-secondary)', marginLeft: 'auto' }}>
            Last {ganttSlice.length} ticks
          </span>
        </div>

        {ganttSlice.length > 0 ? (
          <ResponsiveContainer width="100%" height={80}>
            <BarChart data={ganttSlice} barCategoryGap={1}>
              <XAxis
                dataKey="tick"
                tick={{ fontSize: 9, fill: '#6868888' }}
                tickLine={false}
                axisLine={{ stroke: 'rgba(255,255,255,0.06)' }}
                interval="preserveStartEnd"
              />
              <YAxis hide />
              <Tooltip
                contentStyle={{ background: '#1a1a3e', border: '1px solid rgba(255,255,255,0.1)', borderRadius: 6, fontSize: 11 }}
                labelStyle={{ color: '#9898b8' }}
                formatter={(value: number) => value >= 0 ? `PID ${value}` : 'Idle'}
                labelFormatter={(tick: number) => `Tick ${tick}`}
              />
              <Bar dataKey="pid" radius={[2, 2, 0, 0]}>
                {ganttSlice.map((entry, i) => (
                  <Cell key={i} fill={pidColor(entry.pid)} opacity={entry.pid >= 0 ? 0.85 : 0.15} />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        ) : (
          <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-secondary)', fontSize: 12 }}>
            No ticks yet. Start the simulation.
          </div>
        )}
      </div>

      {/* Metrics */}
      <div style={{ marginTop: 16 }}>
        <div className="section-header"><h3>Metrics</h3></div>
        <div className="metric-grid">
          <div className="metric-card">
            <div className="metric-card__value">{metrics.avg_waiting_time.toFixed(1)}</div>
            <div className="metric-card__label">Avg Wait</div>
          </div>
          <div className="metric-card">
            <div className="metric-card__value">{metrics.avg_turnaround_time.toFixed(1)}</div>
            <div className="metric-card__label">Avg Turnaround</div>
          </div>
          <div className="metric-card">
            <div className="metric-card__value">{metrics.cpu_utilization.toFixed(0)}%</div>
            <div className="metric-card__label">CPU Util</div>
          </div>
          <div className="metric-card">
            <div className="metric-card__value">{metrics.total_context_switches}</div>
            <div className="metric-card__label">Ctx Switches</div>
          </div>
          <div className="metric-card">
            <div className="metric-card__value">{metrics.completed_processes}/{metrics.total_processes}</div>
            <div className="metric-card__label">Completed</div>
          </div>
          <div className="metric-card">
            <div className="metric-card__value">{metrics.throughput}</div>
            <div className="metric-card__label">Throughput/100</div>
          </div>
        </div>
      </div>
    </div>
  );
};
