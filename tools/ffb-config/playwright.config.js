import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: './e2e',
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 2 : 0,
  workers: process.env.CI ? 1 : undefined,
  reporter: 'html',
  
  use: {
    baseURL: 'tauri://localhost',
    trace: 'on-first-retry',
    screenshot: 'only-on-failure',
  },

  projects: [
    {
      name: 'tauri',
      use: { ...devices['Desktop Chrome'] },
    },
  ],

  webServer: process.env.CI ? undefined : {
    command: 'npm run tauri dev',
    port: 1420,
    timeout: 120 * 1000,
    reuseExistingServer: !process.env.CI,
  },
});
