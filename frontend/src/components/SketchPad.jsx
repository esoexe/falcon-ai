import React, { useEffect, useRef, useState } from 'react';
import { Brush, Eraser, Trash2, FlipHorizontal, Sparkles } from 'lucide-react';

const SWATCHES = ['#38bdf8', '#eab308', '#ef4444', '#10b981', '#a855f7', '#f97316', '#e2e8f0', '#334155'];

export default function SketchPad({ onGenerateFromSketch, busy }) {
  const canvasRef = useRef(null);
  const [color, setColor] = useState('#38bdf8');
  const [brushSize, setBrushSize] = useState(14);
  const [eraser, setEraser] = useState(false);
  const [mirrorX, setMirrorX] = useState(true);
  const drawingRef = useRef(false);
  const lastPtRef = useRef(null);

  useEffect(() => {
    drawPresetStamp('shield');
  }, []);

  const clearCanvas = () => {
    const c = canvasRef.current;
    if (!c) return;
    const ctx = c.getContext('2d');
    ctx.clearRect(0, 0, c.width, c.height);
  };

  const drawPresetStamp = (kind) => {
    const c = canvasRef.current;
    if (!c) return;
    const ctx = c.getContext('2d');
    const w = c.width;
    const h = c.height;
    ctx.clearRect(0, 0, w, h);

    if (kind === 'shield') {
      // Arcane Crest Shield
      ctx.fillStyle = '#2563eb';
      ctx.beginPath();
      ctx.moveTo(w * 0.5, h * 0.1);
      ctx.lineTo(w * 0.82, h * 0.22);
      ctx.quadraticCurveTo(w * 0.8, h * 0.68, w * 0.5, h * 0.9);
      ctx.quadraticCurveTo(w * 0.2, h * 0.68, w * 0.18, h * 0.22);
      ctx.closePath();
      ctx.fill();

      ctx.lineWidth = 14;
      ctx.strokeStyle = '#eab308';
      ctx.stroke();

      ctx.fillStyle = '#38bdf8';
      ctx.beginPath();
      ctx.moveTo(w * 0.5, h * 0.3);
      ctx.lineTo(w * 0.63, h * 0.5);
      ctx.lineTo(w * 0.5, h * 0.7);
      ctx.lineTo(w * 0.37, h * 0.5);
      ctx.closePath();
      ctx.fill();
    } else if (kind === 'starblade') {
      ctx.fillStyle = '#cbd5e1';
      ctx.beginPath();
      ctx.moveTo(w * 0.5, h * 0.07);
      ctx.lineTo(w * 0.58, h * 0.64);
      ctx.lineTo(w * 0.42, h * 0.64);
      ctx.closePath();
      ctx.fill();

      ctx.fillStyle = '#eab308';
      ctx.fillRect(w * 0.26, h * 0.62, w * 0.48, h * 0.08);
      ctx.fillStyle = '#7c2d12';
      ctx.fillRect(w * 0.45, h * 0.7, w * 0.1, h * 0.18);
      ctx.fillStyle = '#22d3ee';
      ctx.beginPath();
      ctx.arc(w * 0.5, h * 0.66, 15, 0, Math.PI * 2);
      ctx.fill();
    } else if (kind === 'creature') {
      ctx.fillStyle = '#10b981';
      ctx.beginPath();
      ctx.ellipse(w * 0.5, h * 0.54, w * 0.26, h * 0.3, 0, 0, Math.PI * 2);
      ctx.fill();
      // Ears / horns
      ctx.fillStyle = '#eab308';
      ctx.beginPath();
      ctx.moveTo(w * 0.32, h * 0.32);
      ctx.lineTo(w * 0.2, h * 0.12);
      ctx.lineTo(w * 0.42, h * 0.25);
      ctx.fill();
      ctx.beginPath();
      ctx.moveTo(w * 0.68, h * 0.32);
      ctx.lineTo(w * 0.8, h * 0.12);
      ctx.lineTo(w * 0.58, h * 0.25);
      ctx.fill();
      // Eyes
      ctx.fillStyle = '#ffffff';
      ctx.beginPath();
      ctx.arc(w * 0.4, h * 0.46, 16, 0, Math.PI * 2);
      ctx.arc(w * 0.6, h * 0.46, 16, 0, Math.PI * 2);
      ctx.fill();
    }
  };

  const getCoords = (e) => {
    const c = canvasRef.current;
    const rect = c.getBoundingClientRect();
    return {
      x: ((e.clientX - rect.left) / rect.width) * c.width,
      y: ((e.clientY - rect.top) / rect.height) * c.height,
    };
  };

  const strokeSegment = (p0, p1) => {
    const c = canvasRef.current;
    const ctx = c.getContext('2d');
    ctx.save();
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.lineWidth = brushSize;
    if (eraser) {
      ctx.globalCompositeOperation = 'destination-out';
    } else {
      ctx.globalCompositeOperation = 'source-over';
      ctx.strokeStyle = color;
    }

    ctx.beginPath();
    ctx.moveTo(p0.x, p0.y);
    ctx.lineTo(p1.x, p1.y);
    ctx.stroke();

    if (mirrorX) {
      ctx.beginPath();
      ctx.moveTo(c.width - p0.x, p0.y);
      ctx.lineTo(c.width - p1.x, p1.y);
      ctx.stroke();
    }
    ctx.restore();
  };

  const handleDown = (e) => {
    drawingRef.current = true;
    const pt = getCoords(e);
    lastPtRef.current = pt;
    strokeSegment(pt, pt);
  };

  const handleMove = (e) => {
    if (!drawingRef.current) return;
    const pt = getCoords(e);
    strokeSegment(lastPtRef.current || pt, pt);
    lastPtRef.current = pt;
  };

  const handleUp = () => {
    drawingRef.current = false;
    lastPtRef.current = null;
  };

  const submitSketch = () => {
    const c = canvasRef.current;
    if (!c) return;
    c.toBlob((blob) => {
      if (blob) {
        onGenerateFromSketch(new File([blob], 'sketch_pad.png', { type: 'image/png' }));
      }
    }, 'image/png');
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div style={{ display: 'flex', gap: 4 }}>
          <button
            type="button"
            className={`btn btn-sm ${!eraser ? 'btn-primary' : ''}`}
            onClick={() => setEraser(false)}
          >
            <Brush size={12} /> Brush
          </button>
          <button
            type="button"
            className={`btn btn-sm ${eraser ? 'btn-primary' : ''}`}
            onClick={() => setEraser(true)}
          >
            <Eraser size={12} /> Erase
          </button>
          <button
            type="button"
            className={`btn btn-sm ${mirrorX ? 'btn-emerald' : ''}`}
            onClick={() => setMirrorX(!mirrorX)}
            title="Mirror X Symmetry"
          >
            <FlipHorizontal size={12} /> Sym
          </button>
        </div>
        <button type="button" className="btn btn-sm" onClick={clearCanvas}>
          <Trash2 size={12} /> Clear
        </button>
      </div>

      <div
        style={{
          position: 'relative',
          borderRadius: 8,
          overflow: 'hidden',
          border: '1px solid var(--border-strong)',
          backgroundImage:
            'linear-gradient(45deg, #131926 25%, transparent 25%), linear-gradient(-45deg, #131926 25%, transparent 25%), linear-gradient(45deg, transparent 75%, #131926 75%), linear-gradient(-45deg, transparent 75%, #131926 75%)',
          backgroundSize: '16px 16px',
          backgroundPosition: '0 0, 0 8px, 8px -8px, -8px 0px',
          backgroundColor: '#0c1018',
        }}
      >
        {mirrorX && (
          <div
            style={{
              position: 'absolute',
              top: 0,
              bottom: 0,
              left: '50%',
              borderLeft: '1px dashed rgba(56, 189, 248, 0.35)',
              pointerEvents: 'none',
            }}
          />
        )}
        <canvas
          ref={canvasRef}
          width={320}
          height={320}
          onPointerDown={handleDown}
          onPointerMove={handleMove}
          onPointerUp={handleUp}
          onPointerLeave={handleUp}
          style={{ width: '100%', height: 230, display: 'block', cursor: 'crosshair', touchAction: 'none' }}
        />
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: 6, justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', gap: 5 }}>
          {SWATCHES.map((sw) => (
            <button
              key={sw}
              type="button"
              onClick={() => {
                setColor(sw);
                setEraser(false);
              }}
              style={{
                width: 20,
                height: 20,
                borderRadius: 5,
                background: sw,
                border: color === sw && !eraser ? '2px solid #fff' : '1px solid rgba(255,255,255,0.2)',
                cursor: 'pointer',
              }}
            />
          ))}
        </div>
        <div style={{ width: 85, display: 'flex', alignItems: 'center', gap: 4 }}>
          <span style={{ fontSize: 10, color: 'var(--text-muted)' }}>{brushSize}px</span>
          <input
            type="range"
            min="4"
            max="36"
            value={brushSize}
            onChange={(e) => setBrushSize(Number(e.target.value))}
          />
        </div>
      </div>

      <div style={{ display: 'flex', gap: 5, alignItems: 'center' }}>
        <span style={{ fontSize: 10.5, color: 'var(--text-muted)' }}>Stamps:</span>
        <button type="button" className="chip" onClick={() => drawPresetStamp('shield')}>
          Crest Shield
        </button>
        <button type="button" className="chip" onClick={() => drawPresetStamp('starblade')}>
          Starblade
        </button>
        <button type="button" className="chip" onClick={() => drawPresetStamp('creature')}>
          Monster
        </button>
      </div>

      <button type="button" className="btn btn-primary" disabled={busy} onClick={submitSketch}>
        <Sparkles size={14} /> Inflate 2D Sketch to 3D Mesh
      </button>
    </div>
  );
}
