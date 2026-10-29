import React, { useState } from 'react';

const MODEL_KITBASH_CHIPS = [
  ['+ max ultra kitbash', 'Add everything needed ultra detail kitbash'],
  ['+ shoulder cannons', 'Add twin shoulder plasma cannons'],
  ['+ damascus sword', 'Add wielded Damascus greatsword'],
  ['+ aegis shield', 'Add sculpted gold-filigree Aegis shield'],
  ['+ arcane crystals', 'Add glowing arcane obsidian crystal clusters'],
  ['+ stone diorama arch', 'Add ancient carved stone diorama arch'],
  ['+ bone horns', 'Add swept striated bone horns'],
  ['+ dragon wings', 'Add anatomical 4-fingered dragon wings'],
  ['+ jet boosters', 'Add twin vectoring serrated jet boosters'],
  ['+ filigree halo', 'Add royal gold-filigree halo crown'],
];

const GAME_STARTERS = [
  {
    label: '▶ dark-fantasy dragon boss rpg',
    prompt: 'Citadel Wyvern Siege: Dark-Fantasy Cyber-Knight vs Elder Dragon Boss in volcanic ruins',
  },
  {
    label: '▶ tactical mecha 3d shooter',
    prompt: 'Protocol Gundanium: Tactical 3D Mecha Shooter with defense turrets and storm rain',
  },
  {
    label: '▶ midnight gt hypercar racer',
    prompt: 'Midnight GT: Widebody Carbon Hypercar Drift & Nitro Overdrive Circuit',
  },
  {
    label: '▶ starfighter space dogfight',
    prompt: 'Vector Strike: 3D Stealth Starfighter Dogfight in Dreadnought Asteroid Sector',
  },
];

const GAME_MUTATORS = [
  ['+ ai defense turrets', 'Add autonomous AI plasma defense turrets'],
  ['+ magma dragon boss', 'Summon colossal Elder Magma Wyvern boss'],
  ['+ stunt jump ramps', 'Add aero stunt launch ramps and boost pads'],
  ['+ storm rain', 'Add storm rain and wet cyberpunk reflections'],
  ['+ volcanic embers', 'Ignite volcanic fire embers and crimson sky'],
  ['+ citadel fortresses', 'Add inner-ring medieval bastion fortresses'],
  ['+ plasma railgun', 'Equip rapid-fire Plasma Railgun'],
  ['+ damascus katana', 'Equip Damascus Energy Katana combo arc'],
  ['+ quad-jump + nitro', 'Unlock quad-jump thrusters and +35% speed boost'],
];

export default function SwarmPanel({
  busy,
  stageMode,
  setStageMode,
  currentAsset,
  currentGame,
  onRunModelDetail,
  onCreateGame,
  onMutateGame,
  onExportGameHtml,
  onExportGameCs,
}) {
  const [customDetail, setCustomDetail] = useState('');
  const [customGamePrompt, setCustomGamePrompt] = useState('');

  const activeTrace =
    stageMode === 'game' && currentGame?.agent_trace?.length
      ? currentGame.agent_trace
      : currentAsset?.agent_trace || [];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
      {/* Mode Switcher inside Swarm Panel */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
        <button
          className={`oc-btn oc-btn-sm ${stageMode === 'model' ? 'oc-btn-primary' : ''}`}
          onClick={() => setStageMode('model')}
        >
          3D Asset Swarm
        </button>
        <button
          className={`oc-btn oc-btn-sm ${stageMode === 'game' ? 'oc-btn-primary' : ''}`}
          onClick={() => setStageMode('game')}
        >
          3D Game Swarm
        </button>
      </div>

      {/* Live 6-Agent Subagent Pipeline Trace */}
      <div className="field-group">
        <div className="field-label">
          <span>6-Agent Pipeline</span>
          <span style={{ color: 'var(--sig-green)', textTransform: 'none' }}>Active</span>
        </div>

        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            gap: 5,
            maxHeight: 205,
            overflowY: 'auto',
            border: '1px solid var(--hairline)',
            borderRadius: 4,
            padding: 7,
            background: 'var(--bg-void)',
          }}
        >
          {activeTrace.length > 0 ? (
            activeTrace.map((t, idx) => (
              <div
                key={idx}
                style={{
                  borderBottom: idx < activeTrace.length - 1 ? '1px solid var(--hairline)' : 'none',
                  paddingBottom: idx < activeTrace.length - 1 ? 5 : 0,
                  fontSize: 10.5,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', fontWeight: 600 }}>
                  <span style={{ color: 'var(--ink-primary)' }}>
                    <span style={{ color: 'var(--sig-green)' }}>● </span>
                    {t.agent}
                  </span>
                  <span style={{ color: 'var(--ink-muted)' }}>{t.duration_ms}ms</span>
                </div>
                <div style={{ color: 'var(--ink-secondary)', marginTop: 2, lineHeight: 1.35 }}>{t.summary}</div>
              </div>
            ))
          ) : (
            <div style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>
              Subagents ready: @lead-architect · @kitbash-detailer · @sculptor-csg · @material-pbr · @rig-kinematics ·
              @game-director · @qa-inspector.
            </div>
          )}
        </div>
      </div>

      {stageMode === 'model' ? (
        <>
          {/* Ultra-Detail & "Add Everything Needed" 3D Asset Swarm */}
          <div className="field-group">
            <div className="field-label">
              <span>@kitbash-detailer · add anything needed</span>
            </div>
            <div style={{ display: 'flex', gap: 5 }}>
              <input
                className="oc-composer-input"
                style={{
                  border: '1px solid var(--hairline-strong)',
                  borderRadius: 4,
                  padding: '5px 8px',
                  fontSize: 11,
                }}
                value={customDetail}
                onChange={(e) => setCustomDetail(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter' && customDetail.trim()) {
                    onRunModelDetail(customDetail.trim());
                    setCustomDetail('');
                  }
                }}
                placeholder='e.g. "add shoulder cannons, crystals & arch"'
              />
              <button
                className="oc-btn oc-btn-primary oc-btn-sm"
                disabled={busy || !customDetail.trim()}
                onClick={() => {
                  onRunModelDetail(customDetail.trim());
                  setCustomDetail('');
                }}
              >
                +add
              </button>
            </div>

            <div className="chip-grid" style={{ marginTop: 6 }}>
              {MODEL_KITBASH_CHIPS.map(([label, cmd]) => (
                <button
                  key={label}
                  className="chip"
                  disabled={busy || !currentAsset}
                  onClick={() => onRunModelDetail(cmd)}
                >
                  {label}
                </button>
              ))}
            </div>
          </div>
        </>
      ) : (
        <>
          {/* Multi-Agent 3D Game Creator & Live World Co-Pilot */}
          <div className="field-group">
            <div className="field-label">
              <span>@game-director · create new 3d game</span>
            </div>
            <div style={{ display: 'flex', gap: 5 }}>
              <input
                className="oc-composer-input"
                style={{
                  border: '1px solid var(--hairline-strong)',
                  borderRadius: 4,
                  padding: '5px 8px',
                  fontSize: 11,
                }}
                value={customGamePrompt}
                onChange={(e) => setCustomGamePrompt(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter' && customGamePrompt.trim()) {
                    onCreateGame(customGamePrompt.trim());
                    setCustomGamePrompt('');
                  }
                }}
                placeholder='Describe any 3D game to build...'
              />
              <button
                className="oc-btn oc-btn-primary oc-btn-sm"
                disabled={busy || !customGamePrompt.trim()}
                onClick={() => {
                  onCreateGame(customGamePrompt.trim());
                  setCustomGamePrompt('');
                }}
              >
                build
              </button>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: 4, marginTop: 6 }}>
              {GAME_STARTERS.map((gs) => (
                <button
                  key={gs.label}
                  className="oc-btn oc-btn-sm"
                  style={{ justifyContent: 'flex-start' }}
                  disabled={busy}
                  onClick={() => onCreateGame(gs.prompt)}
                >
                  {gs.label}
                </button>
              ))}
            </div>
          </div>

          {currentGame && (
            <div className="field-group">
              <div className="field-label">
                <span>live world mutator ({currentGame.title?.slice(0, 18)})</span>
              </div>
              <div className="chip-grid">
                {GAME_MUTATORS.map(([label, cmd]) => (
                  <button key={label} className="chip" disabled={busy} onClick={() => onMutateGame(cmd)}>
                    {label}
                  </button>
                ))}
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5, marginTop: 8 }}>
                <button className="oc-btn oc-btn-primary oc-btn-sm" onClick={onExportGameHtml}>
                  [↓ playable .html]
                </button>
                <button className="oc-btn oc-btn-sm" onClick={onExportGameCs}>
                  [↓ unity .cs]
                </button>
              </div>
            </div>
          )}
        </>
      )}
    </div>
  );
}
