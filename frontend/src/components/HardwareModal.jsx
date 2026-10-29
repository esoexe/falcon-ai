import React, { useEffect, useMemo, useState } from 'react';

const POPULAR_IDS = [
  'openrouter',
  'groq',
  'gemini_studio',
  'github_models',
  'mistral_plateforme',
  'deepseek',
  'cerebras',
  'nvidia_nim',
];

export default function HardwareModal({
  open,
  onClose,
  hardware,
  routerCfg,
  onSaveRouter,
  onRescan,
  gridRes,
  setGridRes,
  theme,
  onToggleTheme,
  initialSection = 'connect',
}) {
  const [section, setSection] = useState(initialSection);
  const [cfg, setCfg] = useState(
    routerCfg || {
      provider: 'auto',
      ollama_url: 'http://127.0.0.1:11434',
      ollama_model: 'llama3.2',
      openai_base_url: 'http://127.0.0.1:1234/v1',
      openai_api_key: '',
      openai_model: 'qwen2.5-coder',
    }
  );

  // Provider catalog state
  const [catalog, setCatalog] = useState({ counts: {}, providers: [] });
  const [freeFilter, setFreeFilter] = useState('all');
  const [search, setSearch] = useState('');
  const [keyDrafts, setKeyDrafts] = useState({});
  const [modelDrafts, setModelDrafts] = useState({});
  const [urlDrafts, setUrlDrafts] = useState({});
  const [busyId, setBusyId] = useState('');
  const [toast, setToast] = useState('');

  // Add Custom Provider form state
  const [customForm, setCustomForm] = useState({
    name: '',
    base_url: 'https://api.openai.com/v1',
    api_key: '',
    model: 'gpt-4o-mini',
  });

  const showToast = (msg) => {
    setToast(msg);
    setTimeout(() => setToast(''), 2600);
  };

  const fetchProviders = async () => {
    try {
      const r = await fetch('/api/providers');
      if (r.ok) {
        const d = await r.json();
        setCatalog(d);
      }
    } catch {
      /* ignore */
    }
  };

  useEffect(() => {
    if (routerCfg) setCfg(routerCfg);
  }, [routerCfg]);

  useEffect(() => {
    if (open) {
      setSection(initialSection || 'connect');
      fetchProviders();
    }
  }, [open, initialSection]);

  const updateProvider = async (payload, successMsg) => {
    setBusyId(payload.id || payload.action || 'save');
    try {
      const r = await fetch('/api/providers', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      if (r.ok) {
        const d = await r.json();
        setCatalog(d);
        if (payload.id) {
          setKeyDrafts((prev) => ({ ...prev, [payload.id]: '' }));
        }
        if (successMsg) showToast(successMsg);
      }
    } catch {
      /* ignore */
    } finally {
      setBusyId('');
    }
  };

  const handleAddCustomProvider = async (e) => {
    e.preventDefault();
    if (!customForm.base_url.trim()) return;
    const name = customForm.name.trim() || 'Custom OpenAI Endpoint';
    await updateProvider(
      {
        action: 'add_custom',
        name,
        base_url: customForm.base_url.trim(),
        api_key: customForm.api_key.trim(),
        model: customForm.model.trim() || 'default',
      },
      `Connected ${name}`
    );
    // Also sync direct OpenAI-compatible router fields for convenience
    const nextCfg = {
      ...cfg,
      openai_base_url: customForm.base_url.trim(),
      openai_api_key: customForm.api_key.trim() || cfg.openai_api_key,
      openai_model: customForm.model.trim() || cfg.openai_model,
    };
    setCfg(nextCfg);
    onSaveRouter?.(nextCfg);
    setCustomForm({ name: '', base_url: '', api_key: '', model: '' });
  };

  const customProviders = useMemo(
    () => (catalog.providers || []).filter((p) => p.group === 'custom'),
    [catalog.providers]
  );

  const popularProviders = useMemo(() => {
    const byId = new Map((catalog.providers || []).map((p) => [p.id, p]));
    return POPULAR_IDS.map((id) => byId.get(id)).filter(Boolean);
  }, [catalog.providers]);

  const filteredFree = useMemo(() => {
    const q = search.trim().toLowerCase();
    return (catalog.providers || []).filter((p) => {
      if (p.group === 'paid' || p.group === 'local' || p.group === 'custom') return false;
      if (freeFilter !== 'all' && p.group !== freeFilter) return false;
      if (!q) return true;
      return (
        p.name.toLowerCase().includes(q) ||
        p.model.toLowerCase().includes(q) ||
        p.limit_notes.toLowerCase().includes(q)
      );
    });
  }, [catalog.providers, freeFilter, search]);

  const localCatalog = useMemo(
    () => (catalog.providers || []).filter((p) => p.group === 'local'),
    [catalog.providers]
  );

  const paidProviders = useMemo(() => {
    const q = search.trim().toLowerCase();
    return (catalog.providers || []).filter((p) => {
      if (p.group !== 'paid') return false;
      if (!q) return true;
      return p.name.toLowerCase().includes(q) || p.model.toLowerCase().includes(q);
    });
  }, [catalog.providers, search]);

  if (!open) return null;

  const hw = hardware || {};
  const counts = catalog.counts || {};
  const localRouters = hw.local_llm_routers || [];

  const renderProviderRow = (p) => {
    const draftKey = keyDrafts[p.id] ?? '';
    const draftModel = modelDrafts[p.id] ?? p.model;
    const draftUrl = urlDrafts[p.id] ?? p.base_url;
    const needsKey = p.group === 'free_key' || p.group === 'paid';

    return (
      <div
        key={p.id}
        className="oc-card"
        style={{
          borderColor: p.ready_in_team ? 'rgba(48, 209, 88, 0.36)' : 'var(--hairline)',
        }}
      >
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: 10 }}>
          <div style={{ minWidth: 0 }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
              <span style={{ fontWeight: 600, fontSize: 13 }}>{p.name}</span>
              {p.ready_in_team && (
                <span
                  style={{
                    fontSize: 10.5,
                    fontFamily: 'var(--font-mono)',
                    color: 'var(--sig-green)',
                    padding: '1px 6px',
                    borderRadius: 4,
                    background: 'rgba(48, 209, 88, 0.12)',
                  }}
                >
                  Connected
                </span>
              )}
              {p.group === 'no_key' && (
                <span className="oc-kbd" style={{ color: 'var(--ink-secondary)' }}>
                  No key needed
                </span>
              )}
            </div>
            <div style={{ fontSize: 11.5, color: 'var(--ink-muted)', marginTop: 2 }}>
              {p.limit_notes} {p.site && p.site !== 'custom' ? `· ${p.site}` : ''}
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: 6, flexShrink: 0 }}>
            {p.is_custom && (
              <button
                className="oc-btn oc-btn-sm"
                style={{ color: 'var(--sig-red)' }}
                onClick={() => updateProvider({ action: 'delete_custom', id: p.id }, 'Removed custom provider')}
              >
                Delete
              </button>
            )}
            <button
              className={`oc-btn oc-btn-sm ${p.enabled ? 'oc-btn-primary' : ''}`}
              disabled={busyId === p.id}
              onClick={() =>
                updateProvider(
                  { id: p.id, enabled: !p.enabled },
                  !p.enabled ? `Enabled ${p.name}` : `Disabled ${p.name}`
                )
              }
            >
              {p.enabled ? 'Enabled' : 'Enable'}
            </button>
          </div>
        </div>

        {/* Inline API Key + Model configuration */}
        <div style={{ display: 'grid', gridTemplateColumns: needsKey || p.is_custom ? '1.3fr 1fr auto' : '1fr auto', gap: 6 }}>
          {(needsKey || p.is_custom) && (
            <input
              type="password"
              className="text-input"
              placeholder={
                p.has_key ? `Saved key (${p.key_preview}) — paste new to replace` : 'Paste API key (sk-...)'
              }
              value={draftKey}
              onChange={(e) => setKeyDrafts((prev) => ({ ...prev, [p.id]: e.target.value }))}
            />
          )}
          <input
            className="text-input"
            style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
            placeholder="Model ID"
            value={draftModel}
            onChange={(e) => setModelDrafts((prev) => ({ ...prev, [p.id]: e.target.value }))}
          />
          <div style={{ display: 'flex', gap: 5 }}>
            <button
              className="oc-btn"
              disabled={busyId === p.id}
              onClick={() =>
                updateProvider(
                  {
                    id: p.id,
                    ...(draftKey.trim() ? { api_key: draftKey.trim(), enabled: true } : {}),
                    model: draftModel.trim() || p.model,
                    ...(p.is_custom ? { base_url: draftUrl.trim() || p.base_url } : {}),
                  },
                  `Saved ${p.name}`
                )
              }
            >
              {p.has_key && !draftKey.trim() ? 'Save' : 'Connect'}
            </button>
            {p.has_key && (
              <button
                className="oc-btn"
                title="Clear saved API key"
                onClick={() => updateProvider({ id: p.id, api_key: '__CLEAR__' }, `Cleared key for ${p.name}`)}
              >
                Clear
              </button>
            )}
          </div>
        </div>
      </div>
    );
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal-card oc-settings-shell" onClick={(e) => e.stopPropagation()}>
        {/* Top Modal Header */}
        <div className="oc-settings-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
            <span style={{ fontWeight: 600, fontSize: 14 }}>Settings & Providers</span>
            <span className="oc-kbd">{counts.active_in_swarm ?? 16} active</span>
            {toast && (
              <span style={{ fontSize: 11.5, color: 'var(--sig-green)', fontFamily: 'var(--font-mono)' }}>
                {toast}
              </span>
            )}
          </div>
          <button className="oc-icon-btn" onClick={onClose}>
            Done <span className="oc-kbd">Esc</span>
          </button>
        </div>

        {/* 2-Column OpenCode Settings Body */}
        <div className="oc-settings-body">
          {/* Left Navigation */}
          <nav className="oc-settings-nav">
            <button
              className={`oc-settings-nav-item ${section === 'connect' ? 'active' : ''}`}
              onClick={() => setSection('connect')}
            >
              <span>Add & Connect</span>
              <span className="oc-kbd">+</span>
            </button>
            <button
              className={`oc-settings-nav-item ${section === 'free' ? 'active' : ''}`}
              onClick={() => setSection('free')}
            >
              <span>Free Providers</span>
              <span className="oc-kbd">{(counts.no_key ?? 5) + (counts.free_key ?? 52)}</span>
            </button>
            <button
              className={`oc-settings-nav-item ${section === 'local' ? 'active' : ''}`}
              onClick={() => setSection('local')}
            >
              <span>Local Models</span>
              <span className="oc-kbd">{counts.local ?? 11}</span>
            </button>
            <button
              className={`oc-settings-nav-item ${section === 'paid' ? 'active' : ''}`}
              onClick={() => setSection('paid')}
            >
              <span>Paid Providers</span>
              <span className="oc-kbd">{counts.paid_opt_in ?? 30}</span>
            </button>
            <div style={{ height: 1, background: 'var(--hairline)', margin: '6px 4px' }} />
            <button
              className={`oc-settings-nav-item ${section === 'hardware' ? 'active' : ''}`}
              onClick={() => setSection('hardware')}
            >
              <span>Hardware & Studio</span>
            </button>
          </nav>

          {/* Right Content Pane */}
          <div className="oc-settings-content">
            {/* 1. ADD & CONNECT PROVIDERS */}
            {section === 'connect' && (
              <>
                {/* Active Routing Mode */}
                <div className="oc-card">
                  <div className="field-label">
                    <span>Active AI Router</span>
                    <span style={{ textTransform: 'none', fontWeight: 400, color: 'var(--ink-muted)' }}>
                      Controls how Falcon routes 3D & game prompts
                    </span>
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: 8 }}>
                    <select
                      className="select-input"
                      value={cfg.provider}
                      onChange={(e) => {
                        const next = { ...cfg, provider: e.target.value };
                        setCfg(next);
                        onSaveRouter?.(next);
                        showToast('Updated active router');
                      }}
                    >
                      <option value="auto">Auto — Connected Providers + Local Ollama + C++20 Engine</option>
                      <option value="openai_compat">Custom / OpenAI-Compatible Endpoint</option>
                      <option value="ollama">Local Ollama Only ({cfg.ollama_model})</option>
                      <option value="builtin_symbolic">Offline C++20 Parametric Engine Only (0 Network)</option>
                    </select>
                    <button
                      className="oc-btn"
                      onClick={() => {
                        onSaveRouter?.(cfg);
                        showToast('Router settings saved');
                      }}
                    >
                      Save Router
                    </button>
                  </div>
                </div>

                {/* Add Custom OpenAI-Compatible Provider */}
                <form className="oc-card" onSubmit={handleAddCustomProvider}>
                  <div className="field-label">
                    <span>Add Custom Provider (OpenAI-Compatible)</span>
                    <span style={{ textTransform: 'none', fontWeight: 400, color: 'var(--ink-muted)' }}>
                      Works with OpenRouter, LiteLLM, vLLM, LM Studio, DeepInfra, or any /v1 endpoint
                    </span>
                  </div>
                  <div className="row-2">
                    <input
                      className="text-input"
                      placeholder="Provider Name (e.g. My LiteLLM / OpenRouter)"
                      value={customForm.name}
                      onChange={(e) => setCustomForm({ ...customForm, name: e.target.value })}
                    />
                    <input
                      className="text-input"
                      style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
                      placeholder="Base URL (e.g. https://openrouter.ai/api/v1)"
                      value={customForm.base_url}
                      onChange={(e) => setCustomForm({ ...customForm, base_url: e.target.value })}
                    />
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: '1.3fr 1fr auto', gap: 8 }}>
                    <input
                      type="password"
                      className="text-input"
                      placeholder="API Key (optional for local servers)"
                      value={customForm.api_key}
                      onChange={(e) => setCustomForm({ ...customForm, api_key: e.target.value })}
                    />
                    <input
                      className="text-input"
                      style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
                      placeholder="Model ID (e.g. gpt-4o-mini / qwen2.5)"
                      value={customForm.model}
                      onChange={(e) => setCustomForm({ ...customForm, model: e.target.value })}
                    />
                    <button type="submit" className="oc-btn oc-btn-primary">
                      + Add Provider
                    </button>
                  </div>
                </form>

                {/* User-Added Custom Providers */}
                {customProviders.length > 0 && (
                  <div className="field-group">
                    <div className="field-label">
                      <span>Your Custom Providers ({customProviders.length})</span>
                    </div>
                    {customProviders.map(renderProviderRow)}
                  </div>
                )}

                {/* Popular Quick-Connect Cloud Providers */}
                <div className="field-group">
                  <div className="field-label">
                    <span>Quick Connect Popular Providers</span>
                    <button
                      className="oc-icon-btn"
                      style={{ fontSize: 11, textTransform: 'none' }}
                      onClick={() => setSection('free')}
                    >
                      Browse all 68 free providers →
                    </button>
                  </div>
                  {popularProviders.map(renderProviderRow)}
                </div>
              </>
            )}

            {/* 2. FREE PROVIDERS CATALOG */}
            {section === 'free' && (
              <>
                <div style={{ display: 'flex', gap: 8, alignItems: 'center', flexWrap: 'wrap' }}>
                  <input
                    className="text-input"
                    style={{ flex: 1, minWidth: 200 }}
                    placeholder="Search 57 cloud free providers (Groq, Gemini, GitHub, Qwen, Mistral...)"
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                  />
                  <div className="oc-view-pills">
                    <button
                      className={`oc-view-pill ${freeFilter === 'all' ? 'active' : ''}`}
                      onClick={() => setFreeFilter('all')}
                    >
                      All
                    </button>
                    <button
                      className={`oc-view-pill ${freeFilter === 'no_key' ? 'active' : ''}`}
                      onClick={() => setFreeFilter('no_key')}
                    >
                      No Key ({counts.no_key ?? 5})
                    </button>
                    <button
                      className={`oc-view-pill ${freeFilter === 'free_key' ? 'active' : ''}`}
                      onClick={() => setFreeFilter('free_key')}
                    >
                      Free Key ({counts.free_key ?? 52})
                    </button>
                  </div>
                </div>

                <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                  {filteredFree.map(renderProviderRow)}
                </div>
              </>
            )}

            {/* 3. LOCAL AI MODELS (OLLAMA / LM STUDIO / LLAMA.CPP) */}
            {section === 'local' && (
              <>
                <div className="oc-card">
                  <div className="field-label">
                    <span>Local Ollama & OpenAI-Compatible Server</span>
                    <button className="oc-btn oc-btn-sm" onClick={onRescan}>
                      Scan Local Ports
                    </button>
                  </div>

                  <div className="row-2">
                    <div>
                      <div style={{ fontSize: 11, color: 'var(--ink-muted)', marginBottom: 4 }}>Ollama URL</div>
                      <input
                        className="text-input"
                        style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
                        value={cfg.ollama_url}
                        onChange={(e) => setCfg({ ...cfg, ollama_url: e.target.value })}
                      />
                    </div>
                    <div>
                      <div style={{ fontSize: 11, color: 'var(--ink-muted)', marginBottom: 4 }}>Ollama Model</div>
                      <input
                        className="text-input"
                        style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
                        value={cfg.ollama_model}
                        onChange={(e) => setCfg({ ...cfg, ollama_model: e.target.value })}
                      />
                    </div>
                  </div>

                  <div className="row-2">
                    <div>
                      <div style={{ fontSize: 11, color: 'var(--ink-muted)', marginBottom: 4 }}>
                        LM Studio / llama.cpp / vLLM URL
                      </div>
                      <input
                        className="text-input"
                        style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
                        value={cfg.openai_base_url}
                        onChange={(e) => setCfg({ ...cfg, openai_base_url: e.target.value })}
                      />
                    </div>
                    <div>
                      <div style={{ fontSize: 11, color: 'var(--ink-muted)', marginBottom: 4 }}>Local Model ID</div>
                      <input
                        className="text-input"
                        style={{ fontFamily: 'var(--font-mono)', fontSize: 11.5 }}
                        value={cfg.openai_model}
                        onChange={(e) => setCfg({ ...cfg, openai_model: e.target.value })}
                      />
                    </div>
                  </div>

                  <div style={{ display: 'flex', justifyContent: 'flex-end' }}>
                    <button
                      className="oc-btn oc-btn-primary"
                      onClick={() => {
                        onSaveRouter?.(cfg);
                        showToast('Saved local server settings');
                      }}
                    >
                      Save Local Endpoints
                    </button>
                  </div>
                </div>

                {/* Live Port Status */}
                {localRouters.length > 0 && (
                  <div className="field-group">
                    <div className="field-label">
                      <span>Detected Local Daemons</span>
                    </div>
                    <div className="stat-grid-2">
                      {localRouters.map((r) => (
                        <div key={r.name} className="stat-box" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                          <div>
                            <div style={{ fontWeight: 600, fontSize: 12 }}>{r.name}</div>
                            <div style={{ fontSize: 10.5, fontFamily: 'var(--font-mono)', color: 'var(--ink-muted)' }}>
                              {r.endpoint}
                            </div>
                          </div>
                          <span
                            style={{
                              fontSize: 11,
                              fontFamily: 'var(--font-mono)',
                              color: r.online ? 'var(--sig-green)' : 'var(--ink-muted)',
                            }}
                          >
                            {r.online ? 'Online' : 'Offline'}
                          </span>
                        </div>
                      ))}
                    </div>
                  </div>
                )}

                <div className="field-group">
                  <div className="field-label">
                    <span>All Supported Local Runtimes ({localCatalog.length})</span>
                  </div>
                  {localCatalog.map(renderProviderRow)}
                </div>
              </>
            )}

            {/* 4. PAID PROVIDERS (OPT-IN) */}
            {section === 'paid' && (
              <>
                <div style={{ fontSize: 12, color: 'var(--ink-secondary)' }}>
                  Paid providers are disabled by default. Paste an API key below to opt in a specific provider.
                </div>
                <input
                  className="text-input"
                  placeholder="Search paid providers (OpenAI, Anthropic, xAI Grok, Together, Fireworks...)"
                  value={search}
                  onChange={(e) => setSearch(e.target.value)}
                />
                <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                  {paidProviders.map(renderProviderRow)}
                </div>
              </>
            )}

            {/* 5. HARDWARE & STUDIO SETTINGS */}
            {section === 'hardware' && (
              <>
                <div className="stat-grid-2">
                  <div className="stat-box">
                    <div className="k">CPU</div>
                    <div className="v" style={{ fontSize: 12 }}>
                      {hw.cpu?.brand || 'x86_64'} ({hw.cpu?.logical_cores || 4} threads)
                    </div>
                  </div>
                  <div className="stat-box">
                    <div className="k">Memory</div>
                    <div className="v" style={{ fontSize: 12 }}>
                      {hw.ram?.total_gb || 0} GB ({hw.ram?.available_gb || 0} GB free)
                    </div>
                  </div>
                  <div className="stat-box">
                    <div className="k">Compute Tier</div>
                    <div className="v" style={{ fontSize: 12, color: 'var(--sig-green)' }}>
                      {(hw.tier || 'cpu_universal').toUpperCase()}
                    </div>
                  </div>
                  <div className="stat-box">
                    <div className="k">GPU / Fallback</div>
                    <div className="v" style={{ fontSize: 12 }}>
                      {hw.gpu?.has_dedicated_gpu ? hw.gpu.primary_gpu : 'Zero-GPU C++20 OpenMP'}
                    </div>
                  </div>
                </div>

                <div className="oc-card">
                  <div className="field-label">
                    <span>Studio Preferences</span>
                  </div>
                  <div className="row-2">
                    <div>
                      <div style={{ fontSize: 11, color: 'var(--ink-muted)', marginBottom: 4 }}>
                        Sculpting Voxel Resolution ({gridRes}³)
                      </div>
                      <input
                        type="range"
                        min={48}
                        max={128}
                        step={8}
                        value={gridRes}
                        style={{ width: '100%' }}
                        onChange={(e) => setGridRes?.(Number(e.target.value))}
                      />
                    </div>
                    <div>
                      <div style={{ fontSize: 11, color: 'var(--ink-muted)', marginBottom: 4 }}>Appearance</div>
                      <button className="oc-btn" style={{ width: '100%' }} onClick={onToggleTheme}>
                        Theme: {theme === 'dark' ? 'OpenCode Dark' : 'OpenCode Light'}
                      </button>
                    </div>
                  </div>
                </div>

                {(hw.recommended_3d_engines || []).length > 0 && (
                  <div className="field-group">
                    <div className="field-label">
                      <span>Compatible Local 3D Engines</span>
                    </div>
                    {(hw.recommended_3d_engines || []).map((eng) => (
                      <div key={eng.id} className="oc-card" style={{ padding: '9px 12px' }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                          <span style={{ fontWeight: 600, fontSize: 12.5 }}>{eng.name}</span>
                          <span className="oc-kbd">{eng.license}</span>
                        </div>
                        <div style={{ fontSize: 11.5, color: 'var(--ink-muted)' }}>{eng.notes}</div>
                      </div>
                    ))}
                  </div>
                )}
              </>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
