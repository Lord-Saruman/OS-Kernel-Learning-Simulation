/**
 * useSimState.ts — WebSocket hook for live simulation state
 *
 * Reference: SDD §6.3 (State Management)
 *
 * Connects to the WebSocket endpoint via the Vite dev proxy and replaces
 * the entire display state on each message (the engine is the single
 * source of truth).  Auto-reconnects with exponential backoff.
 */

import { useEffect, useReducer, useRef, useCallback, useState } from 'react';
import { SimState, INITIAL_STATE } from '../types/SimState';

type Action =
  | { type: 'STATE_UPDATE'; payload: SimState }
  | { type: 'RESET' };

function simReducer(_state: SimState, action: Action): SimState {
  switch (action.type) {
    case 'STATE_UPDATE':
      return action.payload;
    case 'RESET':
      return INITIAL_STATE;
    default:
      return _state;
  }
}

/**
 * Build the WebSocket URL.
 * In dev mode the Vite proxy forwards /ws → ws://localhost:8080/ws,
 * so we connect relative to the current host.  This avoids hard-coding
 * a port and makes the dashboard work behind any reverse-proxy too.
 */
function buildWsUrl(): string {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${protocol}//${window.location.host}/ws`;
}

export function useSimState() {
  const [state, dispatch] = useReducer(simReducer, INITIAL_STATE);
  const [connected, setConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const retryRef = useRef(0);
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return;

    const url = buildWsUrl();
    console.log('[WS] Connecting to', url);
    const ws = new WebSocket(url);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log('[WS] Connected');
      setConnected(true);
      retryRef.current = 0; // Reset backoff
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data) as SimState;
        dispatch({ type: 'STATE_UPDATE', payload: data });
      } catch (e) {
        console.error('[WS] Failed to parse state:', e);
      }
    };

    ws.onclose = () => {
      console.log('[WS] Disconnected, will retry…');
      setConnected(false);
      wsRef.current = null;
      // Exponential backoff: 1s, 2s, 4s, 8s, max 16s
      const delay = Math.min(1000 * Math.pow(2, retryRef.current), 16000);
      retryRef.current++;
      timerRef.current = setTimeout(connect, delay);
    };

    ws.onerror = () => {
      ws.close();
    };
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (timerRef.current) clearTimeout(timerRef.current);
      if (wsRef.current) wsRef.current.close();
    };
  }, [connect]);

  return { state, connected };
}
