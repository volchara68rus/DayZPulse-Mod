'use strict';

/**
 * PlayerTracker API
 *
 * Два направления:
 *   1. мод → API:    POST /ingest            (позиции игроков)
 *   2. API → мод:    GET  /commands          (очередь команд, text/plain)
 *                    POST /commands/ack      (подтверждение выполнения)
 *
 * Зависимостей нет, только стандартная библиотека Node.js.
 *
 * Запуск:
 *   node server.js --token SECRET [--port 8080] [--host 127.0.0.1] [--file players.json]
 *
 * Переменные окружения: PT_TOKEN, PT_PORT, PT_HOST, PT_FILE, PT_STALE_MS, PT_CORS
 */

const http = require('http');
const fs = require('fs');
const path = require('path');

const cfg = {
  host: process.env.PT_HOST || '127.0.0.1',
  port: parseInt(process.env.PT_PORT || '8080', 10),
  token: process.env.PT_TOKEN || 'CHANGE_ME_TOKEN',
  file: process.env.PT_FILE || '',
  staleAfterMs: parseInt(process.env.PT_STALE_MS || '15000', 10),
  cors: process.env.PT_CORS || '*',
};

function usage() {
  console.log(`
PlayerTracker API

  node server.js [options]

  --host <ip>        интерфейс, по умолчанию 127.0.0.1 (наружу не торчит)
  --port <n>         порт, по умолчанию 8080
  --token <string>   токен доступа, должен совпадать с PT_Config.API_TOKEN
  --file <path>      читать снимок из файла (файловый режим мода) вместо POST
  --stale <ms>       через сколько мс игрок считается офлайн, по умолчанию 15000

Эндпоинты (мод):
  POST /ingest                 приём снимка позиций
  GET  /commands               очередь команд (text/plain), помечает их отправленными
  POST /commands/ack           результат выполнения
  POST /ac/ingest              приём нарушений AntiCheat

Эндпоинты (админ/UI):
  GET  /api/players            игроки (+ ?online=1)
  GET  /api/player/<id>        один игрок
  POST /api/commands           поставить команду (или пачку {group, commands:[...]})
  GET  /api/commands           история команд (+ ?status=pending&group=...)
  GET  /api/commands/<id>      одна команда
  DELETE /api/commands/<id>    отменить ожидающую команду
  GET  /api/violations         нарушения AntiCheat (+ ?type=&id=&action=&limit=)
  GET  /api/health             состояние
  GET  /demo                   тестовая карта

Действия: ping spawn give preset delete weather time teleport heal damage kill
`);
}

// ---------------------------------------------------------------- аргументы ---

for (let i = 2; i < process.argv.length; i++) {
  const arg = process.argv[i];
  const val = () => process.argv[++i];

  switch (arg) {
    case '--host': cfg.host = val(); break;
    case '--port': cfg.port = parseInt(val(), 10); break;
    case '--token': cfg.token = val(); break;
    case '--file': cfg.file = val(); break;
    case '--stale': cfg.staleAfterMs = parseInt(val(), 10); break;
    case '--help':
    case '-h':
      usage();
      process.exit(0);
      break;
    default:
      console.error(`Неизвестный аргумент: ${arg}`);
      usage();
      process.exit(2);
  }
}

// ------------------------------------------------------------------ state ---

const state = {
  // игроки
  players: new Map(),
  lastIngest: 0,
  lastSource: '',
  lastError: '',
  server: null,

  // команды
  commands: [],
  nextCommandId: 1,
  acks: 0,

  // нарушения AntiCheat
  violations: [],
  nextViolationId: 1,
  lastAcIngest: 0,
  acServer: null,
};

const MAX_BODY = 512 * 1024;
const MAX_HISTORY = 300;
const MAX_VIOLATIONS = 1000;      // хранить последние N нарушений
const MAX_AC_BATCH = 500;         // максимум нарушений в одном POST /ac/ingest
const PENDING_TTL_MS = 120000;    // не забрали за 2 минуты - истекла
const DISPATCHED_TTL_MS = 30000;  // забрали, но не подтвердили - истекла
const POLL_LIMIT = 25;
const MAX_ARGS = 20;
const MAX_ARG_LEN = 256;

// Разрешённые действия. Всё, чего тут нет, мод всё равно не выполнит,
// но лучше отсечь на входе.
const ALLOWED_ACTIONS = new Set([
  'ping',
  'spawn',
  'give',
  'preset',
  'delete',
  'weather',
  'time',
  'teleport',
  'heal',
  'damage',
  'kill',
]);

const REQUIRED_ARGS = {
  ping: [],
  spawn: ['class'],
  give: ['id', 'class'],
  preset: ['name'],
  delete: [],
  weather: [],
  time: [],
  teleport: ['id'],
  heal: ['id'],
  damage: ['id'],
  kill: ['id'],
};

// ------------------------------------------------------------------ игроки ---

function round2(v) {
  return Math.round(v * 100) / 100;
}

function num(value, fallback = 0) {
  const n = Number(value);
  return Number.isFinite(n) ? round2(n) : fallback;
}

function normalize(raw, now, source) {
  const pos = raw.pos || raw.position || {};

  return {
    id: String(raw.id ?? ''),
    name: String(raw.name ?? ''),
    alive: raw.alive !== false,
    health: num(raw.health),
    maxHealth: num(raw.maxHealth),
    pos: {
      x: num(pos.x ?? raw.x),
      y: num(pos.y ?? raw.y),
      z: num(pos.z ?? raw.z),
    },
    orient: raw.orient
      ? { x: num(raw.orient.x), y: num(raw.orient.y), z: num(raw.orient.z) }
      : null,
    source,
    updatedAt: now,
  };
}

/**
 * Снимок от мода - это всегда полный список игроков,
 * поэтому состояние полностью заменяется.
 */
function applySnapshot(body, source) {
  if (!body || !Array.isArray(body.players)) {
    throw new Error('payload must contain "players" array');
  }

  const now = Date.now();
  const seen = new Set();

  for (const raw of body.players) {
    if (!raw || typeof raw !== 'object') continue;

    const id = String(raw.id ?? raw.name ?? '');
    if (!id) continue;

    seen.add(id);
    state.players.set(id, normalize(raw, now, source));
  }

  for (const id of [...state.players.keys()]) {
    if (!seen.has(id)) state.players.delete(id);
  }

  state.lastIngest = now;
  state.lastSource = source;
  state.server = body.server ?? null;
  state.lastError = '';
}

function publicView(player, now) {
  const ageMs = now - player.updatedAt;

  return {
    id: player.id,
    name: player.name,
    alive: player.alive,
    health: player.health,
    maxHealth: player.maxHealth,
    pos: player.pos,
    orient: player.orient,
    ageMs,
    stale: ageMs > cfg.staleAfterMs,
    updatedAt: new Date(player.updatedAt).toISOString(),
  };
}

// ------------------------------------------------------------- античит ---

function normalizeViolation(raw, now, server) {
  const pos = raw.pos || raw.position || {};

  return {
    id: state.nextViolationId++,
    seq: Number(raw.seq) || 0,
    ts: Number(raw.ts) || now,
    receivedAt: now,
    type: String(raw.type || 'unknown').slice(0, 64),
    playerId: String(raw.id ?? ''),
    plainId: String(raw.plainId ?? ''),
    name: String(raw.name ?? '').slice(0, 64),
    pos: {
      x: num(pos.x ?? raw.x),
      y: num(pos.y ?? raw.y),
      z: num(pos.z ?? raw.z),
    },
    points: Number(raw.points) || 0,
    totalPoints: Number(raw.totalPoints) || 0,
    action: String(raw.action || 'log').slice(0, 16),
    details: String(raw.details ?? '').slice(0, 500),
    server: String(server || ''),
  };
}

function publicViolation(violation) {
  return {
    id: violation.id,
    seq: violation.seq,
    ts: new Date(violation.ts).toISOString(),
    receivedAt: new Date(violation.receivedAt).toISOString(),
    type: violation.type,
    playerId: violation.playerId,
    plainId: violation.plainId,
    name: violation.name,
    pos: violation.pos,
    points: violation.points,
    totalPoints: violation.totalPoints,
    action: violation.action,
    details: violation.details,
    server: violation.server,
  };
}

// ----------------------------------------------------------------- команды ---

function sanitizeArg(value, name) {
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) throw new Error(`${name}: not a finite number`);
    return String(value);
  }

  if (typeof value === 'boolean') return value ? '1' : '0';

  if (typeof value !== 'string') {
    throw new Error(`${name}: only string/number/boolean values are supported`);
  }

  // '|' - разделитель текстового протокола, переносы строк ломают формат.
  const cleaned = value.replace(/[|\r\n]/g, '').trim();

  if (cleaned.length > MAX_ARG_LEN) throw new Error(`${name}: value too long`);

  return cleaned;
}

function buildCommand(body) {
  if (!body || typeof body !== 'object') throw new Error('body must be an object');

  const action = String(body.action || '').toLowerCase().trim();

  if (!ALLOWED_ACTIONS.has(action)) {
    throw new Error(`unknown action: ${action || '(empty)'}`);
  }

  const input = body.args && typeof body.args === 'object' ? body.args : {};
  const keys = Object.keys(input);

  if (keys.length > MAX_ARGS) throw new Error('too many args');

  const args = {};

  for (const rawKey of keys) {
    const key = String(rawKey).trim();

    if (!/^[a-zA-Z][a-zA-Z0-9_]{0,31}$/.test(key)) {
      throw new Error(`bad arg name: ${key}`);
    }

    args[key] = sanitizeArg(input[rawKey], key);
  }

  for (const required of REQUIRED_ARGS[action] || []) {
    if (!args[required]) throw new Error(`missing required arg: ${required}`);
  }

  for (const idKey of ['oid', 'container']) {
    if (idKey in args && !/^\d+$/.test(args[idKey])) {
      throw new Error(`${idKey}: must be a positive integer (oid from spawn/preset ack)`);
    }
  }

  if (action === 'weather') {
    const fields = ['overcast', 'rain', 'fog', 'snow', 'wind', 'storm', 'freeze'];

    if (!fields.some((f) => f in args)) {
      throw new Error(`weather: provide at least one of ${fields.join('/')}`);
    }
  }

  if (action === 'time') {
    const fields = ['year', 'month', 'day', 'hour', 'minute'];

    if (!fields.some((f) => f in args)) {
      throw new Error(`time: provide at least one of ${fields.join('/')}`);
    }
  }

  if (action === 'teleport' && !('x' in args && 'z' in args)) {
    throw new Error('teleport: "x" and "z" are required');
  }

  if (action === 'preset') {
    if (!/^[a-zA-Z0-9_-]{1,64}$/.test(String(args.name))) {
      throw new Error('preset: name may contain only a-z, 0-9, "_" and "-"');
    }

    if (!('id' in args) && !('oid' in args) && !('x' in args && 'z' in args)) {
      throw new Error('preset: target required (id / oid / x+z)');
    }
  }

  if (action === 'delete') {
    const hasTarget =
      'oid' in args || 'all' in args || 'class' in args || ('x' in args && 'z' in args);

    if (!hasTarget) {
      throw new Error('delete: provide oid, all=1, class and/or x+z+radius');
    }
  }

  return {
    id: null,
    action,
    args,
    server: body.server ? String(body.server) : null,
    group: body.group ? String(body.group) : null,
    status: 'pending',
    createdAt: null,
    dispatchedAt: null,
    finishedAt: null,
    ok: null,
    message: null,
    oids: [],
  };
}

/**
 * Вставка пачки команд: сначала валидируется вся пачка (атомарно),
 * только потом она попадает в очередь. Один запрос - одна пачка.
 */
function insertCommands(bodies, group) {
  const built = bodies.map(buildCommand);
  const now = Date.now();
  const created = [];

  for (const command of built) {
    command.id = state.nextCommandId++;
    command.createdAt = now;

    if (group) command.group = group;

    state.commands.push(command);
    created.push(command);
  }

  if (state.commands.length > MAX_HISTORY) {
    state.commands.splice(0, state.commands.length - MAX_HISTORY);
  }

  return created;
}

function createCommand(body) {
  return insertCommands([body])[0];
}

function commandView(command) {
  return {
    id: command.id,
    action: command.action,
    args: command.args,
    server: command.server,
    group: command.group || null,
    status: command.status,
    ok: command.ok,
    message: command.message,
    oids: command.oids || [],
    createdAt: new Date(command.createdAt).toISOString(),
    dispatchedAt: command.dispatchedAt ? new Date(command.dispatchedAt).toISOString() : null,
    finishedAt: command.finishedAt ? new Date(command.finishedAt).toISOString() : null,
  };
}

/**
 * Забрать ожидающие команды. Помечает их как dispatched:
 * повторно они не выдаются никогда (at-most-once).
 */
function pollCommands(serverId) {
  const now = Date.now();
  const picked = [];

  for (const command of state.commands) {
    if (picked.length >= POLL_LIMIT) break;
    if (command.status !== 'pending') continue;
    if (command.server && command.server !== serverId) continue;

    command.status = 'dispatched';
    command.dispatchedAt = now;
    picked.push(command);
  }

  return picked;
}

function commandLine(command) {
  const parts = [String(command.id), command.action];

  for (const [key, value] of Object.entries(command.args)) {
    parts.push(`${key}=${value}`);
  }

  return parts.join('|');
}

function ackCommands(results) {
  if (!Array.isArray(results)) throw new Error('"results" must be an array');

  const now = Date.now();
  let applied = 0;
  let unknown = 0;

  for (const result of results) {
    const id = Number(result && result.id);
    const command = state.commands.find((c) => c.id === id);

    if (!command) {
      unknown++;
      continue;
    }

    if (command.status === 'done' || command.status === 'failed') continue;

    command.status = result.ok ? 'done' : 'failed';
    command.ok = Boolean(result.ok);
    command.message = String(result.message ?? '').slice(0, 500);
    command.oids = Array.isArray(result.oids)
      ? result.oids.map(Number).filter((n) => Number.isFinite(n))
      : [];
    command.finishedAt = now;
    applied++;
  }

  state.acks += applied;

  return { applied, unknown };
}

// Истечение команд, которые не забрали или не подтвердили.
setInterval(() => {
  const now = Date.now();

  for (const command of state.commands) {
    if (command.status === 'pending' && now - command.createdAt > PENDING_TTL_MS) {
      command.status = 'expired';
      command.message = 'not collected by the game server';
      command.finishedAt = now;
    }

    if (command.status === 'dispatched' && now - command.dispatchedAt > DISPATCHED_TTL_MS) {
      command.status = 'expired';
      command.message = 'no ack from the game server';
      command.finishedAt = now;
    }
  }
}, 5000).unref();

// -------------------------------------------------------------------- http ---

function send(res, status, obj) {
  const body = JSON.stringify(obj, null, 2);

  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
    'Access-Control-Allow-Origin': cfg.cors,
  });
  res.end(body);
}

function sendText(res, status, text) {
  res.writeHead(status, {
    'Content-Type': 'text/plain; charset=utf-8',
    'Cache-Control': 'no-store',
    'Access-Control-Allow-Origin': cfg.cors,
  });
  res.end(text);
}

// JSON с BOM (например, файл правили в блокноте) — частая причина "Unexpected token".
function parseJson(text) {
  return JSON.parse(String(text).replace(/^\uFEFF/, ''));
}

function readBody(req, cb) {
  let size = 0;
  const chunks = [];

  req.on('data', (chunk) => {
    size += chunk.length;

    if (size > MAX_BODY) {
      cb(new Error('payload too large'));
      req.destroy();
      return;
    }

    chunks.push(chunk);
  });

  req.on('end', () => cb(null, Buffer.concat(chunks).toString('utf8')));
  req.on('error', cb);
}

function authorized(req, urlObj) {
  let token = req.headers['x-api-token'] || req.headers['authorization'] || '';
  token = String(token).replace(/^Bearer\s+/i, '');

  if (!token) token = urlObj.searchParams.get('token') || '';

  return token === cfg.token;
}

/** Читает JSON-тело и отдаёт его в колбэк; ошибки уже отправлены клиенту. */
function withJsonBody(req, res, cb) {
  readBody(req, (err, text) => {
    if (err) return send(res, 413, { error: err.message });

    let body;
    try {
      body = parseJson(text);
    } catch (e) {
      return send(res, 400, { error: 'invalid json' });
    }

    try {
      cb(body);
    } catch (e) {
      send(res, 400, { error: e.message });
    }
  });
}

const server = http.createServer((req, res) => {
  const urlObj = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const p = urlObj.pathname.replace(/\/+$/, '') || '/';

  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'Access-Control-Allow-Origin': cfg.cors,
      'Access-Control-Allow-Methods': 'GET,POST,DELETE,OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type,X-Api-Token,Authorization',
    });
    res.end();
    return;
  }

  try {
    // === каналы мода (нужен токен) =========================================

    if (p === '/ingest') {
      if (req.method !== 'POST') return send(res, 405, { error: 'use POST' });
      if (!authorized(req, urlObj)) return send(res, 401, { error: 'unauthorized' });

      return withJsonBody(req, res, (body) => {
        applySnapshot(body, 'http');
        send(res, 200, { ok: true, players: state.players.size });
      });
    }

    if (p === '/ac/ingest') {
      if (req.method !== 'POST') return send(res, 405, { error: 'use POST' });
      if (!authorized(req, urlObj)) return send(res, 401, { error: 'unauthorized' });

      return withJsonBody(req, res, (body) => {
        const list = Array.isArray(body) ? body : body && body.violations;

        if (!Array.isArray(list)) {
          throw new Error('"violations" must be an array');
        }

        if (list.length > MAX_AC_BATCH) {
          throw new Error(`too many violations in one batch (max ${MAX_AC_BATCH})`);
        }

        const now = Date.now();
        const server = body && body.server ? String(body.server) : '';
        const added = list
          .filter((v) => v && typeof v === 'object')
          .map((v) => normalizeViolation(v, now, server));

        state.violations.push(...added);

        if (state.violations.length > MAX_VIOLATIONS) {
          state.violations.splice(0, state.violations.length - MAX_VIOLATIONS);
        }

        state.lastAcIngest = now;
        if (server) state.acServer = server;

        send(res, 200, { ok: true, added: added.length, total: state.violations.length });
      });
    }

    if (p === '/commands') {
      if (req.method !== 'GET') return send(res, 405, { error: 'use GET' });
      if (!authorized(req, urlObj)) return send(res, 401, { error: 'unauthorized' });

      const serverId = urlObj.searchParams.get('server') || '';
      const picked = pollCommands(serverId);

      if (picked.length === 0) return sendText(res, 204, '');

      return sendText(res, 200, picked.map(commandLine).join('\n'));
    }

    if (p === '/commands/ack') {
      if (req.method !== 'POST') return send(res, 405, { error: 'use POST' });
      if (!authorized(req, urlObj)) return send(res, 401, { error: 'unauthorized' });

      return withJsonBody(req, res, (body) => {
        const summary = ackCommands(body.results);
        send(res, 200, { ok: true, ...summary });
      });
    }

    // === демо-страница без авторизации ======================================

    if (p === '/demo') {
      return fs.readFile(path.join(__dirname, 'demo.html'), 'utf8', (err, html) => {
        if (err) return send(res, 500, { error: 'demo.html not found' });

        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(html);
      });
    }

    // === админские эндпоинты (нужен токен) ==================================

    if (!authorized(req, urlObj)) return send(res, 401, { error: 'unauthorized' });

    if (p === '/') {
      return send(res, 200, {
        name: 'PlayerTracker API',
        players: state.players.size,
        pendingCommands: state.commands.filter((c) => c.status === 'pending').length,
        endpoints: [
          'POST /ingest',
          'GET /commands',
          'POST /commands/ack',
          'POST /ac/ingest',
          'GET /api/players',
          'GET /api/player/<id>',
          'POST /api/commands',
          'GET /api/commands',
          'GET /api/violations',
          'GET /api/health',
          'GET /demo',
        ],
      });
    }

    if (p === '/api/health') {
      return send(res, 200, {
        ok: state.lastError === '',
        players: state.players.size,
        lastIngest: state.lastIngest ? new Date(state.lastIngest).toISOString() : null,
        lastSource: state.lastSource || null,
        lastError: state.lastError || null,
        server: state.server,
        commands: {
          pending: state.commands.filter((c) => c.status === 'pending').length,
          dispatched: state.commands.filter((c) => c.status === 'dispatched').length,
          done: state.commands.filter((c) => c.status === 'done').length,
          failed: state.commands.filter((c) => c.status === 'failed').length,
          expired: state.commands.filter((c) => c.status === 'expired').length,
          acked: state.acks,
        },
        anticheat: {
          reports: state.violations.length,
          lastIngest: state.lastAcIngest ? new Date(state.lastAcIngest).toISOString() : null,
          server: state.acServer,
        },
      });
    }

    if (p === '/api/players') {
      const now = Date.now();
      const onlineOnly = ['1', 'true', 'yes'].includes(
        String(urlObj.searchParams.get('online') || '').toLowerCase()
      );

      let list = [...state.players.values()].map((pl) => publicView(pl, now));

      if (onlineOnly) list = list.filter((pl) => !pl.stale);
      list.sort((a, b) => a.name.localeCompare(b.name));

      return send(res, 200, {
        count: list.length,
        total: state.players.size,
        staleAfterMs: cfg.staleAfterMs,
        lastIngest: state.lastIngest ? new Date(state.lastIngest).toISOString() : null,
        server: state.server,
        players: list,
      });
    }

    if (p === '/api/violations') {
      if (req.method !== 'GET') return send(res, 405, { error: 'use GET' });

      const type = urlObj.searchParams.get('type');
      const playerId = urlObj.searchParams.get('id') || urlObj.searchParams.get('player');
      const action = urlObj.searchParams.get('action');
      const limit = Math.min(
        parseInt(urlObj.searchParams.get('limit') || '100', 10) || 100,
        500
      );

      let list = [...state.violations].reverse();

      if (type) list = list.filter((v) => v.type === type);
      if (action) list = list.filter((v) => v.action === action);
      if (playerId) {
        list = list.filter((v) => v.playerId === playerId || v.plainId === playerId);
      }

      return send(res, 200, {
        count: list.length,
        total: state.violations.length,
        lastIngest: state.lastAcIngest ? new Date(state.lastAcIngest).toISOString() : null,
        server: state.acServer,
        violations: list.slice(0, limit).map(publicViolation),
      });
    }

    if (p.startsWith('/api/player/')) {
      const id = decodeURIComponent(p.slice('/api/player/'.length));
      const player = state.players.get(id);

      if (!player) return send(res, 404, { error: 'player not found', id });

      return send(res, 200, publicView(player, Date.now()));
    }

    if (p === '/api/commands') {
      if (req.method === 'POST') {
        return withJsonBody(req, res, (body) => {
          // Пачка: массив команд или { group, commands: [...] }
          if (Array.isArray(body) || (body && Array.isArray(body.commands))) {
            const list = Array.isArray(body) ? body : body.commands;
            const group = !Array.isArray(body) && body.group ? String(body.group) : null;

            if (list.length === 0) throw new Error('"commands" must not be empty');
            if (list.length > 50) throw new Error('too many commands in one batch (max 50)');

            const created = insertCommands(list, group);

            return send(res, 201, {
              count: created.length,
              group,
              commands: created.map(commandView),
            });
          }

          send(res, 201, commandView(createCommand(body)));
        });
      }

      if (req.method !== 'GET') return send(res, 405, { error: 'use GET or POST' });

      const status = urlObj.searchParams.get('status');
      const group = urlObj.searchParams.get('group');
      const limit = Math.min(parseInt(urlObj.searchParams.get('limit') || '50', 10) || 50, 200);

      let list = [...state.commands].reverse();
      if (status) list = list.filter((c) => c.status === status);
      if (group) list = list.filter((c) => c.group === group);

      return send(res, 200, {
        count: list.length,
        commands: list.slice(0, limit).map(commandView),
      });
    }

    if (p.startsWith('/api/commands/')) {
      const id = Number(decodeURIComponent(p.slice('/api/commands/'.length)));
      const command = state.commands.find((c) => c.id === id);

      if (!command) return send(res, 404, { error: 'command not found', id });

      if (req.method === 'GET') return send(res, 200, commandView(command));

      if (req.method === 'DELETE') {
        if (command.status !== 'pending') {
          return send(res, 409, { error: `cannot cancel command in status '${command.status}'` });
        }

        command.status = 'cancelled';
        command.message = 'cancelled by admin';
        command.finishedAt = Date.now();

        return send(res, 200, commandView(command));
      }

      return send(res, 405, { error: 'use GET or DELETE' });
    }

    return send(res, 404, { error: 'not found', path: p });
  } catch (e) {
    console.error('[api] error:', e);
    return send(res, 500, { error: 'internal error' });
  }
});

// -------------------------------------------------------------- файл-режим ---

if (cfg.file) {
  const filePath = path.resolve(cfg.file);
  let lastMtime = 0;

  console.log(`[api] file mode: ${filePath}`);

  setInterval(() => {
    fs.stat(filePath, (err, st) => {
      if (err) {
        state.lastError = `file: ${err.code}`;
        return;
      }

      if (st.mtimeMs === lastMtime) return;
      lastMtime = st.mtimeMs;

      fs.readFile(filePath, 'utf8', (err2, text) => {
        if (err2) {
          state.lastError = `file: ${err2.code}`;
          return;
        }

        try {
          applySnapshot(parseJson(text), 'file');
        } catch (e) {
          state.lastError = `file parse: ${e.message}`;
        }
      });
    });
  }, 1000);
}

// ------------------------------------------------------------------ listen ---

server.listen(cfg.port, cfg.host, () => {
  console.log(`[api] listening on http://${cfg.host}:${cfg.port}`);

  if (cfg.token === 'CHANGE_ME_TOKEN') {
    console.warn('[api] ВНИМАНИЕ: используется токен по умолчанию. Задайте --token.');
  }

  console.log(`[api] players: http://${cfg.host}:${cfg.port}/api/players?token=<token>`);
  console.log(`[api] commands: POST http://${cfg.host}:${cfg.port}/api/commands?token=<token>`);
});

process.on('SIGINT', () => {
  console.log('\n[api] shutting down');
  server.close(() => process.exit(0));
});
