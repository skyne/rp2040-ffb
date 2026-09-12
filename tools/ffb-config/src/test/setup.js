// Test setup for Vitest
import { expect, afterEach } from 'vitest';
import { cleanup } from '@testing-library/dom';

// Cleanup after each test
afterEach(() => {
  cleanup();
});

// Mock Tauri APIs
window.__TAURI__ = {
  core: {
    invoke: async () => ({}),
  },
  event: {
    listen: async () => () => {},
  },
};
