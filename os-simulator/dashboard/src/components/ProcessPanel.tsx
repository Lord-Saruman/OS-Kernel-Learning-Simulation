import React, { useState } from 'react';
import { SimState } from '../types/SimState';
import { Plus, Trash2, Users } from 'lucide-react';

interface Props {
  state: SimState;
  control: ReturnType<typeof import('../hooks/useSimControl').useSimControl>;
}

export const ProcessPanel: React.FC<Props> = ({ state, control }) => {
  const [showCreate, setShowCreate] = useState(false);
  const [newName, setNewName] = useState('');
  const [newType, setNewType] = useState('CPU_BOUND');
  const [newPriority, setNewPriority] = useState(5);
  const [newBurst, setNewBurst] = useState(0);

  const processes = state.processes;

  const handleCreate = () => {
    control.createProcess({
      name: newName || undefined,
      type: newType,
      priority: newPriority,
      cpu_burst: newBurst || undefined,
    });
    setShowCreate(false);
    setNewName('');
    setNewBurst(0);
  };

  return (
    <div>
      <div className="section-header">
        <Users size={14} color="var(--module-process)" />
        <h3>Processes</h3>
        <span style={{ fontSize: 11, color: 'var(--text-secondary)', marginLeft: 'auto' }}>
          {processes.filter(p => p.state !== 'TERMINATED').length} active / {processes.length} total
        </span>
        <button className="btn sm" onClick={() => setShowCreate(!showCreate)}>
          <Plus size={12} /> New
        </button>
      </div>

      {/* Create Process Form */}
      {showCreate && (
        <div className="card" style={{ marginBottom: 12, display: 'flex', gap: 8, alignItems: 'flex-end', flexWrap: 'wrap' }}>
          <div>
            <label style={{ fontSize: 10, color: 'var(--text-secondary)', display: 'block', marginBottom: 2 }}>Name</label>
            <input className="input" placeholder="auto" value={newName} onChange={e => setNewName(e.target.value)} style={{ width: 80 }} />
          </div>
          <div>
            <label style={{ fontSize: 10, color: 'var(--text-secondary)', display: 'block', marginBottom: 2 }}>Type</label>
            <select className="select" value={newType} onChange={e => setNewType(e.target.value)}>
              <option value="CPU_BOUND">CPU</option>
              <option value="IO_BOUND">I/O</option>
              <option value="MIXED">Mixed</option>
            </select>
          </div>
          <div>
            <label style={{ fontSize: 10, color: 'var(--text-secondary)', display: 'block', marginBottom: 2 }}>Priority</label>
            <input className="input" type="number" min={1} max={10} value={newPriority} onChange={e => setNewPriority(Number(e.target.value))} style={{ width: 46 }} />
          </div>
          <div>
            <label style={{ fontSize: 10, color: 'var(--text-secondary)', display: 'block', marginBottom: 2 }}>Burst</label>
            <input className="input" type="number" min={0} max={100} value={newBurst} onChange={e => setNewBurst(Number(e.target.value))} style={{ width: 46 }} placeholder="auto" />
          </div>
          <button className="btn primary sm" onClick={handleCreate}>Create</button>
        </div>
      )}

      {/* Process Table */}
      <table className="table">
        <thead>
          <tr>
            <th>PID</th>
            <th>Name</th>
            <th>State</th>
            <th>Type</th>
            <th>Pri</th>
            <th>Burst</th>
            <th>Wait</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          {processes.length === 0 && (
            <tr><td colSpan={8} style={{ textAlign: 'center', color: 'var(--text-secondary)', padding: 20 }}>No processes. Create one or load a workload.</td></tr>
          )}
          {processes.map(p => (
            <tr key={p.pid}>
              <td className="mono">{p.pid}</td>
              <td>{p.name}</td>
              <td><span className={`badge ${p.state.toLowerCase()}`}>{p.state}</span></td>
              <td style={{ fontSize: 11, color: 'var(--text-secondary)' }}>{p.type}</td>
              <td className="mono">{p.priority}</td>
              <td className="mono">{p.remaining_burst}/{p.total_cpu_burst}</td>
              <td className="mono">{p.waiting_time}</td>
              <td>
                {p.state !== 'TERMINATED' && (
                  <button className="btn sm danger" onClick={() => control.killProcess(p.pid)} title="Kill">
                    <Trash2 size={10} />
                  </button>
                )}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};
