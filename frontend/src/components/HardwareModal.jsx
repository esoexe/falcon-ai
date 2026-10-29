import React, { useEffect, useState } from 'react';

export default function HardwareModal({
  open,
  onClose,
  hardware,
  routerConfig,
  onRescan,
  onSaveRouter,
}) {
  const [activeTab, setActiveTab] = useState('no_key');
  const [catalog, setCatalog] = useState(null);
  const [search, setSearch] = useState('');
  const [keyDrafts, setKeyDrafts] = useState({});
  const [savingId, setSavingId] = useState('');
  const [cfg, setCfg] = useState({
    provider: 'auto',
    ollama_url: 'http://127.0.0.1:11434',
    ollama_model: '',
    openai_base_url: 'http://127.0.0.1:1234/v1',
    openai_api_key: '',
    openai_model: '',
  });
  const [rescanning, setRescanning] = useState(false);

  useEffect(() => {
    if (routerConfig) setCfg((prev) => ({ ...prev, ...routerConfig }));
  }, [routerConfig]);

  useEffect(() => {
    if (!open) return;
    fetch('/api/providers')
      .then((r) => (r.ok ? r.json() : null))
      .then((d) => {
        if (d) setCatalog(d);
      })
      .catch(() => {});
  }, [open]);

  if (!open) return null;

  const profile = hardware?.profile || {};
  const cpu = hardware?.cpu || {};
  const mem = hardware?.memory || {};
  const gpus = hardware?.gpus || [];
  const services = hardware?.services || {};
  const models = hardware?.models || [];

  const counts = catalog?.counts || {
    no_key: 5,
    free_key: 52,
    local: 11,
    total_free: 68,
    paid_opt_in: 30,
    active_in_swarm: 7,
    available_team_models: 39,
  };
  const allProviders = catalog?.providers || [];

  const handleRescan = async () => {
    setRescanning(true);
    await onRescan();
    setRescanning(false);
  };

  const handleSaveRouter = async () => {
    await onSaveRouter(cfg);
    onClose();
  };

  const updateProvider = async (payload) => {
    setSavingId(payload.id);
    try {
      const r = await fetch('/api/providers', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      if (r.ok) {
        const d = await r.json();
        setCatalog(d);
      }
    } catch {
      // ignore offline
    }
    setSavingId('');
  };

  const filteredProviders = allProviders.filter((p) => {
    if (p.group !== activeTab) return false;
    if (!search.trim()) return true;
    const q = search.toLowerCase();
    return (
      p.name.toLowerCase().includes(q) ||
      p.site.toLowerCase().includes(q) ||
      p.limit_notes.toLowerCase().includes(q) ||
      p.model.toLowerCase().includes(q)
    );
  });

  const serviceStatus = (ok) => (
    <span style={{ color: ok ? 'var(--sig-green)' : 'var(--ink-muted)', fontWeight: 600 }}>
      {ok ? '[on]' : '[--]'}
    </span>
  );

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="modal-card"
        style={{ width: 'min(920px, 95vw)', maxHeight: '88vh', display: 'flex', flexDirection: 'column' }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            padding: '10px 14px',
            borderBottom: '1px solid var(--hairline-strong)',
            background: 'var(--bg-void)',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            gap: 10,
          }}
        >
          <div>
            <strong>settings &rarr; providers &amp; hardware // {counts.total_free} free + {counts.paid_opt_in} opt-in ai providers</strong>
            <div style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>
              [{counts.active_in_swarm} active providers &middot; ~{counts.available_team_models} models in 6-agent swarm team] &middot; paid providers off by default
            </div>
          </div>
          <div style={{ display: 'flex', gap: 6 }}>
            <button className="oc-btn oc-btn-sm" onClick={handleRescan} disabled={rescanning}>
              {rescanning ? '[scanning...]' : '[rescan]'}
            </button>
            <button className="oc-btn oc-btn-sm" onClick={onClose}>
              [esc]
            </button>
          </div>
        </div>

        {/* Summary Counters + Category Tabs */}
        <div
          style={{
            padding: '8px 14px',
            borderBottom: '1px solid var(--hairline)',
            background: 'var(--bg-surface)',
            display: 'flex',
            flexWrap: 'wrap',
            justifyContent: 'space-between',
            alignItems: 'center',
            gap: 8,
          }}
        >
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6 }}>
            {[
              { id: 'no_key', label: `[1] no-key instant (${counts.no_key})` },
              { id: 'free_key', label: `[2] free-key tier (${counts.free_key})` },
              { id: 'local', label: `[3] local computer (${counts.local})` },
              { id: 'paid', label: `[4] paid opt-in (${counts.paid_opt_in})` },
              { id: 'hardware', label: '[5] hardware & 3d' },
            ].map((t) => (
              <button
                key={t.id}
                className={`oc-btn oc-btn-sm ${activeTab === t.id ? 'oc-btn-primary' : ''}`}
                onClick={() => setActiveTab(t.id)}
              >
                {t.label}
              </button>
            ))}
          </div>

          {activeTab !== 'hardware' && (
            <input
              className="text-input"
              style={{ width: 220, padding: '4px 8px', fontSize: 11 }}
              placeholder="filter providers or models..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
            />
          )}
        </div>

        {/* Body */}
        <div style={{ padding: 14, overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: 10, flex: 1 }}>
          {activeTab !== 'hardware' ? (
            <>
              {/* Group Banner */}
              <div
                style={{
                  padding: '8px 11px',
                  border: '1px solid var(--hairline-strong)',
                  borderRadius: 4,
                  background: 'var(--bg-void)',
                  fontSize: 11,
                  color: 'var(--ink-secondary)',
                }}
              >
                {activeTab === 'no_key' && (
                  <>
                    <strong style={{ color: 'var(--sig-green)' }}>[+] work with no key &amp; no account:</strong>{' '}
                    Instant-on right out of a fresh install (~30 free models ready immediately across OVHcloud, Kilo Gateway, LLM7, and Pollinations).
                  </>
                )}
                {activeTab === 'free_key' && (
                  <>
                    <strong style={{ color: 'var(--sig-green)' }}>[+] free tier with a free key ({counts.free_key} providers):</strong>{' '}
                    Sign up for a free key on any provider below and paste it here — each key automatically adds its free models to the 6-Agent Swarm.
                  </>
                )}
                {activeTab === 'local' && (
                  <>
                    <strong style={{ color: 'var(--sig-green)' }}>[+] local on your own computer ({counts.local} engines):</strong>{' '}
                    100% free &amp; unlimited — runs on your own RAM/CPU/GPU with zero cloud limits (includes Falcon Built-in C++20 Zero-GPU Engine).
                  </>
                )}
                {activeTab === 'paid' && (
                  <>
                    <strong style={{ color: 'var(--sig-amber, #f59e0b)' }}>[!] paid providers (30 &middot; off by default):</strong>{' '}
                    Disabled by default so Falcon AI never spends money unless you explicitly opt in and add a key per provider.
                  </>
                )}
              </div>

              {/* Providers List */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
                {filteredProviders.map((p, idx) => {
                  const needsKey = p.group === 'free_key' || p.group === 'paid';
                  const draft = keyDrafts[p.id] ?? '';
                  return (
                    <div
                      key={p.id}
                      style={{
                        padding: '8px 10px',
                        border: '1px solid var(--hairline)',
                        borderRadius: 4,
                        background: p.ready_in_team ? 'var(--bg-surface)' : 'transparent',
                        display: 'flex',
                        flexDirection: 'column',
                        gap: 6,
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
                          <span style={{ color: 'var(--ink-muted)', fontSize: 10.5 }}>#{idx + 1}</span>
                          <strong style={{ fontSize: 12 }}>{p.name}</strong>
                          {p.is_bonus && (
                            <span className="oc-kbd" style={{ color: 'var(--sig-green)' }}>+2026 discovered</span>
                          )}
                          <span className="oc-kbd">{p.site}</span>
                          <span style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>model: {p.model}</span>
                        </div>

                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                          <span
                            style={{
                              fontSize: 10.5,
                              fontWeight: 700,
                              color: p.ready_in_team ? 'var(--sig-green)' : 'var(--ink-muted)',
                            }}
                          >
                            {p.ready_in_team ? '[● in swarm]' : p.group === 'paid' ? '[off]' : '[needs free key]'}
                          </span>
                          <button
                            className="oc-btn oc-btn-sm"
                            disabled={savingId === p.id}
                            onClick={() => updateProvider({ id: p.id, enabled: !p.enabled })}
                          >
                            {p.enabled ? '[disable]' : '[enable]'}
                          </button>
                        </div>
                      </div>

                      <div style={{ fontSize: 11, color: 'var(--ink-secondary)' }}>
                        {p.limit_notes} &middot; <span style={{ color: 'var(--ink-muted)' }}>{p.base_url}</span>
                      </div>

                      {(needsKey || p.id === 'llm7') && (
                        <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                          <input
                            type="password"
                            className="text-input"
                            style={{ flex: 1, padding: '4px 8px', fontSize: 11 }}
                            placeholder={
                              p.has_key
                                ? `saved key (${p.key_preview}) — paste new key to replace`
                                : p.id === 'llm7'
                                ? 'optional free token from token.llm7.io (raises to 120 RPM)...'
                                : `paste ${p.name} API key from ${p.site}...`
                            }
                            value={draft}
                            onChange={(e) => setKeyDrafts((prev) => ({ ...prev, [p.id]: e.target.value }))}
                          />
                          <button
                            className="oc-btn oc-btn-primary oc-btn-sm"
                            disabled={!draft.trim() || savingId === p.id}
                            onClick={async () => {
                              await updateProvider({ id: p.id, api_key: draft.trim(), enabled: true });
                              setKeyDrafts((prev) => ({ ...prev, [p.id]: '' }));
                            }}
                          >
                            [save key]
                          </button>
                          {p.has_key && (
                            <button
                              className="oc-btn oc-btn-sm"
                              onClick={() => updateProvider({ id: p.id, api_key: '__CLEAR__' })}
                            >
                              [clear]
                            </button>
                          )}
                        </div>
                      )}
                    </div>
                  );
                })}
              </div>
            </>
          ) : (
            /* Tab 5: Bare-Metal Hardware & 3D Engines */
            <>
              <div
                style={{
                  padding: 10,
                  border: '1px solid var(--hairline-strong)',
                  borderRadius: 4,
                  background: 'var(--bg-surface)',
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: 'var(--sig-green)', fontWeight: 700, fontSize: 11.5 }}>
                    [+] {profile.tier_badge || 'Tier 1 · C++20 Zero-GPU Mode'}
                  </span>
                  <span className="oc-kbd">grid: {profile.sdf_grid_resolution || 104}³</span>
                </div>
                <div style={{ fontSize: 11, color: 'var(--ink-secondary)', marginTop: 4 }}>
                  {profile.summary}
                </div>
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 8 }}>
                <div style={{ padding: 8, border: '1px solid var(--hairline)', borderRadius: 4 }}>
                  <div style={{ fontSize: 10, color: 'var(--ink-muted)' }}>CPU</div>
                  <div style={{ fontSize: 11, fontWeight: 600, marginTop: 2 }}>{cpu.brand || 'x86_64'}</div>
                  <div style={{ fontSize: 10.5, color: 'var(--ink-secondary)' }}>{cpu.logical_cores || 2} threads</div>
                </div>
                <div style={{ padding: 8, border: '1px solid var(--hairline)', borderRadius: 4 }}>
                  <div style={{ fontSize: 10, color: 'var(--ink-muted)' }}>SYSTEM RAM</div>
                  <div style={{ fontSize: 11, fontWeight: 600, marginTop: 2 }}>{mem.total_gb || 0} GB</div>
                  <div style={{ fontSize: 10.5, color: 'var(--ink-secondary)' }}>{mem.available_gb || 0} GB free</div>
                </div>
                <div style={{ padding: 8, border: '1px solid var(--hairline)', borderRadius: 4 }}>
                  <div style={{ fontSize: 10, color: 'var(--ink-muted)' }}>GPU ACCELERATOR</div>
                  <div style={{ fontSize: 11, fontWeight: 600, marginTop: 2, color: gpus.length ? 'var(--sig-green)' : 'var(--sig-amber, #f59e0b)' }}>
                    {gpus.length ? gpus[0].name : '0 GB VRAM (CPU OpenMP)'}
                  </div>
                  <div style={{ fontSize: 10.5, color: 'var(--ink-secondary)' }}>Compiled C++20 (-O3 OpenMP SIMD)</div>
                </div>
              </div>

              <div>
                <div className="field-label" style={{ marginBottom: 6 }}>probed localhost endpoints</div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 6, fontSize: 11 }}>
                  <div style={{ padding: '5px 8px', border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', justifyContent: 'space-between' }}>
                    <span>ollama :11434</span>{serviceStatus(services.ollama)}
                  </div>
                  <div style={{ padding: '5px 8px', border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', justifyContent: 'space-between' }}>
                    <span>lm-studio :1234</span>{serviceStatus(services.lm_studio)}
                  </div>
                  <div style={{ padding: '5px 8px', border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', justifyContent: 'space-between' }}>
                    <span>llama.cpp :8080</span>{serviceStatus(services.llamacpp)}
                  </div>
                  <div style={{ padding: '5px 8px', border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', justifyContent: 'space-between' }}>
                    <span>vllm :8001</span>{serviceStatus(services.vllm)}
                  </div>
                  <div style={{ padding: '5px 8px', border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', justifyContent: 'space-between' }}>
                    <span>comfyui :8188</span>{serviceStatus(services.comfyui)}
                  </div>
                  <div style={{ padding: '5px 8px', border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', justifyContent: 'space-between' }}>
                    <span>c++20 symbolic</span><span style={{ color: 'var(--sig-green)', fontWeight: 600 }}>[on]</span>
                  </div>
                </div>
              </div>

              <div style={{ padding: 10, border: '1px solid var(--hairline)', borderRadius: 4, display: 'flex', flexDirection: 'column', gap: 8 }}>
                <div className="field-label">custom primary router override</div>
                <div className="row-2">
                  <div>
                    <div className="field-label" style={{ marginBottom: 4 }}>provider mode</div>
                    <select
                      className="select-input"
                      value={cfg.provider}
                      onChange={(e) => setCfg({ ...cfg, provider: e.target.value })}
                    >
                      <option value="auto">auto (active swarm pool &rarr; c++20 symbolic)</option>
                      <option value="ollama">ollama local (:11434)</option>
                      <option value="openai_compat">openai-compatible / free-key endpoint</option>
                      <option value="builtin_symbolic">c++20 symbolic architect (offline 0ms)</option>
                    </select>
                  </div>
                  <div>
                    <div className="field-label" style={{ marginBottom: 4 }}>openai / provider base url</div>
                    <input
                      className="text-input"
                      value={cfg.openai_base_url}
                      onChange={(e) => setCfg({ ...cfg, openai_base_url: e.target.value })}
                    />
                  </div>
                </div>
                <div style={{ display: 'flex', justifyContent: 'flex-end' }}>
                  <button className="oc-btn oc-btn-primary" onClick={handleSaveRouter}>
                    [save router config]
                  </button>
                </div>
              </div>

              <div>
                <div className="field-label" style={{ marginBottom: 6 }}>open-weight 3d engines ({models.length})</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
                  {models.map((m) => (
                    <div key={m.id} style={{ padding: 8, border: '1px solid var(--hairline)', borderRadius: 4 }}>
                      <strong style={{ fontSize: 11.5 }}>{m.name}</strong>{' '}
                      <span style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>({m.license} &middot; {m.min_vram_gb}GB VRAM)</span>
                      <div style={{ fontSize: 10.5, color: 'var(--ink-secondary)', marginTop: 2 }}>{m.notes}</div>
                    </div>
                  ))}
                </div>
              </div>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
