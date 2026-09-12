import { describe, it, expect } from 'vitest';

// Extract utility functions from main.js
function clampf(v, lo, hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

function formatLapTime(ms) {
  if (!ms) return '--:--.---';
  const sec = Math.floor(ms / 1000);
  const rem = ms % 1000;
  const min = Math.floor(sec / 60);
  const s = sec % 60;
  return `${min}:${String(s).padStart(2, '0')}.${String(rem).padStart(3, '0')}`;
}

function formatGap(ms, prefix = '') {
  if (ms == null || ms === -32768) return `${prefix}--.-s`;
  const abs = Math.abs(ms);
  const whole = Math.floor(abs / 1000);
  const frac = Math.floor((abs % 1000) / 10);
  return `${prefix}${ms < 0 ? '-' : '+'}${whole}.${String(frac).padStart(2, '0')}s`;
}

function parseKvMap(lines) {
  const map = {};
  for (const line of lines) {
    if (line.startsWith('OK') || line.startsWith('ERR')) {
      continue;
    }
    const [k, v] = line.split('=');
    if (k && v) {
      map[k.trim()] = v.trim();
    }
  }
  return map;
}

describe('Utility Functions', () => {
  describe('clampf', () => {
    it('should return value in range', () => {
      expect(clampf(5, 0, 10)).toBe(5);
      expect(clampf(0, 0, 10)).toBe(0);
      expect(clampf(10, 0, 10)).toBe(10);
    });
    
    it('should clamp below minimum', () => {
      expect(clampf(-5, 0, 10)).toBe(0);
    });
    
    it('should clamp above maximum', () => {
      expect(clampf(15, 0, 10)).toBe(10);
    });
    
    it('should handle same min/max', () => {
      expect(clampf(5, 10, 10)).toBe(10);
    });
  });
  
  describe('formatLapTime', () => {
    it('should format zero time', () => {
      expect(formatLapTime(0)).toBe('--:--.---');
    });
    
    it('should format seconds only', () => {
      expect(formatLapTime(5000)).toBe('0:05.000');
    });
    
    it('should format with milliseconds', () => {
      expect(formatLapTime(5123)).toBe('0:05.123');
    });
    
    it('should format minutes', () => {
      expect(formatLapTime(65000)).toBe('1:05.000');
    });
    
    it('should format full lap time', () => {
      expect(formatLapTime(83456)).toBe('1:23.456');
    });
    
    it('should handle null', () => {
      expect(formatLapTime(null)).toBe('--:--.---');
    });
  });
  
  describe('formatGap', () => {
    it('should format positive gap', () => {
      expect(formatGap(1234)).toBe('+1.23s');
    });
    
    it('should format negative gap', () => {
      expect(formatGap(-1234)).toBe('-1.23s');
    });
    
    it('should handle no-data sentinel', () => {
      expect(formatGap(-32768)).toBe('--.-s');
    });
    
    it('should handle null', () => {
      expect(formatGap(null)).toBe('--.-s');
    });
    
    it('should add prefix', () => {
      expect(formatGap(1234, 'PB ')).toBe('PB +1.23s');
    });
    
    it('should round fractional seconds', () => {
      expect(formatGap(1298)).toBe('+1.29s');
    });
  });
  
  describe('parseKvMap', () => {
    it('should parse key-value lines', () => {
      const lines = [
        'duty_cap=0.350000',
        'spring_k=0.004000',
        'hid_range=900.00',
      ];
      const map = parseKvMap(lines);
      
      expect(map.duty_cap).toBe('0.350000');
      expect(map.spring_k).toBe('0.004000');
      expect(map.hid_range).toBe('900.00');
    });
    
    it('should skip OK/ERR lines', () => {
      const lines = [
        'OK dump',
        'value=123',
        'ERR failed',
        'OK end',
      ];
      const map = parseKvMap(lines);
      
      expect(map.value).toBe('123');
      expect(map.OK).toBeUndefined();
      expect(map.ERR).toBeUndefined();
    });
    
    it('should handle empty lines', () => {
      const lines = ['', 'value=123', ''];
      const map = parseKvMap(lines);
      
      expect(map.value).toBe('123');
      expect(Object.keys(map)).toHaveLength(1);
    });
    
    it('should trim whitespace', () => {
      const lines = ['  key  =  value  '];
      const map = parseKvMap(lines);
      
      expect(map.key).toBe('value');
    });
  });
});
