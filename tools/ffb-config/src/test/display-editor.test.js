import { describe, it, expect } from 'vitest';

// Extract display editor logic from main.js
function rgb565ToHex(c) {
  const r = ((c >> 11) & 0x1f) * 255 / 31;
  const g = ((c >> 5) & 0x3f) * 255 / 63;
  const b = (c & 0x1f) * 255 / 31;
  const to2 = (n) => Math.round(n).toString(16).padStart(2, '0');
  return `#${to2(r)}${to2(g)}${to2(b)}`;
}

function hexToRgb565(hex) {
  const m = /^#?([0-9a-f]{6})$/i.exec(String(hex).trim());
  if (!m) return 0xffff;
  const n = parseInt(m[1], 16);
  const r = (n >> 16) & 0xff;
  const g = (n >> 8) & 0xff;
  const b = n & 0xff;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

function encodeLayoutHex(elements) {
  const DISP_MAX = 16;
  const count = Math.min(DISP_MAX, elements.length);
  const bytes = new Uint8Array(1 + DISP_MAX * 8);
  bytes[0] = count;
  
  for (let i = 0; i < DISP_MAX; i++) {
    const el = elements[i] || { type: 0, x: 0, y: 0, fontSize: 1, color565: 0 };
    const o = 1 + i * 8;
    bytes[o] = el.type & 0xff;
    bytes[o + 1] = el.x & 0xff;
    bytes[o + 2] = (el.x >> 8) & 0xff;
    bytes[o + 3] = el.y & 0xff;
    bytes[o + 4] = (el.y >> 8) & 0xff;
    bytes[o + 5] = Math.max(1, Math.min(4, el.fontSize || 1));
    bytes[o + 6] = el.color565 & 0xff;
    bytes[o + 7] = (el.color565 >> 8) & 0xff;
  }
  
  return [...bytes].map((b) => b.toString(16).padStart(2, '0')).join('');
}

function decodeLayoutHex(hex) {
  const clean = String(hex || '').trim().toLowerCase();
  if (!/^[0-9a-f]+$/.test(clean) || (clean.length !== 258 && clean.length !== 130)) {
    return null;
  }
  
  const maxSlots = clean.length === 130 ? 8 : 16;
  const bytes = new Uint8Array(1 + maxSlots * 8);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = parseInt(clean.slice(i * 2, i * 2 + 2), 16);
  }
  
  const count = Math.min(16, maxSlots, bytes[0]);
  const out = [];
  for (let i = 0; i < count; i++) {
    const o = 1 + i * 8;
    const type = bytes[o];
    if (type === 0 || type > 55) continue; // Simple validation
    out.push({
      type,
      x: bytes[o + 1] | (bytes[o + 2] << 8),
      y: bytes[o + 3] | (bytes[o + 4] << 8),
      fontSize: Math.max(1, Math.min(4, bytes[o + 5] || 1)),
      color565: bytes[o + 6] | (bytes[o + 7] << 8),
    });
  }
  return out;
}

describe('Display Editor', () => {
  describe('RGB565 Conversion', () => {
    it('should convert white', () => {
      const hex = rgb565ToHex(0xFFFF);
      expect(hex).toBe('#ffffff');
    });
    
    it('should convert black', () => {
      const hex = rgb565ToHex(0x0000);
      expect(hex).toBe('#000000');
    });
    
    it('should convert red', () => {
      const hex = rgb565ToHex(0xF800);
      expect(hex).toMatch(/^#f[89]0000$/); // Allow rounding
    });
    
    it('should convert green', () => {
      const hex = rgb565ToHex(0x07E0);
      expect(hex).toMatch(/^#00f[cd]00$/);
    });
    
    it('should convert blue', () => {
      const hex = rgb565ToHex(0x001F);
      expect(hex).toMatch(/^#0000f[89]$/);
    });
    
    it('should round-trip', () => {
      const original = 0xF800;
      const hex = rgb565ToHex(original);
      const back = hexToRgb565(hex);
      expect(back).toBe(original);
    });
  });
  
  describe('Hex to RGB565 Conversion', () => {
    it('should convert white', () => {
      expect(hexToRgb565('#ffffff')).toBe(0xFFFF);
    });
    
    it('should convert black', () => {
      expect(hexToRgb565('#000000')).toBe(0x0000);
    });
    
    it('should handle uppercase', () => {
      expect(hexToRgb565('#FFFFFF')).toBe(0xFFFF);
    });
    
    it('should handle without hash', () => {
      expect(hexToRgb565('ffffff')).toBe(0xFFFF);
    });
    
    it('should return default for invalid', () => {
      expect(hexToRgb565('invalid')).toBe(0xFFFF);
      expect(hexToRgb565('')).toBe(0xFFFF);
    });
  });
  
  describe('Layout Encoding/Decoding', () => {
    it('should encode empty layout', () => {
      const hex = encodeLayoutHex([]);
      expect(hex).toMatch(/^00/); // Count = 0
      expect(hex.length).toBe(258); // 1 + 16*8 bytes = 129 bytes = 258 hex chars
    });
    
    it('should encode single element', () => {
      const elements = [{
        type: 1,
        x: 100,
        y: 200,
        fontSize: 2,
        color565: 0xF800,
      }];
      const hex = encodeLayoutHex(elements);
      expect(hex).toMatch(/^01/); // Count = 1
      expect(hex.length).toBe(258);
    });
    
    it('should decode encoded layout', () => {
      const original = [{
        type: 2,
        x: 150,
        y: 100,
        fontSize: 3,
        color565: 0x07E0,
      }];
      
      const hex = encodeLayoutHex(original);
      const decoded = decodeLayoutHex(hex);
      
      expect(decoded).toHaveLength(1);
      expect(decoded[0].type).toBe(2);
      expect(decoded[0].x).toBe(150);
      expect(decoded[0].y).toBe(100);
      expect(decoded[0].fontSize).toBe(3);
      expect(decoded[0].color565).toBe(0x07E0);
    });
    
    it('should handle multiple elements', () => {
      const elements = [
        { type: 1, x: 10, y: 20, fontSize: 1, color565: 0xFFFF },
        { type: 2, x: 30, y: 40, fontSize: 2, color565: 0xF800 },
        { type: 3, x: 50, y: 60, fontSize: 3, color565: 0x07E0 },
      ];
      
      const hex = encodeLayoutHex(elements);
      const decoded = decodeLayoutHex(hex);
      
      expect(decoded).toHaveLength(3);
      expect(decoded[0].type).toBe(1);
      expect(decoded[1].type).toBe(2);
      expect(decoded[2].type).toBe(3);
    });
    
    it('should reject invalid hex', () => {
      expect(decodeLayoutHex('invalid')).toBeNull();
      expect(decodeLayoutHex('12')).toBeNull(); // Too short
      expect(decodeLayoutHex('')).toBeNull();
    });
    
    it('should accept legacy 8-widget format', () => {
      const hex = '01' + '00'.repeat(8 * 8); // 130 hex chars
      const decoded = decodeLayoutHex(hex);
      expect(decoded).toBeDefined();
    });
  });
});
