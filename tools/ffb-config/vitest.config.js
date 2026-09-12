import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: ['./src/test/setup.js'],
    exclude: [
      'node_modules/**',
      'dist/**',
      'src-tauri/**',
      'e2e/**', // E2E tests run with Playwright, not Vitest
      '**/*.e2e.{test,spec}.{js,ts}',
    ],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html', 'lcov'],
      exclude: [
        'node_modules/',
        'src-tauri/',
        'src/test/',
        'e2e/',
        '**/*.config.js',
        '**/dist/**',
      ],
    },
  },
});
