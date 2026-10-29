import React, { useEffect, useMemo, useRef, useState } from 'react';

export default function CommandPalette({ open, onClose, commands }) {
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

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (!q) return commands;
    return commands.filter(
      (c) =>
        c.label.toLowerCase().includes(q) ||
        (c.category || '').toLowerCase().includes(q) ||
        (c.shortcut || '').toLowerCase().includes(q)
    );
  }, [commands, query]);

  if (!open) return null;

  const runSelected = (item) => {
    if (!item) return;
    onClose();
    item.action?.();
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="modal-card"
        style={{ width: 'min(560px, 92vw)', display: 'flex', flexDirection: 'column' }}
        onClick={(e) => e.stopPropagation()}
      >
        <div style={{ padding: '12px 14px', borderBottom: '1px solid var(--hairline)' }}>
          <input
            ref={inputRef}
            className="oc-composer-input"
            placeholder="Type a command or search settings, providers, exports..."
            value={query}
            onChange={(e) => {
              setQuery(e.target.value);
              setSelectedIdx(0);
            }}
            onKeyDown={(e) => {
              if (e.key === 'Escape') onClose();
              else if (e.key === 'ArrowDown') {
                e.preventDefault();
                setSelectedIdx((i) => (filtered.length ? (i + 1) % filtered.length : 0));
              } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                setSelectedIdx((i) => (filtered.length ? (i - 1 + filtered.length) % filtered.length : 0));
              } else if (e.key === 'Enter' && filtered[selectedIdx]) {
                e.preventDefault();
                runSelected(filtered[selectedIdx]);
              }
            }}
          />
        </div>

        <div style={{ maxHeight: 340, overflowY: 'auto', padding: 6 }}>
          {filtered.length === 0 ? (
            <div style={{ padding: 14, color: 'var(--ink-muted)', fontSize: 12, textAlign: 'center' }}>
              No matching commands
            </div>
          ) : (
            filtered.map((cmd, idx) => (
              <button
                key={cmd.id}
                className={`oc-slash-item ${idx === selectedIdx ? 'selected' : ''}`}
                style={{ width: '100%', padding: '8px 10px' }}
                onMouseEnter={() => setSelectedIdx(idx)}
                onClick={() => runSelected(cmd)}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                  <span style={{ fontSize: 11, color: 'var(--ink-muted)', minWidth: 54 }}>{cmd.category}</span>
                  <span>{cmd.label}</span>
                </div>
                {cmd.shortcut && <span className="oc-kbd">{cmd.shortcut}</span>}
              </button>
            ))
          )}
        </div>
      </div>
    </div>
  );
}
