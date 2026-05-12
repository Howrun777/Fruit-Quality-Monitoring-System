const fs = require('fs');
const http = require('http');
const path = require('path');
const { spawn } = require('child_process');
const { chromium } = require('playwright');

const QA_URL = 'http://127.0.0.1:9100/admin/dashboard.html';
const EDGE_PATH = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';

function canReach(url) {
  return new Promise(resolve => {
    const req = http.get(url, res => {
      res.resume();
      resolve(res.statusCode >= 200 && res.statusCode < 500);
    });
    req.on('error', () => resolve(false));
    req.setTimeout(800, () => {
      req.destroy();
      resolve(false);
    });
  });
}

async function waitForServer(url, timeoutMs = 6000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (await canReach(url)) return true;
    await new Promise(resolve => setTimeout(resolve, 250));
  }
  return false;
}

async function ensureMockServer() {
  if (await canReach(QA_URL)) return null;

  const child = spawn(process.execPath, ['scripts/mock_dashboard_server.js'], {
    cwd: path.resolve(__dirname, '..'),
    env: { ...process.env, PORT: '9100' },
    stdio: ['ignore', 'pipe', 'pipe']
  });

  let stderr = '';
  child.stderr.on('data', chunk => {
    stderr += chunk.toString();
  });

  if (!(await waitForServer(QA_URL))) {
    child.kill();
    throw new Error(`Mock dashboard server did not start.\n${stderr}`);
  }

  return child;
}

async function main() {
  const mockServer = await ensureMockServer();
  const outDir = path.resolve('render_plan/dashboard_qa');
  fs.mkdirSync(outDir, { recursive: true });

  let browser;
  try {
    browser = await chromium.launch({
      headless: true,
      executablePath: EDGE_PATH
    });
  } catch (error) {
    if (mockServer) mockServer.kill();
    throw error;
  }

  const results = [];
  const viewports = [
    { name: 'desktop', width: 1440, height: 1000 },
    { name: 'mobile', width: 390, height: 844 }
  ];

  try {
    for (const viewport of viewports) {
      const page = await browser.newPage({
        viewport: { width: viewport.width, height: viewport.height },
        deviceScaleFactor: 1
      });
      const messages = [];
      page.on('console', msg => {
        const text = msg.text();
        if (!/favicon|Failed to load resource/i.test(text)) {
          messages.push(`${msg.type()}: ${text}`);
        }
      });
      page.on('pageerror', err => messages.push(`pageerror: ${err.message}`));

      await page.goto(QA_URL, {
        waitUntil: 'networkidle',
        timeout: 30000
      });
      await page.waitForTimeout(1500);
      await page.screenshot({
        path: path.join(outDir, `${viewport.name}.png`),
        fullPage: true
      });

      const metrics = await page.evaluate(() => {
        const ids = [
          'fruitHistoryChart',
          'tempChart',
          'humChart',
          'lightChart',
          'spoilChart',
          'fruit-grid-container',
          'vision-gallery-container'
        ];
        const dims = Object.fromEntries(ids.map(id => {
          const el = document.getElementById(id);
          const rect = el ? el.getBoundingClientRect() : { width: 0, height: 0 };
          return [id, { width: Math.round(rect.width), height: Math.round(rect.height) }];
        }));
        return {
          title: document.title,
          scrollWidth: document.documentElement.scrollWidth,
          clientWidth: document.documentElement.clientWidth,
          bodyTextSample: document.body.innerText.slice(0, 300),
          dims
        };
      });

      results.push({ viewport: viewport.name, messages, metrics });
      await page.close();
    }

    console.log(JSON.stringify(results, null, 2));
  } finally {
    await browser.close();
    if (mockServer) mockServer.kill();
  }
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
