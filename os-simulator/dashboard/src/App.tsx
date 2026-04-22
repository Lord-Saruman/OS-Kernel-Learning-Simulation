import React, { useState } from 'react';
import './index.css';
import { useSimState } from './hooks/useSimState';
import { useSimControl } from './hooks/useSimControl';
import { Taskbar } from './components/Taskbar';
import { SimControlBar } from './components/SimControlBar';
import { ProcessPanel } from './components/ProcessPanel';
import { SchedulerPanel } from './components/SchedulerPanel';
import { MemoryPanel } from './components/MemoryPanel';
import { SyncPanel } from './components/SyncPanel';
import { DecisionLogPanel } from './components/DecisionLogPanel';
import {
  Settings, Cpu, Calendar, HardDrive, ShieldAlert, ScrollText,
} from 'lucide-react';

// Tab definitions
const TABS = [
  { id: 'process', title: 'Process Manager', icon: Cpu },
  { id: 'scheduler', title: 'CPU Scheduler', icon: Calendar },
  { id: 'memory', title: 'Memory Manager', icon: HardDrive },
  { id: 'sync', title: 'Synchronization', icon: ShieldAlert },
  { id: 'log', title: 'Decision Log', icon: ScrollText },
];

function App() {
  const { state, connected } = useSimState();
  const control = useSimControl();
  const [activeTab, setActiveTab] = useState('process');

  const renderPanel = (id: string) => {
    switch (id) {
      case 'process': return <ProcessPanel state={state} control={control} />;
      case 'scheduler': return <SchedulerPanel state={state} />;
      case 'memory': return <MemoryPanel state={state} />;
      case 'sync': return <SyncPanel state={state} />;
      case 'log': return <DecisionLogPanel state={state} />;
      default: return null;
    }
  };

  return (
    <div className="desktop">
      {/* Connection overlay */}
      {!connected && (
        <div className="connection-overlay">
          <div className="connection-overlay__card">
            <h2>Connecting to Engine...</h2>
            <div className="spinner" />
            <p>Waiting for C++ engine on port 8080</p>
            <p style={{ marginTop: 8, fontSize: 11 }}>
              Run: <code style={{ color: 'var(--accent)' }}>os_simulator.exe</code>
            </p>
          </div>
        </div>
      )}

      {/* Main Layout */}
      <div className="app-layout">
        {/* Control Bar — always visible at top */}
        <div className="app-control-bar">
          <div className="window" style={{ position: 'relative', width: '100%' }}>
            <div className="window__titlebar">
              <Settings size={14} className="window__icon" />
              <span className="window__title">Simulation Control</span>
            </div>
            <div className="window__body">
              <SimControlBar state={state} control={control} />
            </div>
          </div>
        </div>

        {/* Content Area — two-panel layout */}
        <div className="app-content">
          {/* Left: Tab Navigation */}
          <div className="app-sidebar">
            {TABS.map(tab => {
              const Icon = tab.icon;
              const isActive = activeTab === tab.id;
              return (
                <button
                  key={tab.id}
                  className={`sidebar-tab ${isActive ? 'active' : ''}`}
                  onClick={() => setActiveTab(tab.id)}
                  title={tab.title}
                >
                  <Icon size={18} />
                  <span className="sidebar-tab__label">{tab.title}</span>
                </button>
              );
            })}
          </div>

          {/* Right: Active Panel */}
          <div className="app-panel">
            <div className="window" style={{ position: 'relative', width: '100%', height: '100%' }}>
              <div className="window__titlebar">
                {TABS.filter(t => t.id === activeTab).map(t => {
                  const Icon = t.icon;
                  return (
                    <React.Fragment key={t.id}>
                      <Icon size={14} className="window__icon" />
                      <span className="window__title">{t.title}</span>
                    </React.Fragment>
                  );
                })}
              </div>
              <div className="window__body" style={{ flex: 1, overflow: 'auto' }}>
                {renderPanel(activeTab)}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Taskbar */}
      <Taskbar
        state={state}
        connected={connected}
        activePanel={activeTab}
        onPanelClick={setActiveTab}
      />
    </div>
  );
}

export default App;
