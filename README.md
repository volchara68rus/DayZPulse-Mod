# PlayerTracker — позиции игроков и удалённые команды для DayZ через HTTP API

Мод для сервера DayZ: отдаёт координаты игроков по HTTP и принимает команды
(спавн, погода, время, телепорт, здоровье) из вашего API.

## Как это работает (важно)

Enforce Script **не умеет открывать сетевой порт и слушать входящие HTTP-запросы** —
такого API в DayZ нет и модом его не добавить. Поэтому оба направления идут от игры:

```
                        ┌──────────────────────────────────────────┐
   позиции игроков      │                                          │
   POST /ingest ───────►│  Node.js API (api/server.js)             │
                        │                                          │
   GET  /commands ◄─────│  очередь команд                          │
   POST /commands/ack ─►│  результаты выполнения                   │
                        └──────────────────────────────────────────┘
                                        ▲
                                        │ GET /api/players
                                        │ POST /api/commands
                                        │
                                  сайт, карта, бот, панель
```

* мод раз в 2 секунды **сам** отправляет снимок позиций и **сам** опрашивает очередь команд;
* API хранит последний снимок и очередь команд;
* задержка команды — до `COMMAND_INTERVAL_MS` (по умолчанию 2 сек);
* команды выдаются **ровно один раз** (at-most-once): если подтверждение не дошло,
  команда помечается `expired` и не повторяется — чтобы не заспавнить предмет дважды.

## Структура проекта

```
DayZPulseCore/                         ← аддон 1 (исходники для Addon Builder)
  config.cpp                           ← CfgPatches / CfgMods (dir = "DayZPulseCore")
  Scripts/3_Game/PT_Config.c           ← НАСТРОЙКИ (правится только этот файл)
  Scripts/3_Game/PT_Util.c             ← JSON, форматирование, текстовый протокол
  Scripts/5_Mission/PT_Tracker.c       ← отправка позиций, опрос команд, ack
  Scripts/5_Mission/PT_Command.c       ← разбор ответа GET /commands
  Scripts/5_Mission/PT_CommandRunner.c ← выполнение команд на сервере
  Scripts/5_Mission/PT_SpawnRegistry.c ← реестр заспавненных объектов (oid)
  Scripts/5_Mission/PT_Preset.c        ← загрузка пресетов предметов
  Scripts/5_Mission/PT_MissionServer.c ← запуск при старте миссии

DayZPulseAntiCheat/                    ← аддон 2: серверная валидация
  config.cpp
  README.md                            ← подробная документация по античиту
  Scripts/3_Game/AC_Config.c           ← НАСТРОЙКИ античита
  Scripts/3_Game/AC_Util.c
  Scripts/3_Game/AC_Log.c
  Scripts/4_World/AC_PlayerState.c
  Scripts/4_World/AC_Guard.c           ← проверки, очки, кик/бан
  Scripts/4_World/AC_PlayerBase.c      ← EEHitBy / EEKilled
  Scripts/5_Mission/AC_Http.c
  Scripts/5_Mission/AC_MissionServer.c

api/
  server.js                            ← HTTP API (без зависимостей)
  simulate.js                          ← имитация мода: позиции + приём команд
  demo.html                            ← тестовая карта на canvas
  package.json

presets/
  akm_kit.json                         ← пример пресета (копировать в profiles)
```

В один мод-фолдер они собираются как `@DayZPulse` (см. раздел «Сборка мода»).

## 1. Быстрая проверка API (без DayZ)

```powershell
cd api
node server.js --port 8080 --token SECRET
```

Во втором терминале:

```powershell
node simulate.js --token SECRET
```

Симулятор отправляет позиции и **принимает команды** так же, как это делает мод.

```powershell
# поставить команду в очередь
curl.exe -X POST "http://127.0.0.1:8080/api/commands?token=SECRET" ^
  -H "Content-Type: application/json" ^
  -d "{\"action\":\"spawn\",\"args\":{\"class\":\"AKM\",\"x\":7500,\"y\":0,\"z\":7500}}"

# посмотреть историю
curl.exe "http://127.0.0.1:8080/api/commands?token=SECRET"
```

Откройте `http://127.0.0.1:8080/demo` и введите токен — точки будут двигаться по кругу.
Если здесь работает, в игре заработает тоже.

## 2. Сборка мода

Мод собирается одним скриптом (чистая сборка + обфускация):

```powershell
powershell -ExecutionPolicy Bypass -File obfuscate-mods.ps1
```

Результат — один мод-фолдер с двумя аддонами:

```
dist\mods\@DayZPulse\
  meta.cpp
  Addons\
    DayZPulseCore.pbo        ← PlayerTracker (позиции, команды)
    DayZPulseAntiCheat.pbo   ← AntiCheat
```

Скрипт сам собирает чистые PBO из `mod\DayZPulseCore` и `mod\DayZPulseAntiCheat`
(через `pboobf pack`), обфусцирует их и проверяет, что аддоны не конфликтуют по
именам классов.

Если нужна **чистая** (необфусцированная) сборка — добавьте `-NoObfuscate`.

Можно собрать и вручную, DayZ Tools → **Addon Builder**:

1. *Addon source directory*: `...\mod\DayZPulseCore` (или `...\mod\DayZPulseAntiCheat`)
2. *Output directory*: `...\@DayZPulse\Addons`
3. **Prefix оставить пустым**, имя PBO должно получиться `DayZPulseCore.pbo`
   (оно должно совпадать с `dir` в `config.cpp` и с путями `files[]`).
4. Pack.

Мод ставится **только на сервер**, клиентам он не нужен:

```
DayZServer_x64.exe -config=serverDZ.cfg -servermod=@DayZPulse ...
```

## 3. Настройки

Файл `DayZPulseCore/Scripts/3_Game/PT_Config.c`:

| Параметр | Смысл |
|---|---|
| `ENABLE_HTTP` | отправлять позиции POST-ом на API |
| `ENABLE_FILE` | писать снимок в файл (альтернатива HTTP) |
| `TICK_INTERVAL_MS` | период сбора позиций, мс |
| `INGEST_URL` | полный URL приёма позиций |
| `ENABLE_COMMANDS` | опрашивать API на предмет команд |
| `COMMAND_INTERVAL_MS` | период опроса команд, мс |
| `COMMAND_MAX_PER_POLL` | максимум команд за один опрос |
| `COMMAND_URL` / `ACK_URL` | URL очереди и подтверждений |
| `API_TOKEN` | токен, совпадает с `--token` у API |
| `SERVER_ID` | попадает в ответ как `server.id` |
| `INCLUDE_PLAIN_ID` | отдавать SteamID64 в поле `id` |
| `SPAWN_MAX_QTY` | предохранитель на `qty` в spawn/give/preset |
| `REGISTRY_MAX` | сколько объектов помнить для `delete` |
| `PRESET_DIR` | каталог пресетов |

После правки — пересобрать PBO и перезапустить сервер.

## 4. Эндпоинты

Со стороны мода (токен в `X-Api-Token`):

| Метод | Путь | Описание |
|---|---|---|
| POST | `/ingest` | приём снимка позиций |
| GET | `/commands` | выдать очередь команд (`text/plain`, `204` если пусто) |
| POST | `/commands/ack` | подтверждение выполнения |

Со стороны админа/UI (токен в `?token=`, `X-Api-Token` или `Authorization: Bearer`):

| Метод | Путь | Описание |
|---|---|---|
| GET | `/api/players` | все игроки; `?online=1` — только актуальные |
| GET | `/api/player/<id>` | один игрок |
| POST | `/api/commands` | поставить команду в очередь |
| GET | `/api/commands` | история (`?status=pending`, `?limit=`) |
| GET | `/api/commands/<id>` | одна команда |
| DELETE | `/api/commands/<id>` | отменить ожидающую |
| GET | `/api/health` | состояние, счётчики команд |
| GET | `/demo` | тестовая карта |

Пример ответа `GET /api/players`:

```json
{
  "count": 1,
  "lastIngest": "2026-10-28T20:36:12.815Z",
  "server": { "id": "my-dayz-server", "tick": 123456, "players": 1, "alive": 1 },
  "players": [
    {
      "id": "76561198000000001",
      "name": "Alpha",
      "alive": true,
      "health": 97.5,
      "maxHealth": 100,
      "pos": { "x": 7500.25, "y": 30.5, "z": 7412.75 },
      "orient": { "x": 0.5, "y": 0, "z": 0.5 },
      "ageMs": 58,
      "stale": false,
      "updatedAt": "2026-10-28T20:36:12.815Z"
    }
  ]
}
```

`pos.x` / `pos.z` — метры по карте (0…15360 для Чернаруси), `pos.y` — высота.
`stale: true` — данных нет дольше `staleAfterMs` (мод упал или игрок вышел).

## 5. Команды: обратный канал

Команда ставится в очередь через `POST /api/commands`:

```json
{ "action": "spawn", "args": { "class": "AKM", "x": 7500, "y": 0, "z": 7500, "qty": 2 } }
```

Ответ — `201` с объектом команды (`id`, `status: "pending"`). Дальше мод её заберёт
(в течение `COMMAND_INTERVAL_MS`), выполнит и пришлёт ack. Смотреть статус:

```powershell
curl.exe "http://127.0.0.1:8080/api/commands/3?token=SECRET"
```

### Действия

| action | Аргументы | Что делает |
|---|---|---|
| `ping` | — | проверка канала, ответ `pong` |
| `spawn` | `class`, `x`, `y`, `z` **или** `container`, `qty`, `health`, `persist`, `attachments` | создать объект(ы) в мире или в контейнере |
| `give` | `id`, `class`, `qty`, `attachments` | положить предмет прямо в инвентарь игроку |
| `preset` | `name`, цель: `id` / `oid` / `x`+`z` | выдать пресет (см. ниже) |
| `delete` | `oid` или `all=1` или `class`/`x`+`z`+`radius` | удалить объекты, созданные модом |
| `weather` | `overcast`, `rain`, `fog`, `snow`, `wind`, `storm`, `transition`, `freeze` | погода (0…1) |
| `time` | `year`, `month`, `day`, `hour`, `minute` | серверное время |
| `teleport` | `id`, `x`, `z`, `y` | переместить игрока |
| `heal` | `id`, `value`, `percent` | вылечить (по умолчанию до максимума) |
| `damage` | `id`, `value`, `percent` | нанести урон (по умолчанию до нуля) |
| `kill` | `id` | убить игрока |

`id` — это `id` из `/api/players` (SteamID64 либо BI id).
`health` — `-1` заводское, `0…1` доля, `1…100` проценты.
`attachments` — список classname через запятую, например `Mag_AKM_30Rnd,AK_Suppressor`.
`persist` (по умолчанию `false`) — объект переживёт рестарт сервера.

### oid: как удалять заспавненное

Каждый созданный объект получает `oid` (в пределах сессии сервера). Мод возвращает
их в ack, API сохраняет в команде:

```json
{ "id": 42, "action": "spawn", "status": "done",
  "oids": [7, 8], "message": "spawned 2x AKM" }
```

Дальше: `{"action":"delete","args":{"oid":7}}`, либо `{"class":"AKM","x":7500,"z":7500,"radius":30}`,
либо `{"all":1}`. Реестр помнит последние `REGISTRY_MAX` (2000) объектов и очищается
при старте миссии.

```powershell
# дождь и туман на 60 секунд, с закреплением погоды
curl.exe -X POST "http://127.0.0.1:8080/api/commands?token=SECRET" ^
  -H "Content-Type: application/json" ^
  -d "{\"action\":\"weather\",\"args\":{\"overcast\":1,\"rain\":0.8,\"fog\":0.3,\"transition\":60,\"freeze\":1}}"

# полдень
curl.exe -X POST "http://127.0.0.1:8080/api/commands?token=SECRET" ^
  -H "Content-Type: application/json" ^
  -d "{\"action\":\"time\",\"args\":{\"hour\":12,\"minute\":0}}"

# телепорт игрока и лечение
curl.exe -X POST "http://127.0.0.1:8080/api/commands?token=SECRET" ^
  -H "Content-Type: application/json" ^
  -d "{\"action\":\"teleport\",\"args\":{\"id\":\"76561198000000001\",\"x\":7500,\"z\":7500}}"
```

### Статусы команды

`pending` (в очереди) → `dispatched` (забрал мод) → `done` / `failed`,
либо `expired` (не забрали за 2 мин / нет ack 30 сек), `cancelled` (админ отменил).

### Погода: нюанс с Central Economy

Погоду в DayZ постоянно пересчитывает погодный контроллер CE. `freeze=1` вызывает
`SetWeatherUpdateFreeze(true)` — прогноз перестаёт меняться, значения держатся.
Чтобы вернуть автоматическую погоду, отправьте `{"action":"weather","args":{"freeze":0}}`
(или `freeze:false`). `transition` — длительность перехода в секундах.

### Пресеты предметов

Пресеты — JSON-файлы в `DayZServer/profiles/DayZPulseCore/presets/`, по одному на пресет.
Файл читается при каждой команде, перезапуск сервера после правки не нужен.
Пример лежит в `presets/akm_kit.json` — скопируйте его в профили.

```json
{
  "name": "akm_kit",
  "items": [
    {
      "className": "AKM",
      "quantity": 1,
      "health": -1,
      "attachments": [ "Mag_AKM_30Rnd", "AK_Suppressor" ],
      "cargo": []
    },
    {
      "className": "PlateCarrierVest",
      "quantity": 1,
      "health": -1,
      "attachments": [],
      "cargo": [
        { "className": "BandageDressing", "quantity": 2 },
        { "className": "Mag_AKM_30Rnd", "quantity": 2 }
      ]
    }
  ]
}
```

* `quantity` — `-1` или `0` = один предмет, иначе количество (ограничено `SPAWN_MAX_QTY`);
* `health` — `-1` заводское, `0…1` доля, `1…100` проценты;
* `attachments` — classname в слоты предмета (оптика, магазин, приклад — так задаются и патроны);
* `cargo` — вложенные предметы, рекурсия до 3 уровней.

Выдача — в руки/инвентарь игроку, в контейнер или на землю:

```powershell
# игроку в инвентарь
curl.exe -X POST "http://127.0.0.1:8080/api/commands?token=SECRET" ^
  -H "Content-Type: application/json" ^
  -d "{\"action\":\"preset\",\"args\":{\"name\":\"akm_kit\",\"id\":\"76561198000000001\"}}"

# в контейнер, созданный ранее (oid из ack)
curl.exe -X POST "http://127.0.0.1:8080/api/commands?token=SECRET" ^
  -H "Content-Type: application/json" ^
  -d "{\"action\":\"preset\",\"args\":{\"name\":\"akm_kit\",\"oid\":15}}"
```

### Пачка команд одним запросом

`POST /api/commands` принимает массив или объект `{ group, commands: [...] }`:

```json
{
  "group": "event-1",
  "commands": [
    { "action": "weather", "args": { "overcast": 1, "rain": 0.9, "transition": 30 } },
    { "action": "spawn", "args": { "class": "Land_HelipadCircle_F", "x": 7500, "z": 7500 } },
    { "action": "spawn", "args": { "class": "AKM", "x": 7501, "y": 0, "z": 7500, "qty": 3 } },
    { "action": "preset", "args": { "name": "akm_kit", "x": 7502, "z": 7500 } }
  ]
}
```

Ответ — `201` со списком созданных команд (каждая со своим `id`). Особенности:

* пачка валидируется **целиком**: если хоть одна команда неверна, в очередь не попадёт
  ни одна (атомарно), в ответе — `400` с текстом ошибки;
* максимум 50 команд в пачке, до 25 забирается за один опрос (порядок сохраняется);
* `group` пишется в каждую команду и позволяет потом найти их:
  `GET /api/commands?group=event-1`;
* ack приходит по каждой команде отдельно — пачка не «всё или ничего» на исполнении.

### Формат протокола (если пишете свой сервер)

Ответ `GET /commands` — простой текст, одна команда на строку:

```
<id>|<action>|<key>=<value>|<key>=<value>...
```

```
42|spawn|class=AKM|x=7500.25|y=0|z=7412.75|qty=1
43|weather|overcast=0.9|rain=0.7|transition=30
```

Пустые строки и строки с `#` игнорируются. Значения не должны содержать `|` и
переводы строк (API вычищает их сам). JSON разбирать на стороне игры не нужно —
именно поэтому протокол плоский.

### Минимальный контракт для вашего API

Если у вас уже есть свой бэкенд, мод ждёт от него ровно три вещи.
Токен во всех запросах — заголовок `X-Api-Token`.

**1. Приём позиций.** `POST /ingest`, тело `application/json`:

```json
{
  "server": { "id": "my-dayz-server", "tick": 123456, "players": 1, "alive": 1 },
  "players": [
    {
      "id": "76561198000000001",
      "name": "Alpha",
      "alive": true,
      "health": 97.5,
      "maxHealth": 100,
      "pos": { "x": 7500.25, "y": 30.5, "z": 7412.75 },
      "orient": { "x": 0.5, "y": 0, "z": 0.5 }
    }
  ]
}
```

Список всегда полный: игроков, которых в нём нет, нужно считать вышедшими.
Ответ — любой `2xx`.

**2. Выдача команд.** `GET /commands` → `200 text/plain` (или `204`, если пусто):

```
42|spawn|class=AKM|x=7500.25|y=0|z=7412.75|qty=1
43|weather|overcast=0.9|rain=0.7|transition=30
```

Выданную команду нужно сразу помечать как «отправленную» и **больше никогда не
выдавать** (at-most-once), даже если ack не пришёл.

**3. Подтверждение.** `POST /commands/ack`, тело `application/json`:

```json
{
  "server": "my-dayz-server",
  "results": [
    { "id": 42, "ok": true, "message": "spawned 1x AKM", "oids": [7] },
    { "id": 43, "ok": false, "message": "weather: controller is not available" }
  ]
}
```

`oids` необязателен: мод присылает его для `spawn` / `give` / `preset`, чтобы можно было
потом удалить объект командой `delete`. Ответ — любой `2xx`. Всё остальное (очередь,
статусы, отмена, TTL, батчи) — на вашей стороне.

## 6. Файловый режим (без HTTP-отправки позиций)

Если не хочется, чтобы игровой поток ждал HTTP:

1. В `PT_Config.c`: `ENABLE_HTTP = false`, `ENABLE_FILE = true`.
2. Мод пишет `DayZServer/profiles/DayZPulseCore/players.json`.
3. `node api/server.js --token SECRET --file "D:\DayZServer\profiles\DayZPulseCore\players.json"`

**Команды в этом режиме не работают** — они требуют HTTP-канала (`ENABLE_COMMANDS`
и доступный `COMMAND_URL`). Можно включить оба режима одновременно: позиции из
файла, команды по HTTP.

## 7. AntiCheat — серверная валидация

Отдельный аддон `DayZPulseAntiCheat/` (собирается в `DayZPulseAntiCheat.pbo`).
Ставится **только на сервер**, клиентам не нужен. Подробности — в
[`README.md`](DayZPulseAntiCheat/README.md) этого аддона.

Что проверяет:

* телепорты, ускорение, полёт, нахождение под землёй;
* попадания с невозможной дистанции, урон без оружия, урон от мёртвого
  или без сознания стрелка;
* слишком частые попадания/убийства, опционально — попадания через стену.

Нарушения копят очки: при `KICK_POINTS` — кик, при `BAN_POINTS` — бан
(`ENFORCEMENT = 2`). По умолчанию `ENFORCEMENT = 1` (кик), но начинать
рекомендуется с `0` (только лог).

```
DayZPulseAntiCheat/Scripts/3_Game/AC_Config.c   ← все пороги и пути
$profile:DayZPulseAntiCheat/violations.log      ← нарушения
$profile:DayZPulseAntiCheat/bans.txt            ← бан-лист
$profile:DayZPulseAntiCheat/whitelist.txt       ← исключения (админы)
$profile:DayZPulseAntiCheat/commands.txt        ← команды админа
```

Команды администратора (пишутся в `commands.txt`, файл очищается сам):

```
status
reload
kick 76561198000000001 AFK
ban 76561198000000001 cheat
unban 76561198000000001
exempt 76561198000000001 300
```

Если включить `ENABLE_HTTP` в `AC_Config.c`, нарушения уходят в API:

* `POST /ac/ingest` — приём (токен в `X-Api-Token`);
* `GET /api/violations?token=...` — просмотр (`?type=`, `?id=`, `?action=`,
  `?limit=`).

**Ограничение:** серверный мод не видит память клиента. ESP, аимбот, wallhack
и подобные читы он не ловит — для них нужен BattlEye и/или клиентская часть.
AntiCheat дополняет BattlEye, а не заменяет его.

## Важные предостережения

* **`RestContext` синхронный.** Запрос блокирует игровой поток до таймаута. Держите
  API на той же машине (localhost) и не ставьте интервалы меньше ~1 секунды.
* **`POST /api/commands` — это удалённое управление сервером.** Токен обязателен,
  наружу — только через HTTPS reverse-proxy с ограничением по IP. Утечка токена =
  возможность спавнить предметы и телепортировать игроков.
* **At-most-once.** Команда не повторяется. Если нужен повтор — поставьте её заново;
  статус `expired` в истории подскажет, что до игры она не дошла.
* **Один экземпляр API — один игровой сервер.** Снимок заменяет состояние, очередь
  общая. Для нескольких серверов — несколько экземпляров (разные порты/токены).
* **Приватность.** SteamID64 и ники — персональные данные; при необходимости
  `INCLUDE_PLAIN_ID = false`.
* **Тяжёлые спавны** лучше дробить на несколько команд: всё выполняется в игровом
  потоке, пачка из сотен объектов даст лаг.

## Куда расширять

* добавить в `BuildPayload` группу/клан, кровь (`GetHealth("", "Blood")`), температуру;
* новые действия в `PT_CommandRunner` (например, `lock` сервера, `kick` через RCon);
* удаление произвольных объектов мира через `GetObjectsAtPosition` (сейчас `delete`
  работает только по реестру мода — это безопаснее);
* история перемещений в БД, WebSocket/SSE вместо polling для карты;
* очередь команд с приоритетами и окном выполнения (например, «погода только по
  расписанию»).
