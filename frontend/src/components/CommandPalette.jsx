import React, { useEffect, useRef, useState } from 'react';

export default function CommandPalette({ open, onClose, commands = [] }) {
  const [query, setQuery] = useState('');
  const [selectedIdx, setSelectedIdx] = useState(0);
  const inputRef = useRef(null);

  useEffect(() => {
    if (open) {
      setQuery('');
      setSelectedIdx(0);
      setTimeout(() => inputRef.current?.focus(), 20);
    }
  }, [open]);

  if (!open) return null;

  const q = query.trim().toLowerCase();
  const filtered = commands.filter(
    (c) =>
      !q ||
      c.label.toLowerCase().includes(q) ||
      (c.category && c.category.toLowerCase().includes(q)) ||
      (c.hint && c.hint.toLowerCase().includes(q))
  );

  const handleKeyDown = (e) => {
    if (e.key === 'Escape') {
      e.preventDefault();
      onClose();
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      setSelectedIdx((i) => (filtered.length ? (i + 1) % filtered.length : 0));
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      setSelectedIdx((i) => (filtered.length ? (i - 1 + filtered.length) % filtered.length : 0));
    } else if (e.key === 'Enter' && filtered[selectedIdx]) {
      e.preventDefault();
      const action = filtered[selectedIdx].action;
      onClose();
      action();
    }
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="modal-card"
        style={{ width: 'min(580px, 92vw)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 8,
            padding: '10px 12px',
            borderBottom: '1px solid var(--hairline-strong)',
            background: 'var(--bg-surface)',
          }}
        >
          <span style={{ color: 'var(--ink-muted)', fontWeight: 700 }}>&gt;</span>
          <input
            ref={inputRef}
            value={query}
            onChange={(e) => {
              setQuery(e.target.value);
              setSelectedIdx(0);
            }}
            onKeyDown={handleKeyDown}
            placeholder="Type a command or search actions... (esc to close)"
            style={{
              flex: 1,
              background: 'transparent',
              border: 'none',
              outline: 'none',
              color: 'var(--ink-primary)',
              fontSize: 12.5,
            }}
          />
          <span className="oc-kbd">ESC</span>
        </div>

        <div style={{ maxHeight: '54vh', overflowY: 'auto' }}>
          {filtered.length === 0 ? (
            <div style={{ padding: 16, color: 'var(--ink-muted)', fontSize: 11.5 }}>
              [!] no matching falcon commands for "{query}"
            </div>
          ) : (
            filtered.map((cmd, idx) => {
              const active = idx === selectedIdx;
              return (
                <button
                  key={cmd.id}
                  onMouseEnter={() => setSelectedIdx(idx)}
                  onClick={() => {
                    onClose();
                    cmd.action();
                  }}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    width: '100%',
                    padding: '8px 12px',
                    background: active ? 'var(--bg-elevated)' : 'transparent',
                    border: 'none',
                    borderBottom: '1px solid var(--hairline)',
                    color: active ? 'var(--ink-primary)' : 'var(--ink-secondary)',
                    cursor: 'pointer',
                    textAlign: 'left',
                    fontSize: 11.5,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                    <span style={{ color: active ? 'var(--ink-primary)' : 'var(--ink-muted)', width: 22 }}>
                      {active ? '[>]' : '[ ]'}
                    </span>
                    <span style={{ color: 'var(--ink-muted)', fontSize: 10.5 }}>
                      {cmd.category} /
                    </span>
                    <strong style={{ fontWeight: active ? 600 : 400 }}>{cmd.label}</strong>
                  </div>
                  {cmd.shortcut && <span className="oc-kbd">{cmd.shortcut}</span>}
                </button>
              );
            })
          )}
        </div>

        <div
          style={{
            padding: '6px 12px',
            borderTop: '1px solid var(--hairline)',
            background: 'var(--bg-void)',
            display: 'flex',
            justifyContent: 'space-between',
            fontSize: 10.5,
            color: 'var(--ink-muted)',
          }}
        >
          <span>[↑/↓] navigate · [enter] execute</span>
          <span>falcon command palette</span>
        </div>
      </div>
    </div>
  );
}
