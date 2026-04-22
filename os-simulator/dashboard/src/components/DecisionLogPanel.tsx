import React, { useRef, useEffect } from 'react';
import { SimState } from '../types/SimState';
import { ScrollText } from 'lucide-react';

interface Props {
  state: SimState;
}

export const DecisionLogPanel: React.FC<Props> = ({ state }) => {
  const bottomRef = useRef<HTMLDivElement>(null);
  const { decision_log } = state;

  // Auto-scroll to bottom on new entries
  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [decision_log.length]);

  // Show last 200 entries
  const visible = decision_log.slice(-200);

  return (
    <div>
      <div className="section-header">
        <ScrollText size={14} color="var(--accent)" />
        <h3>Decision Log</h3>
        <span style={{ fontSize: 11, color: 'var(--text-secondary)', marginLeft: 'auto' }}>
          {decision_log.length} entries
        </span>
      </div>

      <div style={{ maxHeight: 300, overflowY: 'auto' }}>
        {visible.length === 0 ? (
          <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-secondary)', fontSize: 12 }}>
            No decisions yet. Start the simulation.
          </div>
        ) : (
          visible.map((entry, i) => (
            <div className="log-entry" key={i}>
              <span className="log-entry__tick">T{entry.tick}</span>
              <span className="log-entry__msg">{entry.message}</span>
            </div>
          ))
        )}
        <div ref={bottomRef} />
      </div>
    </div>
  );
};
