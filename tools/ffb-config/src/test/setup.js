// Test setup for Vitest
import { expect } from 'vitest';

// Mock Tauri APIs
window.__TAURI__ = {
  core: {
    invoke: async () => ({}),
  },
  event: {
    listen: async () => () => {},
  },
};
