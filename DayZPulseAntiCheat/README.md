# AntiCheat — серверная валидация для DayZ

Серверный аддон: ставится **только на сервер**, клиентам ничего не нужно.
Проверяет то, что сервер и так получает от клиента — позиции игроков и события
урона — и реагирует по настраиваемым порогам: пишет в лог, кикает или банит.

> **Это не замена BattlEye.** Серверный мод не видит память клиента и не может
> надёжно поймать ESP, аимбот, wallhack или подмену моделей. Он ловит игровые
> аномалии: телепорты, ускорение, полёт, урон без оружия, попадания с
> невозможной дистанции, слишком частые попадания/убийства. Используйте вместе
> с BattlEye, а не вместо него.

---

## Что проверяется

| Тип | Что означает | Как ловится |
|---|---|---|
| `teleport` | прыжок на большое расстояние за один тик | позиция / время |
| `speed` | горизонтальная скорость выше `MAX_HORIZONTAL_SPEED` несколько тиков подряд | позиция / время |
| `fly` | движение вверх быстрее `MAX_UP_SPEED` | позиция / время |
| `underground` | игрок ниже поверхности земли глубже `MAX_UNDERGROUND_DEPTH` | `SurfaceY` |
| `hit_distance` | попадание с дистанции больше лимита для оружия/ближнего боя | `EEHitBy` |
| `no_weapon` | попадание пулей, когда в руках не оружие | `EEHitBy` |
| `dead_attacker` | урон от мёртвого или без сознания стрелка | `EEHitBy` |
| `wallbang` | попадание через стену (опционально, raycast) | `EEHitBy` |
| `hit_rate` | слишком много попаданий за окно времени | `EEHitBy` |
| `kill_distance` | убийство с невозможной дистанции | `EEKilled` |
| `kill_rate` | слишком много убийств за окно времени | `EEKilled` |

Игроки в транспорте, в воде, на лестнице, в прыжке/падении, без сознания и в
наручниках не проверяются на движение — это снижает ложные срабатывания.

---

## Как это работает

```
                    сервер DayZ
┌─────────────────────────────────────────────────────┐
│  MissionServer.OnInit                               │
│    └─ AC_Guard.Start()  → таймер проверок (1 с)     │
│    └─ AC_Http.Start()   → таймер отправки (10 с)    │
│                                                     │
│  раз в секунду:                                     │
│    позиции всех игроков → speed / fly / teleport    │
│                                                     │
│  PlayerBase.EEHitBy  → дистанция / оружие / частота │
│  PlayerBase.EEKilled → дистанция / частота          │
│                                                     │
│  нарушение → очки → log / kick / ban                │
└─────────────────────────────────────────────────────┘
```

Нарушения одного типа пишутся не чаще, чем раз в `VIOLATION_COOLDOWN_MS`, но
очки за них начисляются. Очки копятся в окне `POINT_WINDOW_MS` (по умолчанию
5 минут), затем счётчик сбрасывается.

---

## Структура

```
DayZPulseAntiCheat/
  config.cpp
  Scripts/3_Game/AC_Config.c        ← настройки (правится только он)
  Scripts/3_Game/AC_Util.c          ← строки, числа, векторы, время
  Scripts/3_Game/AC_Log.c           ← нарушение, файловый лог, HTTP-очередь
  Scripts/4_World/AC_PlayerState.c  ← состояние игрока
  Scripts/4_World/AC_Guard.c        ← ядро: проверки, очки, кик/бан
  Scripts/4_World/AC_PlayerBase.c   ← хуки EEHitBy / EEKilled
  Scripts/5_Mission/AC_Http.c       ← отправка нарушений в API
  Scripts/5_Mission/AC_MissionServer.c
```

---

## Сборка

Аддон `DayZPulseAntiCheat` собирается вместе с `DayZPulseCore` в один мод
`@DayZPulse` скриптом из корня проекта:

```powershell
powershell -ExecutionPolicy Bypass -File obfuscate-mods.ps1
```

Вручную (DayZ Tools → **Addon Builder**):

1. *Addon source directory*: `...\mod\DayZPulseAntiCheat`
2. *Output directory*: `...\@DayZPulse\Addons`
3. **Prefix оставить пустым**, имя PBO должно получиться `DayZPulseAntiCheat.pbo`
   (совпадает с `dir` в `config.cpp` и путями `files[]`).
4. Pack.

```
@DayZPulse/
  Addons/
    DayZPulseCore.pbo
    DayZPulseAntiCheat.pbo
```

Запуск сервера:

```
DayZServer_x64.exe -config=serverDZ.cfg -servermod=@DayZPulse ...
```

Клиентам мод не нужен и в их `-mod=` его добавлять не надо.

---

## Настройка

Всё в `Scripts/3_Game/AC_Config.c`. После правки — пересобрать PBO и
перезапустить сервер.

### Общее

| Параметр | Смысл |
|---|---|
| `ENABLED` | главный выключатель |
| `ENFORCEMENT` | `0` — только лог; `1` — кик при `KICK_POINTS`; `2` — кик + бан при `BAN_POINTS` |
| `KICK_POINTS` / `BAN_POINTS` | пороги очков |
| `POINT_WINDOW_MS` | окно накопления очков |
| `VIOLATION_COOLDOWN_MS` | не чаще одного лога на тип нарушения |
| `STATE_TTL_MS` | сколько помнить очки игрока после выхода (15 мин) |
| `STARTUP_GRACE_MS` | пауза после входа |
| `RESPAWN_GRACE_MS` | пауза после респавна |

Состояние игрока **не удаляется сразу при выходе**: оно живёт `STATE_TTL_MS`
и вычищается фоновым тиком. Так читер не сбрасывает очки переподключением, а
при `ENFORCEMENT = 2` накопленные очки успевают дойти до бана.

**Начинайте с `ENFORCEMENT = 0`.** Посмотрите `violations.log` на своём сервере
несколько дней, подкрутите пороги под моды/карту, и только потом включайте
кик. Ошибка в пороге = кик легальных игроков.

### Движение

| Параметр | По умолчанию | Смысл |
|---|---|---|
| `CHECK_MOVEMENT` | `true` | проверять позиции |
| `CHECK_INTERVAL_MS` | `1000` | период проверки |
| `MAX_HORIZONTAL_SPEED` | `12.0` | м/с пешком |
| `MAX_UP_SPEED` | `10.0` | м/с вверх |
| `TELEPORT_DISTANCE` | `40.0` | м за тик |
| `TELEPORT_SPEED` | `30.0` | м/с для телепорта |
| `SPEED_STRIKES` | `3` | тиков сверхскорости подряд |
| `CHECK_UNDERGROUND` | `true` | проверять игроков под землёй |
| `MAX_UNDERGROUND_DEPTH` | `30.0` | глубина, м |
| `REVERT_TELEPORT` | `false` | вернуть на последнюю безопасную позицию |

`REVERT_TELEPORT` включайте только если на сервере нет административных
телепортов или все, кого телепортируют, в `whitelist.txt`.

### Бой

| Параметр | По умолчанию | Смысл |
|---|---|---|
| `CHECK_COMBAT` | `true` | проверять урон |
| `MAX_MELEE_HIT_DISTANCE` | `5.0` | м для ближнего боя |
| `MAX_FIREARM_HIT_DISTANCE` | `1500.0` | м для пуль |
| `MAX_KILL_DISTANCE` | `1500.0` | м для убийства |
| `HIT_WINDOW_MS` / `MAX_HITS_PER_WINDOW` | `10000` / `250` | частота попаданий |
| `KILL_WINDOW_MS` / `MAX_KILLS_PER_WINDOW` | `10000` / `10` | частота убийств |
| `CHECK_WEAPON_IN_HANDS` | `true` | пуля только с оружием в руках |
| `CHECK_DEAD_ATTACKER` | `true` | урон от мёртвого/без сознания |
| `CHECK_LINE_OF_SIGHT` | `false` | raycast через стену |

`CHECK_LINE_OF_SIGHT` может ложно срабатывать на кустах, заборах и высокой
траве. Включайте только после теста на своём сервере.

### Очки

| Нарушение | Очки |
|---|---|
| `speed` | 10 |
| `fly` | 15 |
| `teleport` | 10 |
| `underground` | 20 |
| `hit_distance` | 20 |
| `dead_attacker` | 25 |
| `no_weapon` | 15 |
| `wallbang` | 20 |
| `hit_rate` | 15 |
| `kill_distance` | 15 |
| `kill_rate` | 10 |

При `KICK_POINTS = 60` и `VIOLATION_COOLDOWN_MS = 5000` игрок со скоростью
2x кикается примерно через 30 секунд. Разовая аномалия от лага не даёт кик.

---

## Файлы

Все пути — от `$profile:` (обычно `DayZServer/profiles`).

| Файл | Что это |
|---|---|
| `DayZPulseAntiCheat/violations.log` | все нарушения, по строке |
| `DayZPulseAntiCheat/admin.log` | старт/стоп, кики, баны, команды |
| `DayZPulseAntiCheat/bans.txt` | список ID для бана, по одному в строке |
| `DayZPulseAntiCheat/whitelist.txt` | ID, которых не проверять |
| `DayZPulseAntiCheat/commands.txt` | очередь команд администратора |

`bans.txt` и `whitelist.txt` перечитываются раз в `RELOAD_LISTS_MS` (60 с),
поэтому их можно править на работающем сервере. Строки с `#` игнорируются.

По умолчанию бан пишется по `GetPlainId()` (SteamID64 на PC). Если нужен
хешированный `GetId()` — `BAN_BY_PLAIN_ID = false`.

---

## Команды администратора

Запишите команду в `$profile:DayZPulseAntiCheat/commands.txt` (одна строка — одна
команда). Мод прочитает файл в течение `COMMAND_INTERVAL_MS` (5 с) и очистит
его.

```
status
reload
kick 76561198000000001 AFK
ban 76561198000000001 cheat
unban 76561198000000001
exempt 76561198000000001 300
help
```

| Команда | Что делает |
|---|---|
| `status` | состояние: игроки, нарушения, кики, баны |
| `reload` | перечитать `bans.txt` и `whitelist.txt` |
| `kick <id> [reason]` | кикнуть игрока |
| `ban <id> [reason]` | добавить в `bans.txt` и кикнуть |
| `unban <id>` | убрать из `bans.txt` |
| `exempt <id> <seconds>` | временно не проверять игрока |

`id` — SteamID64 или хешированный ID из лога. Команды выполняются на сервере,
клиенту ничего не отправляется.

---

## Отправка нарушений в API

В `AC_Config.c`:

```c
static const bool   ENABLE_HTTP = true;
static const string HTTP_URL    = "http://127.0.0.1:8080/ac/ingest";
static const string API_TOKEN   = "SECRET";
static const string SERVER_ID   = "my-dayz-server";
```

Мод раз в `HTTP_INTERVAL_MS` отправляет батч:

```json
{
  "server": "my-dayz-server",
  "violations": [
    {
      "seq": 12,
      "ts": 123456789,
      "type": "teleport",
      "id": "a1b2c3...",
      "plainId": "76561198000000001",
      "name": "Alpha",
      "pos": { "x": 7500.1, "y": 30.2, "z": 7412.7 },
      "points": 10,
      "totalPoints": 40,
      "action": "log",
      "details": "dist=120.5m speed=120.5m/s ..."
    }
  ]
}
```

В `api/server.js` уже есть:

* `POST /ac/ingest` — приём (токен в `X-Api-Token`);
* `GET /api/violations?token=...` — просмотр (`?type=`, `?id=`, `?action=`,
  `?limit=`).

`RestContext` синхронный: запрос блокирует игровой поток до таймаута. Держите
API на localhost и не ставьте интервал меньше 5 секунд. Если API недоступен,
нарушения копятся в памяти (до `HTTP_QUEUE_LIMIT`) и уйдут позже.

---

## Интеграция с PlayerTracker

Если оба мода собраны вместе, административный телепорт из PlayerTracker можно
исключить из проверок. В `PT_CommandRunner.TeleportCmd` после перемещения
раскомментируйте:

```c
AC_Guard.Get().Exempt(player, 15000);
```

Либо вызывайте `AC_Guard.Get().Exempt(player, ms)` / `ExemptId(id, ms)` из
любого серверного мода. Без AntiCheat эти строки компилироваться не будут,
поэтому они закомментированы.

---

## Ложные срабатывания и тюнинг

Серверный античит видит только результат, а не намерения. Вот что стоит
проверить, если в логе появились легальные игроки:

* **`speed` на машинах/лодках** — транспорт исключён, но модовый транспорт
  может телепортировать пассажира. Поднимите `MAX_HORIZONTAL_SPEED`.
* **`teleport` после админ-телепорта** — добавьте администраторов в
  `whitelist.txt` или используйте `Exempt`.
* **`underground` в подземных базах/бункерах** — увеличьте
  `MAX_UNDERGROUND_DEPTH` или выключите `CHECK_UNDERGROUND`.
* **`no_weapon` на турелях/модовом оружии** — выключите
  `CHECK_WEAPON_IN_HANDS` или добавьте исключения в код.
* **`hit_rate` на модовом оружии с высоким RPM** — увеличьте
  `MAX_HITS_PER_WINDOW`.
* **`wallbang` на кустах/заборах** — оставьте `CHECK_LINE_OF_SIGHT = false`.

Рекомендуемый порядок:

1. `ENFORCEMENT = 0`, `LOG_CONNECTS = true`.
2. Неделя наблюдения, разбор логов.
3. Подкрутка порогов и whitelist.
4. `ENFORCEMENT = 1`.
5. Только после этого, при необходимости, `ENFORCEMENT = 2`.

---

## Ограничения

* **Клиентские читы** (ESP, аимбот, wallhack, fly в клиентской симуляции)
  серверным модом не ловятся надёжно. Нужен BattlEye/клиентская часть.
* **Ложные срабатывания** возможны на модовом транспорте, оружии, картах с
  подземельями и при сильных лагах. Начинайте с логов.
* **`GetPlainId()`** на PC — SteamID64, но на консолях это не SteamID.
  Для кросс-платформенных серверов используйте `GetId()`.
* **Кик** — это не бан. Игрок может зайти снова; для упорных используйте
  `ENFORCEMENT = 2` или `bans.txt`.
* **Права на файлы.** Мод пишет в `$profile:`; у процесса сервера должен быть
  доступ на запись.
* **Один экземпляр на сервер.** Не ставьте два PBO AntiCheat.
