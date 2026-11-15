'use strict';

/**
 * Симулятор мода: проверяет оба направления без запуска DayZ.
 *
 *   node simulate.js --token SECRET [--url http://127.0.0.1:8080/ingest]
 *
 * Раз в 2 секунды:
 *   - отправляет фальшивые позиции (POST /ingest);
 *   - опрашивает очередь команд (GET /commands) и подтверждает их
 *     (POST /commands/ack) - как это делает мод.
 *
 * Команды не выполняются по-настоящему, только логируются и подтверждаются.
 */

const http = require('http');

const args = process.argv.slice(2);
const get = (name, fallback) => {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
};

const ingestUrl = new URL(get('--url', process.env.PT_URL || 'http://127.0.0.1:8080/ingest'));
const token = get('--token', process.env.PT_TOKEN || 'CHANGE_ME_TOKEN');

const ingestPath = ingestUrl.pathname;
const commandsPath = '/commands';
const ackPath = '/commands/ack';

const players = [
  { id: '76561198000000001', name: 'Alpha', radius: 900, speed: 0.9, y: 30 },
  { id: '76561198000000002', name: 'Bravo', radius: 450, speed: -1.4, y: 25 },
  { id: '76561198000000003', name: 'Charlie', radius: 1500, speed: 0.4, y: 60 },
];

const CX = 7500; // центр Чернаруси
const CZ = 7500;

let tick = 0;

function request(method, pathname, body) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? null : body;
    const headers = { 'X-Api-Token': token };

    if (payload !== null) {
      headers['Content-Type'] = 'application/json';
      headers['Content-Length'] = Buffer.byteLength(payload);
    }

    const req = http.request(
      {
        hostname: ingestUrl.hostname,
        port: ingestUrl.port || 80,
        path: pathname,
        method,
        headers,
      },
      (res) => {
        let data = '';
        res.on('data', (c) => (data += c));
        res.on('end', () => resolve({ status: res.statusCode, body: data }));
      }
    );

    req.on('error', reject);

    if (payload !== null) req.write(payload);
    req.end();
  });
}

function buildSnapshot() {
  tick += 0.05;

  return JSON.stringify({
    server: { id: 'simulator', tick: Date.now(), players: players.length, alive: players.length },
    players: players.map((p) => {
      const a = tick * p.speed * 2;

      return {
        id: p.id,
        name: p.name,
        alive: true,
        health: 100,
        maxHealth: 100,
        pos: {
          x: +(CX + Math.cos(a) * p.radius).toFixed(2),
          y: p.y,
          z: +(CZ + Math.sin(a) * p.radius).toFixed(2),
        },
        orient: { x: Math.cos(a), y: 0, z: Math.sin(a) },
      };
    }),
  });
}

async function pushPositions() {
  try {
    const res = await request('POST', ingestPath, buildSnapshot());
    console.log(`[sim] positions: ${res.status} ${res.body.trim()}`);
  } catch (e) {
    console.error('[sim] positions error:', e.message);
  }
}

async function pollCommands() {
  try {
    const res = await request('GET', commandsPath);

    // 204 = очередь пуста
    if (res.status === 204 || !res.body.trim()) return;
    if (res.status !== 200) {
      console.error(`[sim] commands: ${res.status} ${res.body.trim()}`);
      return;
    }

    const lines = res.body.split('\n').map((l) => l.trim()).filter(Boolean);
    const results = [];

    for (const line of lines) {
      const parts = line.split('|');
      const id = Number(parts[0]);

      console.log(`[sim] command #${id}: ${parts.slice(1).join(' ')}`);

      // Как настоящий мод: spawn/give/preset возвращают oid созданных объектов.
      const spawnsObjects = ['spawn', 'give', 'preset'].includes(parts[1]);

      results.push({
        id,
        ok: true,
        message: `simulated on ${parts[1]}`,
        oids: spawnsObjects ? [9000 + id] : [],
      });
    }

    if (results.length === 0) return;

    const ack = await request('POST', ackPath, JSON.stringify({ server: 'simulator', results }));
    console.log(`[sim] ack: ${ack.status} ${ack.body.trim()}`);
  } catch (e) {
    console.error('[sim] commands error:', e.message);
  }
}

async function loop() {
  await Promise.all([pushPositions(), pollCommands()]);
}

console.log(`[sim] pushing to ${ingestUrl.href} every 2s; polling ${commandsPath}`);
loop();
setInterval(loop, 2000);
