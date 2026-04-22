import React from 'react';
import { SimState } from '../types/SimState';
import { Lock, Unlock, ShieldAlert } from 'lucide-react';

interface Props {
  state: SimState;
}

export const SyncPanel: React.FC<Props> = ({ state }) => {
  const mutexes = Object.values(state.mutex_table);
  const semaphores = Object.values(state.semaphore_table);

  return (
    <div>
      <div className="section-header">
        <ShieldAlert size={14} color="var(--module-sync)" />
        <h3>Synchronization</h3>
      </div>

      {/* Mutexes */}
      {mutexes.length > 0 && (
        <>
          <div style={{ fontSize: 11, color: 'var(--text-secondary)', marginBottom: 6, fontWeight: 600, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
            Mutexes
          </div>
          {mutexes.map(mx => (
            <div className="sync-card" key={mx.mutex_id}>
              <div className="sync-card__header">
                <span className="sync-card__name">
                  {mx.locked ? <Lock size={12} style={{ marginRight: 4 }} /> : <Unlock size={12} style={{ marginRight: 4 }} />}
                  {mx.name}
                </span>
                <span className={`sync-card__status ${mx.locked ? 'locked' : 'unlocked'}`}>
                  {mx.locked ? `Locked by P${mx.owner_pid}` : 'Free'}
                </span>
              </div>
              {mx.waiting_pids.length > 0 && (
                <div style={{ fontSize: 11, color: 'var(--text-secondary)' }}>
                  Blocked: {mx.waiting_pids.map(p => `P${p}`).join(', ')}
                </div>
              )}
              <div style={{ fontSize: 10, color: 'var(--text-secondary)', marginTop: 4 }}>
                Acquisitions: {mx.total_acquisitions} · Contentions: {mx.total_contentions}
              </div>
            </div>
          ))}
        </>
      )}

      {/* Semaphores */}
      {semaphores.length > 0 && (
        <>
          <div style={{ fontSize: 11, color: 'var(--text-secondary)', marginBottom: 6, marginTop: 12, fontWeight: 600, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
            Semaphores
          </div>
          {semaphores.map(sem => (
            <div className="sync-card" key={sem.sem_id}>
              <div className="sync-card__header">
                <span className="sync-card__name">{sem.name}</span>
                <span style={{ fontSize: 12, fontFamily: 'var(--font-mono)', color: sem.value > 0 ? 'var(--state-running)' : 'var(--state-waiting)' }}>
                  {sem.value}/{sem.initial_value}
                </span>
              </div>
              <div style={{ fontSize: 10, color: 'var(--text-secondary)' }}>
                {sem.primitive_type} · Waits: {sem.total_waits} · Signals: {sem.total_signals} · Blocks: {sem.total_blocks}
              </div>
              {sem.waiting_pids.length > 0 && (
                <div style={{ fontSize: 11, color: 'var(--state-waiting)', marginTop: 4 }}>
                  Blocked: {sem.waiting_pids.map(p => `P${p}`).join(', ')}
                </div>
              )}
            </div>
          ))}
        </>
      )}

      {mutexes.length === 0 && semaphores.length === 0 && (
        <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-secondary)', fontSize: 12 }}>
          No synchronization primitives created.
        </div>
      )}
    </div>
  );
};
