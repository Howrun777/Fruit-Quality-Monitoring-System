const http = require('http');
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', 'FieldMonitoringPlatform');
const port = Number(process.env.PORT || 9100);
const now = Math.floor(Date.now() / 1000);

const fruitRows = [
  { device_id: '1001-01-01', sugar_brix: 15.8, maturity_score: 1.05, collected_at: now - 120 },
  { device_id: '1001-01-02', sugar_brix: 13.2, maturity_score: 0.88, collected_at: now - 300 },
  { device_id: '1001-01-03', sugar_brix: 16.9, maturity_score: 1.13, collected_at: now - 2400 },
  { device_id: '1001-01-04', sugar_brix: 11.6, maturity_score: 0.77, collected_at: now - 80 },
  { device_id: '1001-01-05', sugar_brix: 14.7, maturity_score: 0.98, collected_at: now - 160 }
];

const imageFiles = [
  '1001-01-01_1777814150.jpg',
  '1001-01-02_1777814210.jpg',
  '1001-01-03_1777814270.jpg',
  '1001-01-01_1777924020.jpg'
];

function json(res, body) {
  res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
  res.end(JSON.stringify(body));
}

function sampleSeries(count, mapper) {
  return Array.from({ length: count }, (_, i) => {
    const timestamp = now - (count - 1 - i) * 300;
    return mapper(timestamp, i);
  });
}

function serveStatic(req, res) {
  const url = new URL(req.url, `http://localhost:${port}`);
  let filePath = url.pathname === '/' ? '/admin/dashboard.html' : url.pathname;
  filePath = path.normalize(decodeURIComponent(filePath)).replace(/^(\.\.[/\\])+/, '');
  const absPath = path.join(root, filePath);
  if (!absPath.startsWith(root)) {
    res.writeHead(403);
    res.end('Forbidden');
    return;
  }
  fs.readFile(absPath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    const ext = path.extname(absPath).toLowerCase();
    const type = ext === '.html' ? 'text/html; charset=utf-8'
      : ext === '.css' ? 'text/css; charset=utf-8'
      : ext === '.js' ? 'application/javascript; charset=utf-8'
      : ext === '.jpg' || ext === '.jpeg' ? 'image/jpeg'
      : 'application/octet-stream';
    res.writeHead(200, { 'Content-Type': type });
    res.end(data);
  });
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://localhost:${port}`);

  if (url.pathname === '/api/admin/field/list') {
    return json(res, { code: 200, data: { list: [{ field_id: '1001' }, { field_id: '1002' }] } });
  }

  if (url.pathname === '/api/admin/fruit/list') {
    return json(res, { code: 200, data: fruitRows });
  }

  if (url.pathname === '/api/admin/field/environment') {
    return json(res, {
      code: 200,
      data: sampleSeries(36, (timestamp, i) => ({
        timestamp,
        temperature: 4 + Math.sin(i / 4) * 3 + (i > 26 ? 7 : 0),
        humidity: 63 + Math.cos(i / 5) * 8,
        light: Math.max(0, 26000 + Math.sin(i / 3) * 18000),
        spoilage: Math.max(0, 18 + i * 1.35 + Math.sin(i / 2) * 8)
      }))
    });
  }

  if (url.pathname === '/api/admin/fruit/history') {
    const deviceId = url.searchParams.get('device_id') || '1001-01-01';
    return json(res, {
      code: 200,
      data: sampleSeries(36, (timestamp, i) => ({
        timestamp,
        sugar_brix: 11.8 + i * 0.12 + Math.sin(i / 4) * 0.5,
        maturity_score: 0.76 + i * 0.006,
        quality_level: i % 7 === 0 ? -1 : 8 + (i % 3),
        image_url: `assets/uploads/${imageFiles[i % imageFiles.length]}`
      }))
    });
  }

  serveStatic(req, res);
});

server.listen(port, '127.0.0.1', () => {
  console.log(`Mock dashboard server listening on http://127.0.0.1:${port}/admin/dashboard.html`);
});
