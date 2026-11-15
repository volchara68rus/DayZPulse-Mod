# DayZPulse Mod

Серверный мод для DayZ Standalone. Работает вместе с панелью
[DayZPulse-Panel](https://github.com/volchara68rus/DayZPulse-Panel): отдаёт
данные об игроках и принимает команды (предметы, лечение, телепорт, погода,
время, сообщения). Клиентам мод ставить **не нужно** — он серверный.

## Состав

| Аддон | Что делает |
|---|---|
| `DayZPulseCore` | Телеметрия игроков (позиции, здоровье, статистика, инвентарь) и выполнение команд панели |
| `DayZPulseAntiCheat` | Серверная валидация: телепорты, скорость, полёт, урон с невозможной дистанции и т.п. |

Оба аддона собираются в один мод-фолдер `@DayZPulse`.

## Как это работает

Enforce Script не умеет открывать порт и слушать входящие запросы, поэтому
обмен инициирует сама игра:

- **Позиции и статистика** — мод пишет снимок в файл
  `$profile:DayZPulseCore/players.json`, панель читает его раз в секунду.
  Это не блокирует игровой поток (в отличие от синхронного `POST`).
- **Команды** — мод сам опрашивает панель (`GET /tracker/commands`), выполняет
  и подтверждает (`POST /tracker/commands/ack`). Команды выдаются ровно один
  раз (at-most-once).
- **Инвентарь** — снимок отправляется по HTTP по запросу панели.

## Установка

1. Соберите PBO (см. ниже) или скачайте готовый `@DayZPulse` с сайта панели.
2. Положите папку `@DayZPulse` рядом с сервером.
3. Добавьте мод в запуск как **серверный**:

   ```
   DayZServer_x64.exe -config=serverDZ.cfg -serverMod=@DayZPulse
   ```

4. Панель сама создаст `profiles/DayZPulseCore/config.json` с адресом и токеном.
   Пересобирать PBO для этого не нужно.

## Сборка PBO

Нужен **Addon Builder** из DayZ Tools (Steam → DayZ Tools).

1. *Addon source directory*: `DayZPulseCore` (или `DayZPulseAntiCheat`).
2. *Output directory*: папка `@DayZPulse\Addons`.
3. **Prefix оставить пустым** — имя PBO должно совпасть с `dir` в `config.cpp`
   (`DayZPulseCore.pbo` / `DayZPulseAntiCheat.pbo`).
4. Pack.

## Настройки

Файл `profiles/DayZPulseCore/config.json` (создаёт панель):

| Параметр | Смысл |
|---|---|
| `ENABLE_FILE` | писать снимок позиций в `players.json` (рекомендуется) |
| `ENABLE_HTTP` | отправлять позиции POST-ом (блокирует игровой поток) |
| `TICK_INTERVAL_MS` | период сбора позиций, мс |
| `ENABLE_COMMANDS` | опрашивать панель на команды |
| `COMMAND_INTERVAL_MS` | период опроса команд, мс |
| `INGEST_URL` / `COMMAND_URL` / `ACK_URL` | адреса панели |
| `API_TOKEN` | токен сервера (панель подставляет сама) |
| `SERVER_ID` | идентификатор сервера в панели |

У античита свой `profiles/DayZPulseAntiCheat/config.json` — пороги проверок.

## Требования

- DayZ Standalone 1.30+
- Панель [DayZPulse-Panel](https://github.com/volchara68rus/DayZPulse-Panel)

## Важно

Серверный мод не видит память клиента: ESP, аимбот и подобные читы он не
ловит — для них нужен BattlEye. AntiCheat дополняет BattlEye, а не заменяет.
