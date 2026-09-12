import { describe, it, expect } from 'vitest';

// Extract telemetry parsing logic from main.js
function parseTelemetry(line) {
  if (!line.startsWith('T ')) return null;
  
  const body = line.substring(2);
  const map = new Map();
  
  for (const part of body.split(/\s+/)) {
    const [k, v] = part.split('=');
    if (k && v) {
      map.set(k, v);
    }
  }
  
  const parseF64 = (key) => parseFloat(map.get(key)) || 0.0;
  const parseInt32 = (key) => parseInt(map.get(key), 10) || 0;
  const parseBool = (key) => map.get(key) !== '0';
  
  return {
    axle: parseF64('axle'),
    hidX: parseInt32('hidX'),
    sens: parseF64('sens'),
    hall: parseBool('hall'),
    idx: parseBool('idx'),
    idxH: parseBool('idxH'),
    edges: parseInt32('edges'),
    home: map.get('home') || '-',
    homeM: parseBool('homeM'),
    adxl: parseBool('adxl'),
    ax: parseInt32('ax'),
    gear: parseF64('gear'),
    motors: parseBool('motors'),
    ffb: map.get('ffb') || '?',
    torq: parseF64('torq'),
    hidRange: parseF64('hidRange'),
    rim: parseBool('rim'),
    adcT: parseInt32('adcT'),
    adcB: parseInt32('adcB'),
    adcC: parseInt32('adcC'),
    nT: parseF64('nT'),
    nB: parseF64('nB'),
    nC: parseF64('nC'),
  };
}

describe('Telemetry Parsing', () => {
  it('should parse valid telemetry line', () => {
    const line = 'T axle=45.2 hidX=100 sens=450.0 hall=1 idx=0 rim=1 motors=0 ffb=Spring torq=0.123';
    const telem = parseTelemetry(line);
    
    expect(telem).toBeDefined();
    expect(telem.axle).toBe(45.2);
    expect(telem.hidX).toBe(100);
    expect(telem.sens).toBe(450.0);
    expect(telem.hall).toBe(true);
    expect(telem.idx).toBe(false);
    expect(telem.rim).toBe(true);
    expect(telem.motors).toBe(false);
    expect(telem.ffb).toBe('Spring');
    expect(telem.torq).toBeCloseTo(0.123, 3);
  });
  
  it('should return null for non-telemetry line', () => {
    const line = 'OK dump';
    const telem = parseTelemetry(line);
    expect(telem).toBeNull();
  });
  
  it('should handle missing fields gracefully', () => {
    const line = 'T axle=10.0';
    const telem = parseTelemetry(line);
    
    expect(telem).toBeDefined();
    expect(telem.axle).toBe(10.0);
    expect(telem.hidX).toBe(0);
    expect(telem.sens).toBe(0.0);
  });
  
  it('should parse boolean fields correctly', () => {
    const line = 'T hall=1 idx=0 rim=1 motors=0 adxl=1';
    const telem = parseTelemetry(line);
    
    expect(telem.hall).toBe(true);
    expect(telem.idx).toBe(false);
    expect(telem.rim).toBe(true);
    expect(telem.motors).toBe(false);
    expect(telem.adxl).toBe(true);
  });
});
