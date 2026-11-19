// ============================================================================
//  PlayerTracker - настройки
//
//  Все настраиваемые значения читаются из файла JSON при старте сервера:
//      $profile:DayZPulseCore/config.json
//
//  Файл создаётся автоматически при первом запуске. Панель DayZ Pulse
//  подставляет туда токен, имя сервера и адреса автоматически.
//  Правьте пороги и перезапускайте сервер - пересобирать PBO НЕ нужно.
// ============================================================================

// Значения по умолчанию + структура config.json.
class PT_ConfigData
{
	bool   ENABLE_HTTP;
	bool   ENABLE_FILE;
	int    TICK_INTERVAL_MS;
	string INGEST_URL;

	bool   ENABLE_COMMANDS;
	int    COMMAND_INTERVAL_MS;
	int    COMMAND_MAX_PER_POLL;
	string COMMAND_URL;
	string ACK_URL;

	bool   ENABLE_INVENTORY;
	string INVENTORY_URL;

	// Журнал событий (файловый режим): вход/выход, киллы, предметы, постройки.
	bool   ENABLE_EVENTS;
	string EVENTS_ITEM_MODE;   // all | significant | none
	int    EVENTS_MAX_QUEUE;

	string API_TOKEN;
	string SERVER_ID;
	bool   INCLUDE_PLAIN_ID;
	int    SPAWN_MAX_QTY;
	int    REGISTRY_MAX;
	int    VEHICLE_MAX;
	int    WORLD_OBJECT_MAX;
	float  SPAWN_FOOD;
	float  SPAWN_WATER;
	float  SPAWN_WARMTH;

	void PT_ConfigData()
	{
		ENABLE_HTTP          = false;
		ENABLE_FILE          = true;
		TICK_INTERVAL_MS     = 2000;
		INGEST_URL           = "http://127.0.0.1:8730/tracker/ingest";

		ENABLE_COMMANDS      = true;
		COMMAND_INTERVAL_MS  = 2000;
		COMMAND_MAX_PER_POLL = 25;
		COMMAND_URL          = "http://127.0.0.1:8730/tracker/commands";
		ACK_URL              = "http://127.0.0.1:8730/tracker/commands/ack";

		ENABLE_INVENTORY     = true;
		INVENTORY_URL        = "http://127.0.0.1:8730/tracker/inventory";

		ENABLE_EVENTS        = true;
		EVENTS_ITEM_MODE     = "significant";
		EVENTS_MAX_QUEUE     = 500;

		API_TOKEN            = "";
		SERVER_ID            = "cherno";
		INCLUDE_PLAIN_ID     = true;
		SPAWN_MAX_QTY        = 25;
		REGISTRY_MAX         = 2000;
		VEHICLE_MAX          = 300;
		WORLD_OBJECT_MAX     = 3000;
		SPAWN_FOOD           = -1;
		SPAWN_WATER          = -1;
		SPAWN_WARMTH         = -1;
	}
}

class PT_Config
{
	// Значения, заполняются из config.json в Load().
	static bool   ENABLE_HTTP;
	static bool   ENABLE_FILE;
	static int    TICK_INTERVAL_MS;
	static string INGEST_URL;
	static bool   ENABLE_COMMANDS;
	static int    COMMAND_INTERVAL_MS;
	static int    COMMAND_MAX_PER_POLL;
	static string COMMAND_URL;
	static string ACK_URL;
	static bool   ENABLE_INVENTORY;
	static string INVENTORY_URL;
	static bool   ENABLE_EVENTS;
	static string EVENTS_ITEM_MODE;
	static int    EVENTS_MAX_QUEUE;
	static string API_TOKEN;
	static string SERVER_ID;
	static bool   INCLUDE_PLAIN_ID;
	static int    SPAWN_MAX_QTY;
	static int    REGISTRY_MAX;
	static int    VEHICLE_MAX;
	static int    WORLD_OBJECT_MAX;
	static float  SPAWN_FOOD;
	static float  SPAWN_WATER;
	static float  SPAWN_WARMTH;

	// Структурные пути (не выносятся в JSON).
	static const string CONFIG_DIR  = "$profile:DayZPulseCore";
	static const string CONFIG_FILE = "$profile:DayZPulseCore/config.json";
	static const string FILE_DIR    = "$profile:DayZPulseCore";
	static const string FILE_PATH   = "$profile:DayZPulseCore/players.json";
	static const string EVENT_LOG   = "$profile:DayZPulseCore/events.jsonl";
	static const string PRESET_DIR  = "$profile:DayZPulseCore/presets";

	// Читает config.json (создаёт с настройками по умолчанию, если файла нет).
	static void Load()
	{
		PT_ConfigData data = new PT_ConfigData();

		if (FileExist(CONFIG_FILE))
		{
			string errorMessage;
			JsonFileLoader<PT_ConfigData>.LoadFile(CONFIG_FILE, data, errorMessage);
			if (errorMessage != "")
				Print("[DayZPulseCore] config.json: " + errorMessage);
		}
		else
		{
			MakeDirectory(CONFIG_DIR);
			JsonFileLoader<PT_ConfigData>.JsonSaveFile(CONFIG_FILE, data);
			Print("[DayZPulseCore] Создан файл настроек: " + CONFIG_FILE);
		}

		ENABLE_HTTP          = data.ENABLE_HTTP;
		ENABLE_FILE          = data.ENABLE_FILE;
		TICK_INTERVAL_MS     = data.TICK_INTERVAL_MS;
		INGEST_URL           = data.INGEST_URL;
		ENABLE_COMMANDS      = data.ENABLE_COMMANDS;
		COMMAND_INTERVAL_MS  = data.COMMAND_INTERVAL_MS;
		COMMAND_MAX_PER_POLL = data.COMMAND_MAX_PER_POLL;
		COMMAND_URL          = data.COMMAND_URL;
		ACK_URL              = data.ACK_URL;
		ENABLE_INVENTORY     = data.ENABLE_INVENTORY;
		INVENTORY_URL        = data.INVENTORY_URL;
		ENABLE_EVENTS        = data.ENABLE_EVENTS;
		EVENTS_ITEM_MODE     = data.EVENTS_ITEM_MODE;
		EVENTS_MAX_QUEUE     = data.EVENTS_MAX_QUEUE;
		API_TOKEN            = data.API_TOKEN;
		SERVER_ID            = data.SERVER_ID;
		INCLUDE_PLAIN_ID     = data.INCLUDE_PLAIN_ID;
		SPAWN_MAX_QTY        = data.SPAWN_MAX_QTY;
		REGISTRY_MAX         = data.REGISTRY_MAX;
		VEHICLE_MAX          = data.VEHICLE_MAX;
		WORLD_OBJECT_MAX     = data.WORLD_OBJECT_MAX;
		SPAWN_FOOD           = data.SPAWN_FOOD;
		SPAWN_WATER          = data.SPAWN_WATER;
		SPAWN_WARMTH         = data.SPAWN_WARMTH;
	}
}
