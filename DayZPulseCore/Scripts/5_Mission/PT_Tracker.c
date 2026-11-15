// ============================================================================
//  PlayerTracker - транспорт.
//
//  Работает только на выделенном сервере (IsDedicatedServer).
//  Enforce не умеет слушать порт, поэтому оба направления идут от игры:
//    - позиции: POST снимка на API (INGEST_URL) и/или запись в файл;
//    - команды: GET очереди (COMMAND_URL), выполнение, POST подтверждений
//      (ACK_URL). Команды не повторяются: если подтверждение не дошло,
//      API помечает команду истёкшей (at-most-once).
//
//  DayZ 1.29: SetHeader() задаёт только Content-Type, поэтому токен и
//  идентификатор сервера передаются в query (?token=...&server=...), а
//  POST_now()/GET_now() возвращают тело ответа без HTTP-кода - успех
//  определяется по содержимому ответа.
// ============================================================================

class PT_Tracker
{
	protected static ref PT_Tracker s_Instance;

	// RestApi/RestContext нельзя держать как ref/autoptr-члены: у них
	// приватный деструктор, движок выдаёт "Method '~RestApi' is private".
	// Обычные (не ref) члены работают.
	protected RestApi     m_Api;
	protected RestContext m_CtxIngest;
	protected RestContext m_CtxCommands;
	protected RestContext m_CtxAck;
	protected RestContext m_CtxInventory;

	protected bool m_Running;
	protected int  m_ConsecFail;
	protected int  m_CmdFail;
	protected int  m_Sent;
	protected int  m_Ticks;
	protected int  m_Executed;

	static PT_Tracker Get()
	{
		if (!s_Instance)
			s_Instance = new PT_Tracker();

		return s_Instance;
	}

	void Start()
	{
		if (m_Running)
			return;

		if (!GetGame().IsDedicatedServer())
		{
			Print("[DayZPulseCore] Not a dedicated server - tracker disabled.");
			return;
		}

		// Читаем config.json (или создаём с умолчаниями).
		PT_Config.Load();

		if (PT_Config.ENABLE_FILE)
			MakeDirectory(PT_Config.FILE_DIR);

		if (PT_Config.ENABLE_COMMANDS)
		{
			MakeDirectory(PT_Config.FILE_DIR);
			MakeDirectory(PT_Config.PRESET_DIR);
			PT_SpawnRegistry.Get().Clear();
		}

		if (PT_Config.ENABLE_HTTP || PT_Config.ENABLE_COMMANDS)
		{
			m_Api = CreateRestApi();

			if (!m_Api)
			{
				Print("[DayZPulseCore] ERROR: CreateRestApi() failed.");
				return;
			}

			if (PT_Config.ENABLE_HTTP)
			{
				m_CtxIngest = m_Api.GetRestContext(AuthUrl(PT_Config.INGEST_URL));

				if (!m_CtxIngest)
				{
					Print("[DayZPulseCore] ERROR: GetRestContext() failed for " + PT_Config.INGEST_URL);
					return;
				}

				ApplyHeaders(m_CtxIngest);
			}

			if (PT_Config.ENABLE_COMMANDS)
			{
				// server= нужен API, чтобы не отдавать команды чужому серверу.
				m_CtxCommands = m_Api.GetRestContext(AuthUrl(WithQuery(PT_Config.COMMAND_URL, "server", PT_Config.SERVER_ID)));
				m_CtxAck = m_Api.GetRestContext(AuthUrl(PT_Config.ACK_URL));

				if (!m_CtxCommands || !m_CtxAck)
				{
					Print("[DayZPulseCore] ERROR: GetRestContext() failed for command endpoints.");
					return;
				}

				ApplyHeaders(m_CtxCommands);
				ApplyHeaders(m_CtxAck);
			}
		}

		m_Running = true;
		m_Ticks = 0;

		if (PT_Config.ENABLE_HTTP || PT_Config.ENABLE_FILE)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, PT_Config.TICK_INTERVAL_MS, true);

		if (PT_Config.ENABLE_COMMANDS)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PollCommands, PT_Config.COMMAND_INTERVAL_MS, true);

		Print("[DayZPulseCore] Started | http=" + PT_Config.ENABLE_HTTP + " file=" + PT_Config.ENABLE_FILE + " commands=" + PT_Config.ENABLE_COMMANDS + " interval=" + PT_Config.TICK_INTERVAL_MS + "ms");
	}

	void Stop()
	{
		if (!m_Running)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Tick);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(PollCommands);

		m_Running = false;

		Print("[DayZPulseCore] Stopped | sent=" + m_Sent + " ticks=" + m_Ticks + " commands=" + m_Executed);
	}

	protected void ApplyHeaders(RestContext ctx)
	{
		// SetHeader задаёт только Content-Type. Произвольные заголовки
		// (X-Api-Token) отправить нельзя — токен идёт в query (см. AuthUrl).
		ctx.SetHeader("application/json");
	}

	// Добавляет token=... в query (?token=... или &token=...).
	protected string AuthUrl(string url)
	{
		if (PT_Config.API_TOKEN == "")
			return url;

		string sep = "?";

		if (url.IndexOf("?") >= 0)
			sep = "&";

		return url + sep + "token=" + PT_Config.API_TOKEN;
	}

	// Добавляет параметр в query (?key=value или &key=value).
	protected string WithQuery(string url, string key, string value)
	{
		if (key == "" || value == "")
			return url;

		string sep = "?";

		if (url.IndexOf("?") >= 0)
			sep = "&";

		return url + sep + key + "=" + value;
	}

	// -------------------------------------------------------------- позиции ---

	void Tick()
	{
		if (!m_Running)
			return;

		m_Ticks++;

		string payload = BuildPayload();

		if (PT_Config.ENABLE_HTTP && m_CtxIngest)
			SendPayload(payload);

		if (PT_Config.ENABLE_FILE)
			WritePayload(payload);
	}

	// --------------------------------------------------------------- команды ---

	void PollCommands()
	{
		if (!m_Running || !m_CtxCommands)
			return;

		string body = m_CtxCommands.GET_now("");

		// Пустой ответ — нет команд в очереди (API отдаёт 204), это норма.
		if (body == "")
		{
			m_CmdFail = 0;
			return;
		}

		if (body.IndexOf("\"error\"") >= 0)
		{
			m_CmdFail++;

			if (m_CmdFail == 1 || (m_CmdFail % 30) == 0)
				Print("[DayZPulseCore] command poll failed (attempts " + m_CmdFail + "): " + body);

			return;
		}

		m_CmdFail = 0;

		array<ref PT_Command> commands = PT_CommandParser.Parse(body);

		if (commands.Count() == 0)
			return;

		string results = "";
		int handled = 0;

		foreach (PT_Command cmd : commands)
		{
			if (handled >= PT_Config.COMMAND_MAX_PER_POLL)
				break;

			// Защита: если элемент массива обнулился (некорректная строка),
			// пропускаем его, чтобы не уронить весь цикл.
			if (!cmd)
				continue;

			PT_Result result = PT_CommandRunner.Execute(cmd);
			m_Executed++;

			if (results != "")
				results += ",";

			string entry = "{\"id\":" + cmd.id + ",\"ok\":" + PT_Util.BoolStr(result.ok) + ",\"message\":\"" + PT_Util.Escape(result.message) + "\"";

			if (result.oids && result.oids.Count() > 0)
			{
				entry += ",\"oids\":[";

				for (int i = 0; i < result.oids.Count(); i++)
				{
					if (i > 0)
						entry += ",";

					entry += result.oids[i].ToString();
				}

				entry += "]";
			}

			entry += "}";
			results += entry;

			string okStr = "FAIL";

			if (result.ok)
				okStr = "OK";

			Print("[DayZPulseCore] command #" + cmd.id + " '" + cmd.action + "' -> " + okStr + ": " + result.message);

			handled++;
		}

		SendAck(results);
	}

	protected void SendAck(string results)
	{
		string payload = "{\"server\":\"" + PT_Util.Escape(PT_Config.SERVER_ID) + "\",\"results\":[" + results + "]}";
		string response = m_CtxAck.POST_now("", payload);

		if (response.IndexOf("\"ok\":true") < 0 && response.IndexOf("\"ok\": true") < 0)
			Print("[DayZPulseCore] ack failed: " + response);
	}

	// ------------------------------------------------------------------ JSON ---

	string BuildPayload()
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		int aliveCount = 0;
		int pingSum = 0;
		int pingCount = 0;
		string list = "";

		foreach (Man man : players)
		{
			PlayerBase pb = PlayerBase.Cast(man);
			if (!pb)
				continue;

			PlayerIdentity ident = pb.GetIdentity();

			string id = "";
			string plainId = "";
			string name = "";
			int ping = 0;

			if (ident)
			{
				// id = GetId() (хеш DayZ) — совпадает с id= в ADM-логе, поэтому
				// панель связывает данные мода и логи в одну запись игрока.
				id = ident.GetId();

				// plainId = SteamID64 — показывается в базе игроков.
				plainId = ident.GetPlainId();

				name = ident.GetName();

				// Средний пинг игрока в миллисекундах (PlayerIdentity.GetPingAvg).
				ping = ident.GetPingAvg();
			}

			if (ping > 0)
			{
				pingSum += ping;
				pingCount++;
			}

			if (id == "")
				id = "unknown_" + players.Find(man);

			vector pos = pb.GetPosition();
			vector ori = pb.GetOrientation();

			float health = pb.GetHealth("", "Health");
			float maxHealth = pb.GetMaxHealth("", "Health");
			bool alive = pb.IsAlive();

			if (alive)
				aliveCount++;

			string entry = "{";
			entry += "\"id\":\"" + PT_Util.Escape(id) + "\",";
			entry += "\"plainId\":\"" + PT_Util.Escape(plainId) + "\",";
			entry += "\"name\":\"" + PT_Util.Escape(name) + "\",";
			entry += "\"alive\":" + PT_Util.BoolStr(alive) + ",";
			entry += "\"ping\":" + ping + ",";
			entry += "\"health\":" + PT_Util.Num(health, 1) + ",";
			entry += "\"maxHealth\":" + PT_Util.Num(maxHealth, 1) + ",";
			entry += "\"pos\":{\"x\":" + PT_Util.Num(pos[0], 2) + ",\"y\":" + PT_Util.Num(pos[1], 2) + ",\"z\":" + PT_Util.Num(pos[2], 2) + "},";
			entry += "\"orient\":{\"x\":" + PT_Util.Num(ori[0], 3) + ",\"y\":" + PT_Util.Num(ori[1], 3) + ",\"z\":" + PT_Util.Num(ori[2], 3) + "},";
			entry += "\"stats\":" + BuildStatsJson(pb) + ",";
			entry += "\"mods\":" + BuildModifiersJson(pb);
			entry += "}";

			if (list != "")
				list += ",";

			list += entry;
		}

		int avgPing = 0;

		if (pingCount > 0)
			avgPing = pingSum / pingCount;

		string payload = "{";
		payload += "\"server\":{";
		payload += "\"id\":\"" + PT_Util.Escape(PT_Config.SERVER_ID) + "\",";
		payload += "\"tick\":" + GetGame().GetTime() + ",";
		payload += "\"players\":" + players.Count() + ",";
		payload += "\"alive\":" + aliveCount + ",";

		// Производительность сервера. GetFps() возвращает МИЛЛИСЕКУНДЫ на кадр,
		// GetAvgFPS() — кадры в секунду: берём именно кадры.
		float frameMs = GetGame().GetFps();
		float fps = GetGame().GetAvgFPS();

		payload += "\"fps\":" + PT_Util.Num(fps, 1) + ",";
		payload += "\"fpsAvg\":" + PT_Util.Num(fps, 1) + ",";
		payload += "\"frameMs\":" + PT_Util.Num(frameMs, 2) + ",";
		payload += "\"pingAvg\":" + avgPing + ",";

		payload += "\"vehicles\":" + PT_VehicleRegistry.Get().Count() + ",";
		payload += "\"infected\":" + PT_InfectedRegistry.Get().Count() + ",";
		payload += "\"worldObjects\":" + PT_WorldObjectRegistry.Get().Count();
		payload += "},";
		payload += "\"players\":[" + list + "],";
		payload += "\"vehicleList\":" + PT_VehicleRegistry.Get().BuildJson(PT_Config.VEHICLE_MAX) + ",";
		payload += "\"worldObjectCounts\":" + PT_WorldObjectRegistry.Get().BuildCountsJson() + ",";
		payload += "\"worldObjectList\":" + PT_WorldObjectRegistry.Get().BuildJson(PT_Config.WORLD_OBJECT_MAX) + "";
		payload += "}";

		return payload;
	}

	// Статистика выживания. Вода/еда/теплокомфорт/сырость/стамина/токсичность —
	// это PlayerStat (Get/GetMax/GetNormalized). Здоровье, кровь и шок — зоны
	// DamageSystem (GetHealth/GetMaxHealth). Температура тела — GetTemperature.
	static string BuildStatsJson(PlayerBase pb)
	{
		if (!pb)
			return "{}";

		string s = "{";

		s += "\"water\":" + StatPair(pb.GetStatWater()) + ",";
		s += "\"energy\":" + StatPair(pb.GetStatEnergy()) + ",";
		s += "\"heatComfort\":" + StatPair(pb.GetStatHeatComfort()) + ",";
		s += "\"wet\":" + StatPair(pb.GetStatWet()) + ",";
		s += "\"stamina\":" + StatPair(pb.GetStatStamina()) + ",";
		s += "\"toxicity\":" + StatPair(pb.GetStatToxicity()) + ",";
		s += "\"diet\":" + StatPair(pb.GetStatDiet()) + ",";

		// Зоны здоровья.
		s += "\"health\":" + ZonePair(pb, "Health") + ",";
		s += "\"blood\":" + ZonePair(pb, "Blood") + ",";
		s += "\"shock\":" + ZonePair(pb, "Shock") + ",";

		// Температура тела (°C) и её границы.
		float temp = pb.GetTemperature();
		float tmin = pb.GetTemperatureMin();
		float tmax = pb.GetTemperatureMax();
		s += "\"temperature\":{\"v\":" + PT_Util.Num(temp, 1) + ",\"min\":" + PT_Util.Num(tmin, 1) + ",\"max\":" + PT_Util.Num(tmax, 1) + "},";

		// Флаги состояния.
		s += "\"unconscious\":" + PT_Util.BoolStr(pb.IsUnconscious()) + ",";
		s += "\"restrained\":" + PT_Util.BoolStr(pb.IsRestrained()) + ",";
		s += "\"bleeding\":" + PT_Util.BoolStr(pb.IsBleeding()) + ",";
		s += "\"alive\":" + PT_Util.BoolStr(pb.IsAlive());

		s += "}";

		return s;
	}

	// Пара {"v":значение,"n":нормализованное,"max":максимум} для одного стата.
	static string StatPair(PlayerStatBase stat)
	{
		if (!stat)
			return "null";

		float v = stat.Get();
		float mx = stat.GetMax();
		float n = stat.GetNormalized();

		return "{\"v\":" + PT_Util.Num(v, 1) + ",\"n\":" + PT_Util.Num(n, 3) + ",\"max\":" + PT_Util.Num(mx, 1) + "}";
	}

	// Пара для зоны здоровья DamageSystem.
	static string ZonePair(PlayerBase pb, string zone)
	{
		float v = pb.GetHealth("", zone);
		float mx = pb.GetMaxHealth("", zone);
		float n = 0;

		if (mx > 0)
			n = v / mx;

		return "{\"v\":" + PT_Util.Num(v, 1) + ",\"n\":" + PT_Util.Num(n, 3) + ",\"max\":" + PT_Util.Num(mx, 1) + "}";
	}

	// Активные модификаторы (болезни, состояния): списком строк для UI.
	// IsModifierActive() падает, если модификатор не зарегистрирован в этой
	// версии игры (внутри m_ModifierList.Get(id).IsActive() без проверки),
	// поэтому используем GetModifier(id) с явной проверкой на null.
	static string BuildModifiersJson(PlayerBase pb)
	{
		if (!pb)
			return "[]";

		ModifiersManager mm = pb.GetModifiersManager();

		if (!mm)
			return "[]";

		string list = "";

		list = ModJoin(list, ModActive(mm, eModifiers.MDF_BLEEDING), "bleeding");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_BROKEN_LEGS), "broken_legs");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_BROKEN_ARMS), "broken_arms");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_CHOLERA), "cholera");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_INFLUENZA), "influenza");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_SALMONELLA), "salmonella");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_FEVER), "fever");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_COMMON_COLD), "cold");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_WOUND_INFECTION1), "wound_infection");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_WOUND_INFECTION2), "wound_infection2");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_POISONING), "poisoning");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HEMOLYTIC_REACTION), "hemolytic");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_PNEUMONIA), "pneumonia");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HEAVYMETAL1), "heavy_metal");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HEAVYMETAL2), "heavy_metal2");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HEAVYMETAL3), "heavy_metal3");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HEAT_STROKE1), "heat_stroke");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_BURNING), "burning");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_CONTAMINATION1), "contamination");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_UNCONSCIOUSNESS), "unconscious");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HEART_ATTACK), "heart_attack");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_FATIGUE), "fatigue");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_STUFFED), "stuffed");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_BLINDED), "blinded");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_THIRST), "dehydration");
		list = ModJoin(list, ModActive(mm, eModifiers.MDF_HUNGER), "hunger");

		return "[" + list + "]";
	}

	// Активен ли модификатор (безопасно: null, если не зарегистрирован).
	static bool ModActive(ModifiersManager mm, eModifiers id)
	{
		ModifierBase m = mm.GetModifier(id);

		if (!m)
			return false;

		return m.IsActive();
	}

	// Добавляет ключ в JSON-список строк, если состояние активно.
	static string ModJoin(string list, bool active, string key)
	{
		if (!active)
			return list;

		if (list != "")
			list += ",";

		return list + "\"" + key + "\"";
	}

	// ----------------------------------------------------------------- HTTP ---

	// Отправка снимка инвентаря игрока на /tracker/inventory.
	void PostInventory(string name, string id, string itemsJson)
	{
		if (!PT_Config.ENABLE_HTTP || !PT_Config.ENABLE_INVENTORY)
			return;

		if (!m_Api)
			return;

		if (!m_CtxInventory)
		{
			m_CtxInventory = m_Api.GetRestContext(AuthUrl(PT_Config.INVENTORY_URL));

			if (!m_CtxInventory)
				return;

			ApplyHeaders(m_CtxInventory);
		}

		string payload = "{\"server\":\"" + PT_Util.Escape(PT_Config.SERVER_ID) + "\",";
		payload += "\"id\":\"" + PT_Util.Escape(id) + "\",";
		payload += "\"name\":\"" + PT_Util.Escape(name) + "\",";
		payload += "\"items\":" + itemsJson + "}";

		string response = m_CtxInventory.POST_now("", payload);

		if (response.IndexOf("\"ok\":true") < 0 && response.IndexOf("\"ok\": true") < 0)
			Print("[DayZPulseCore] inventory POST failed: " + response);
	}

	protected void SendPayload(string payload)
	{
		string response = m_CtxIngest.POST_now("", payload);

		if (response.IndexOf("\"ok\":true") >= 0 || response.IndexOf("\"ok\": true") >= 0)
		{
			m_Sent++;
			m_ConsecFail = 0;
			return;
		}

		m_ConsecFail++;

		// Не забиваем лог: первая ошибка и далее каждая 30-я.
		if (m_ConsecFail == 1 || (m_ConsecFail % 30) == 0)
			Print("[DayZPulseCore] POST failed (attempts " + m_ConsecFail + "): " + response);
	}

	// ----------------------------------------------------------------- FILE ---

	protected void WritePayload(string payload)
	{
		FileHandle fh = OpenFile(PT_Config.FILE_PATH, FileMode.WRITE);

		if (!fh)
		{
			if (m_ConsecFail == 0)
				Print("[DayZPulseCore] ERROR: cannot write " + PT_Config.FILE_PATH);

			m_ConsecFail++;
			return;
		}

		FPrint(fh, payload);
		CloseFile(fh);
		m_ConsecFail = 0;
	}
}
