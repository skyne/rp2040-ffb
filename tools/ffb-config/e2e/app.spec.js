import { test, expect } from '@playwright/test';

test.describe('FFB Config App', () => {
  test('should load main window', async ({ page }) => {
    await page.goto('/');
    await expect(page).toHaveTitle(/ffb-config/i);
  });
  
  test('should display connection controls', async ({ page }) => {
    await page.goto('/');
    
    const connectBtn = page.locator('#connect');
    const disconnectBtn = page.locator('#disconnect');
    const portSelect = page.locator('#port');
    
    await expect(connectBtn).toBeVisible();
    await expect(disconnectBtn).toBeVisible();
    await expect(portSelect).toBeVisible();
  });
  
  test('should have tab navigation', async ({ page }) => {
    await page.goto('/');
    
    const tabs = ['settings', 'rim', 'display', 'race', 'diag', 'monitor'];
    
    for (const tab of tabs) {
      const tabButton = page.locator(`[data-tab="${tab}"]`);
      await expect(tabButton).toBeVisible();
    }
  });
  
  test('should switch tabs', async ({ page }) => {
    await page.goto('/');
    
    // Click Display tab
    await page.click('[data-tab="display"]');
    const displayPanel = page.locator('[data-panel="display"]');
    await expect(displayPanel).toBeVisible();
    
    // Click Settings tab
    await page.click('[data-tab="settings"]');
    const settingsPanel = page.locator('[data-panel="settings"]');
    await expect(settingsPanel).toBeVisible();
  });
  
  test('should show offline banner when not connected', async ({ page }) => {
    await page.goto('/');
    
    const offlineBanner = page.locator('[data-offline-banner]');
    await expect(offlineBanner).toBeVisible();
  });
});

test.describe('Display Editor', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.click('[data-tab="display"]');
  });
  
  test('should show canvas', async ({ page }) => {
    const canvas = page.locator('#disp-canvas');
    await expect(canvas).toBeVisible();
  });
  
  test('should have widget palette', async ({ page }) => {
    const palette = page.locator('.disp-palette-groups');
    await expect(palette).toBeVisible();
    
    // Check for some widget buttons
    const gearWidget = page.locator('[data-disp-type="1"]');
    const speedWidget = page.locator('[data-disp-type="2"]');
    await expect(gearWidget).toBeVisible();
    await expect(speedWidget).toBeVisible();
  });
  
  test('should have page controls', async ({ page }) => {
    const pageTabsContainer = page.locator('#disp-page-tabs');
    await expect(pageTabsContainer).toBeVisible();
  });
  
  test('should have export/import buttons', async ({ page }) => {
    const exportBtn = page.locator('#disp-export');
    const importBtn = page.locator('#disp-import');
    
    await expect(exportBtn).toBeVisible();
    await expect(importBtn).toBeVisible();
  });
});

test.describe('Settings Panel', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.click('[data-tab="settings"]');
  });
  
  test('should show firmware version fields', async ({ page }) => {
    const baseFw = page.locator('#base_fw');
    const rimFw = page.locator('#rim_fw');
    
    await expect(baseFw).toBeVisible();
    await expect(rimFw).toBeVisible();
  });
  
  test('should show FFB settings', async ({ page }) => {
    const dutyCap = page.locator('#duty_cap');
    const springK = page.locator('#spring_k');
    const hidRange = page.locator('#hid_range');
    
    await expect(dutyCap).toBeVisible();
    await expect(springK).toBeVisible();
    await expect(hidRange).toBeVisible();
  });
  
  test('should have action buttons', async ({ page }) => {
    const applyBtn = page.locator('#apply');
    const saveBtn = page.locator('#save');
    const reloadBtn = page.locator('#reload');
    const defaultsBtn = page.locator('#defaults');
    
    await expect(applyBtn).toBeVisible();
    await expect(saveBtn).toBeVisible();
    await expect(reloadBtn).toBeVisible();
    await expect(defaultsBtn).toBeVisible();
  });
});

test.describe('Monitor Panel', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.click('[data-tab="monitor"]');
  });
  
  test('should show log area', async ({ page }) => {
    const logArea = page.locator('#log');
    await expect(logArea).toBeVisible();
  });
  
  test('should have log controls', async ({ page }) => {
    const clearBtn = page.locator('#clear-log');
    const pauseBtn = page.locator('#pause-log');
    const autoscroll = page.locator('#autoscroll');
    
    await expect(clearBtn).toBeVisible();
    await expect(pauseBtn).toBeVisible();
    await expect(autoscroll).toBeVisible();
  });
  
  test('should have raw command input', async ({ page }) => {
    const rawInput = page.locator('#raw-input');
    const rawSend = page.locator('#raw-send');
    
    await expect(rawInput).toBeVisible();
    await expect(rawSend).toBeVisible();
  });
});

test.describe('Diagnostics Panel', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.click('[data-tab="diag"]');
  });
  
  test('should show status chips', async ({ page }) => {
    const chips = page.locator('#chips');
    await expect(chips).toBeVisible();
  });
  
  test('should show axle position display', async ({ page }) => {
    const axleCanvas = page.locator('#axle-canvas');
    const axleLabel = page.locator('#axle-label');
    
    await expect(axleCanvas).toBeVisible();
    await expect(axleLabel).toBeVisible();
  });
  
  test('should show pedal displays', async ({ page }) => {
    const throttle = page.locator('[data-axis="t"]');
    const brake = page.locator('[data-axis="b"]');
    const clutch = page.locator('[data-axis="c"]');
    
    await expect(throttle).toBeVisible();
    await expect(brake).toBeVisible();
    await expect(clutch).toBeVisible();
  });
});
