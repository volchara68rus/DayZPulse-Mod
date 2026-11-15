// ============================================================================
//  AntiCheat - настройки
//
//  Все настраиваемые значения читаются из файла JSON при старте сервера:
//      $profile:DayZPulseAntiCheat/config.json
//
//  Файл создаётся автоматически при первом запуске. Панель DayZ Pulse
//  подставляет туда токен, имя сервера и адрес автоматически.
//  Правьте пороги и перезапускайте сервер - пересобирать PBO НЕ нужно.
//
//  ВАЖНО: это серверная валидация, а не клиентский античит. Она видит только
//  то, что клиент присылает серверу (позиции, попадания, урон). ESP, аимбот
//  и читы через память клиента таким модом не ловятся - для них нужен
//  BattlEye и/или клиентская часть.
// ============================================================================

// Значения по умолчанию + структура config.json.
class AC_ConfigData
{
	// --- Общее ---
	bool   ENABLED;
	int    ENFORCEMENT;
	int    KICK_POINTS;
	int    BAN_POINTS;
	int    POINT_WINDOW_MS;
	int    VIOLATION_COOLDOWN_MS;
	int    STATE_TTL_MS;
	int    STARTUP_GRACE_MS;
	int    RESPAWN_GRACE_MS;

	// --- Движение ---
	bool   CHECK_MOVEMENT;
	int    CHECK_INTERVAL_MS;
	float  MAX_HORIZONTAL_SPEED;
	float  MAX_UP_SPEED;
	float  TELEPORT_DISTANCE;
	float  TELEPORT_SPEED;
	int    SPEED_STRIKES;
	bool   CHECK_UNDERGROUND;
	float  MAX_UNDERGROUND_DEPTH;
	bool   REVERT_TELEPORT;

	// --- Бой ---
	bool   CHECK_COMBAT;
	float  MAX_MELEE_HIT_DISTANCE;
	float  MAX_FIREARM_HIT_DISTANCE;
	float  MAX_KILL_DISTANCE;
	int    HIT_WINDOW_MS;
	int    MAX_HITS_PER_WINDOW;
	int    KILL_WINDOW_MS;
	int    MAX_KILLS_PER_WINDOW;
	bool   CHECK_WEAPON_IN_HANDS;
	bool   CHECK_DEAD_ATTACKER;
	bool   CHECK_LINE_OF_SIGHT;

	// --- Очки за нарушения ---
	int    POINTS_SPEED;
	int    POINTS_FLY;
	int    POINTS_TELEPORT;
	int    POINTS_UNDERGROUND;
	int    POINTS_HIT_DISTANCE;
	int    POINTS_DEAD_ATTACKER;
	int    POINTS_NO_WEAPON;
	int    POINTS_WALLBANG;
	int    POINTS_HIT_RATE;
	int    POINTS_KILL_DISTANCE;
	int    POINTS_KILL_RATE;

	// --- Списки и логи ---
	int    RELOAD_LISTS_MS;
	int    COMMAND_INTERVAL_MS;
	bool   LOG_TO_FILE;
	bool   LOG_TO_CONSOLE;
	bool   LOG_CONNECTS;
	bool   BAN_BY_PLAIN_ID;

	// --- Отправка нарушений в API ---
	bool   ENABLE_HTTP;
	string HTTP_URL;
	string API_TOKEN;
	string SERVER_ID;
	int    HTTP_INTERVAL_MS;
	int    HTTP_MAX_BATCH;
	int    HTTP_QUEUE_LIMIT;

	void AC_ConfigData()
	{
		ENABLED               = true;
		ENFORCEMENT           = 1;
		KICK_POINTS           = 60;
		BAN_POINTS            = 150;
		POINT_WINDOW_MS       = 300000;
		VIOLATION_COOLDOWN_MS = 5000;
		STATE_TTL_MS          = 900000;
		STARTUP_GRACE_MS      = 20000;
		RESPAWN_GRACE_MS      = 10000;

		CHECK_MOVEMENT        = true;
		CHECK_INTERVAL_MS     = 1000;
		MAX_HORIZONTAL_SPEED  = 12.0;
		MAX_UP_SPEED          = 10.0;
		TELEPORT_DISTANCE     = 40.0;
		TELEPORT_SPEED        = 30.0;
		SPEED_STRIKES         = 3;
		CHECK_UNDERGROUND     = true;
		MAX_UNDERGROUND_DEPTH = 30.0;
		REVERT_TELEPORT       = false;

		CHECK_COMBAT             = true;
		MAX_MELEE_HIT_DISTANCE   = 5.0;
		MAX_FIREARM_HIT_DISTANCE = 1500.0;
		MAX_KILL_DISTANCE        = 1500.0;
		HIT_WINDOW_MS            = 10000;
		MAX_HITS_PER_WINDOW      = 250;
		KILL_WINDOW_MS           = 10000;
		MAX_KILLS_PER_WINDOW     = 10;
		CHECK_WEAPON_IN_HANDS    = true;
		CHECK_DEAD_ATTACKER      = true;
		CHECK_LINE_OF_SIGHT      = false;

		POINTS_SPEED         = 10;
		POINTS_FLY           = 15;
		POINTS_TELEPORT      = 10;
		POINTS_UNDERGROUND   = 20;
		POINTS_HIT_DISTANCE  = 20;
		POINTS_DEAD_ATTACKER = 25;
		POINTS_NO_WEAPON     = 15;
		POINTS_WALLBANG      = 20;
		POINTS_HIT_RATE      = 15;
		POINTS_KILL_DISTANCE = 15;
		POINTS_KILL_RATE     = 10;

		RELOAD_LISTS_MS      = 60000;
		COMMAND_INTERVAL_MS  = 5000;
		LOG_TO_FILE          = true;
		LOG_TO_CONSOLE       = true;
		LOG_CONNECTS         = false;
		BAN_BY_PLAIN_ID      = true;

		ENABLE_HTTP        = true;
		HTTP_URL           = "http://127.0.0.1:8730/tracker/ac/ingest";
		API_TOKEN          = "";
		SERVER_ID          = "cherno";
		HTTP_INTERVAL_MS   = 10000;
		HTTP_MAX_BATCH     = 50;
		HTTP_QUEUE_LIMIT   = 1000;
	}
}

class AC_Config
{
	// Значения, заполняются из config.json в Load().
	static bool   ENABLED;
	static int    ENFORCEMENT;
	static int    KICK_POINTS;
	static int    BAN_POINTS;
	static int    POINT_WINDOW_MS;
	static int    VIOLATION_COOLDOWN_MS;
	static int    STATE_TTL_MS;
	static int    STARTUP_GRACE_MS;
	static int    RESPAWN_GRACE_MS;

	static bool   CHECK_MOVEMENT;
	static int    CHECK_INTERVAL_MS;
	static float  MAX_HORIZONTAL_SPEED;
	static float  MAX_UP_SPEED;
	static float  TELEPORT_DISTANCE;
	static float  TELEPORT_SPEED;
	static int    SPEED_STRIKES;
	static bool   CHECK_UNDERGROUND;
	static float  MAX_UNDERGROUND_DEPTH;
	static bool   REVERT_TELEPORT;

	static bool   CHECK_COMBAT;
	static float  MAX_MELEE_HIT_DISTANCE;
	static float  MAX_FIREARM_HIT_DISTANCE;
	static float  MAX_KILL_DISTANCE;
	static int    HIT_WINDOW_MS;
	static int    MAX_HITS_PER_WINDOW;
	static int    KILL_WINDOW_MS;
	static int    MAX_KILLS_PER_WINDOW;
	static bool   CHECK_WEAPON_IN_HANDS;
	static bool   CHECK_DEAD_ATTACKER;
	static bool   CHECK_LINE_OF_SIGHT;

	static int    POINTS_SPEED;
	static int    POINTS_FLY;
	static int    POINTS_TELEPORT;
	static int    POINTS_UNDERGROUND;
	static int    POINTS_HIT_DISTANCE;
	static int    POINTS_DEAD_ATTACKER;
	static int    POINTS_NO_WEAPON;
	static int    POINTS_WALLBANG;
	static int    POINTS_HIT_RATE;
	static int    POINTS_KILL_DISTANCE;
	static int    POINTS_KILL_RATE;

	static int    RELOAD_LISTS_MS;
	static int    COMMAND_INTERVAL_MS;
	static bool   LOG_TO_FILE;
	static bool   LOG_TO_CONSOLE;
	static bool   LOG_CONNECTS;
	static bool   BAN_BY_PLAIN_ID;

	static bool   ENABLE_HTTP;
	static string HTTP_URL;
	static string API_TOKEN;
	static string SERVER_ID;
	static int    HTTP_INTERVAL_MS;
	static int    HTTP_MAX_BATCH;
	static int    HTTP_QUEUE_LIMIT;

	// Структурные пути (не выносятся в JSON).
	static const string DIR            = "$profile:DayZPulseAntiCheat";
	static const string CONFIG_FILE    = "$profile:DayZPulseAntiCheat/config.json";
	static const string LOG_FILE       = "$profile:DayZPulseAntiCheat/violations.log";
	static const string ADMIN_LOG      = "$profile:DayZPulseAntiCheat/admin.log";
	static const string BAN_FILE       = "$profile:DayZPulseAntiCheat/bans.txt";
	static const string WHITELIST_FILE = "$profile:DayZPulseAntiCheat/whitelist.txt";
	static const string COMMAND_FILE   = "$profile:DayZPulseAntiCheat/commands.txt";

	// Читает config.json (создаёт с настройками по умолчанию, если файла нет).
	static void Load()
	{
		AC_ConfigData data = new AC_ConfigData();

		if (FileExist(CONFIG_FILE))
		{
			string errorMessage;
			JsonFileLoader<AC_ConfigData>.LoadFile(CONFIG_FILE, data, errorMessage);
			if (errorMessage != "")
				Print("[DayZPulseAntiCheat] config.json: " + errorMessage);
		}
		else
		{
			MakeDirectory(DIR);
			JsonFileLoader<AC_ConfigData>.JsonSaveFile(CONFIG_FILE, data);
			Print("[DayZPulseAntiCheat] Создан файл настроек: " + CONFIG_FILE);
		}

		ENABLED               = data.ENABLED;
		ENFORCEMENT           = data.ENFORCEMENT;
		KICK_POINTS           = data.KICK_POINTS;
		BAN_POINTS            = data.BAN_POINTS;
		POINT_WINDOW_MS       = data.POINT_WINDOW_MS;
		VIOLATION_COOLDOWN_MS = data.VIOLATION_COOLDOWN_MS;
		STATE_TTL_MS          = data.STATE_TTL_MS;
		STARTUP_GRACE_MS      = data.STARTUP_GRACE_MS;
		RESPAWN_GRACE_MS      = data.RESPAWN_GRACE_MS;

		CHECK_MOVEMENT        = data.CHECK_MOVEMENT;
		CHECK_INTERVAL_MS     = data.CHECK_INTERVAL_MS;
		MAX_HORIZONTAL_SPEED  = data.MAX_HORIZONTAL_SPEED;
		MAX_UP_SPEED          = data.MAX_UP_SPEED;
		TELEPORT_DISTANCE     = data.TELEPORT_DISTANCE;
		TELEPORT_SPEED        = data.TELEPORT_SPEED;
		SPEED_STRIKES         = data.SPEED_STRIKES;
		CHECK_UNDERGROUND     = data.CHECK_UNDERGROUND;
		MAX_UNDERGROUND_DEPTH = data.MAX_UNDERGROUND_DEPTH;
		REVERT_TELEPORT       = data.REVERT_TELEPORT;

		CHECK_COMBAT             = data.CHECK_COMBAT;
		MAX_MELEE_HIT_DISTANCE   = data.MAX_MELEE_HIT_DISTANCE;
		MAX_FIREARM_HIT_DISTANCE = data.MAX_FIREARM_HIT_DISTANCE;
		MAX_KILL_DISTANCE        = data.MAX_KILL_DISTANCE;
		HIT_WINDOW_MS            = data.HIT_WINDOW_MS;
		MAX_HITS_PER_WINDOW      = data.MAX_HITS_PER_WINDOW;
		KILL_WINDOW_MS           = data.KILL_WINDOW_MS;
		MAX_KILLS_PER_WINDOW     = data.MAX_KILLS_PER_WINDOW;
		CHECK_WEAPON_IN_HANDS    = data.CHECK_WEAPON_IN_HANDS;
		CHECK_DEAD_ATTACKER      = data.CHECK_DEAD_ATTACKER;
		CHECK_LINE_OF_SIGHT      = data.CHECK_LINE_OF_SIGHT;

		POINTS_SPEED         = data.POINTS_SPEED;
		POINTS_FLY           = data.POINTS_FLY;
		POINTS_TELEPORT      = data.POINTS_TELEPORT;
		POINTS_UNDERGROUND   = data.POINTS_UNDERGROUND;
		POINTS_HIT_DISTANCE  = data.POINTS_HIT_DISTANCE;
		POINTS_DEAD_ATTACKER = data.POINTS_DEAD_ATTACKER;
		POINTS_NO_WEAPON     = data.POINTS_NO_WEAPON;
		POINTS_WALLBANG      = data.POINTS_WALLBANG;
		POINTS_HIT_RATE      = data.POINTS_HIT_RATE;
		POINTS_KILL_DISTANCE = data.POINTS_KILL_DISTANCE;
		POINTS_KILL_RATE     = data.POINTS_KILL_RATE;

		RELOAD_LISTS_MS      = data.RELOAD_LISTS_MS;
		COMMAND_INTERVAL_MS  = data.COMMAND_INTERVAL_MS;
		LOG_TO_FILE          = data.LOG_TO_FILE;
		LOG_TO_CONSOLE       = data.LOG_TO_CONSOLE;
		LOG_CONNECTS         = data.LOG_CONNECTS;
		BAN_BY_PLAIN_ID      = data.BAN_BY_PLAIN_ID;

		ENABLE_HTTP        = data.ENABLE_HTTP;
		HTTP_URL           = data.HTTP_URL;
		API_TOKEN          = data.API_TOKEN;
		SERVER_ID          = data.SERVER_ID;
		HTTP_INTERVAL_MS   = data.HTTP_INTERVAL_MS;
		HTTP_MAX_BATCH     = data.HTTP_MAX_BATCH;
		HTTP_QUEUE_LIMIT   = data.HTTP_QUEUE_LIMIT;
	}
}
