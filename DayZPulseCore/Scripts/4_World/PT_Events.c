// ============================================================================
//  PlayerTracker - журнал событий игрока.
//
//  Мод не может писать в SQLite, поэтому события пишутся построчно в
//  $profile:DayZPulseCore/events.jsonl (по одному JSON-объекту в строке).
//  Панель читает файл (раз в секунду), разбирает строки и складывает их
//  в локальную базу данных SQLite.
//
//  Формат строки:
//    {"t":"<тип>","at":<unix>,"id":"...","name":"...", ...поля...}
//
//  Записи только добавляются (FileMode.APPEND) — файл не перезаписывается,
//  поэтому его можно читать на ходу.
// ============================================================================

// Типы событий (строки для JSON).
//   session_in / session_out          вход и выход игрока
//   kill / death / suicide / env_death  киллфид
//   hit                               попадание по игроку
//   item_pickup / item_drop           поднял / выложил предмет
//   item_into_hands / item_out_hands  взял в руки / убрал
//   item_attach / item_detach         надел / снял экипировку
//   item_cargo_in / item_cargo_out    положил / достал из контейнера
//   build / dismantle                 построил / разобрал объект
//   chat                              сообщение в чате

class PT_Events
{
	protected static ref PT_Events s_Instance;

	// Очередь строк: пишем пачкой раз в тик, а не на каждое событие.
	// Частые операции (подбор предметов) иначе дёргали бы диск постоянно.
	protected ref array<string> m_Queue;

	protected int m_Dropped;

	static PT_Events Get()
	{
		if (!s_Instance)
			s_Instance = new PT_Events();

		return s_Instance;
	}

	void PT_Events()
	{
		m_Queue = new array<string>();
	}

	// ------------------------------------------------------------------ запись ---

	// Добавляет событие в очередь. Поля формирует вызывающий код.
	void Add(string type, string payload)
	{
		if (!PT_Config.ENABLE_EVENTS)
			return;

		if (!m_Queue)
			m_Queue = new array<string>();

		if (m_Queue.Count() >= PT_Config.EVENTS_MAX_QUEUE)
		{
			// Переполнение: теряем самые старые, но не растём бесконечно.
			m_Queue.Remove(0);
			m_Dropped++;
		}

		m_Queue.Insert(payload);
	}

	// Проверяет, пишем ли события предметов и нужно ли писать этот предмет.
	bool ShouldLogItem(string className)
	{
		if (!PT_Config.ENABLE_EVENTS)
			return false;

		string mode = PT_Config.EVENTS_ITEM_MODE;

		if (mode == "none")
			return false;

		if (mode == "all")
			return true;

		// "significant": только значимые предметы — оружие, патроны, медицина,
		// инструменты, рюкзаки, разгрузки, шлемы. Каждый яблок из стоек не пишем,
		// иначе журнал пухнет на сотни тысяч строк.
		return PT_EventsIsSignificant(className);
	}

	// Сбрасывает накопленные строки в файл одним куском.
	void Flush()
	{
		if (!m_Queue || m_Queue.Count() == 0)
			return;

		string path = PT_Config.EVENT_LOG;

		FileHandle fh = OpenFile(path, FileMode.APPEND);

		if (!fh)
		{
			// Каталог мог не создаться — попробуем ещё раз.
			MakeDirectory(PT_Config.FILE_DIR);
			fh = OpenFile(path, FileMode.APPEND);

			if (!fh)
				return;
		}

		for (int i = 0; i < m_Queue.Count(); i++)
		{
			FPrintln(fh, m_Queue[i]);
		}

		CloseFile(fh);
		m_Queue.Clear();
	}

	// Сколько событий потеряно из-за переполнения очереди (для диагностики).
	int Dropped()
	{
		return m_Dropped;
	}

	// ------------------------------------------------------------- конструктор ---

	// Собирает JSON-строку события. Поля добавляются вручную, чтобы не тащить
	// сериализатор в горячий код.
	//
	// Время — миллисекунды с запуска движка (GetTime): абсолютного времени мод
	// не знает. Панель при чтении строки сама проставляет точный момент
	// (событие попадает в файл в пределах пары секунд от действия).
	string Line(string type, string id, string name)
	{
		string s = "{";
		s += "\"t\":\"" + PT_Util.Escape(type) + "\"";
		s += ",\"ms\":" + GetGame().GetTime();
		s += ",\"id\":\"" + PT_Util.Escape(id) + "\"";
		s += ",\"name\":\"" + PT_Util.Escape(name) + "\"";
		return s;
	}

	// Точка входа из MissionServer: событие входа игрока.
	void OnPlayerConnected(PlayerIdentity identity, PlayerBase player)
	{
		if (!PT_Config.ENABLE_EVENTS || !identity)
			return;

		string s = Line("session_in", identity.GetId(), identity.GetName());
		s += ",\"steamId\":\"" + PT_Util.Escape(identity.GetPlainId()) + "\"";
		s += "}";

		Add("session_in", s);
	}

	// Событие выхода игрока.
	void OnPlayerDisconnected(PlayerIdentity identity, PlayerBase player)
	{
		if (!PT_Config.ENABLE_EVENTS || !identity)
			return;

		string s = Line("session_out", identity.GetId(), identity.GetName());
		s += ",\"steamId\":\"" + PT_Util.Escape(identity.GetPlainId()) + "\"";
		s += "}";

		Add("session_out", s);
	}

	// Смерть игрока (киллфид). killer может быть null (окружение / суицид).
	void OnPlayerKilled(PlayerBase victim, Object killer)
	{
		if (!PT_Config.ENABLE_EVENTS || !victim)
			return;

		PlayerIdentity vid = victim.GetIdentity();

		string victimId = "";
		string victimName = victim.GetCachedName();

		if (vid)
		{
			victimId = vid.GetId();
			if (victimName == "")
				victimName = vid.GetName();
		}

		PlayerBase killerPlayer = PlayerBase.Cast(killer);

		if (killerPlayer && killerPlayer == victim)
		{
			// Покончил с собой.
			string s = Line("suicide", victimId, victimName);
			s += "}";
			Add("suicide", s);
			return;
		}

		if (!killerPlayer)
		{
			// Убит окружением (зомби, падение, голод) или самим собой.
			string envName = "";

			if (killer)
				envName = killer.GetType();

			string s = Line("env_death", victimId, victimName);
			s += ",\"source\":\"" + PT_Util.Escape(envName) + "\"";
			s += "}";
			Add("env_death", s);
			return;
		}

		PlayerIdentity kid = killerPlayer.GetIdentity();
		string killerId = "";
		string killerName = killerPlayer.GetCachedName();

		if (kid)
		{
			killerId = kid.GetId();
			if (killerName == "")
				killerName = kid.GetName();
		}

		// Оружие в руках убийцы и дистанция между игроками.
		string weapon = "";
		EntityAI inHands = killerPlayer.GetItemInHands();

		if (inHands)
			weapon = inHands.GetType();

		vector a = victim.GetPosition();
		vector b = killerPlayer.GetPosition();
		float distance = vector.Distance(a, b);

		string s = Line("kill", victimId, victimName);
		s += ",\"killerId\":\"" + PT_Util.Escape(killerId) + "\"";
		s += ",\"killerName\":\"" + PT_Util.Escape(killerName) + "\"";
		s += ",\"weapon\":\"" + PT_Util.Escape(weapon) + "\"";
		s += ",\"distance\":" + PT_Util.Num(distance, 1);
		s += "}";

		Add("kill", s);
	}

	// Попадание по игроку (для киллфида и античита).
	void OnPlayerHit(PlayerBase victim, EntityAI source, string ammo, string zone)
	{
		if (!PT_Config.ENABLE_EVENTS || !victim)
			return;

		PlayerIdentity vid = victim.GetIdentity();

		if (!vid)
			return;

		string attackerId = "";
		string attackerName = "";

		PlayerBase attacker = PlayerBase.Cast(source);

		if (attacker)
		{
			PlayerIdentity aid = attacker.GetIdentity();

			if (aid)
			{
				attackerId = aid.GetId();
				attackerName = aid.GetName();
			}
		}
		else if (source)
		{
			attackerName = source.GetType();
		}

		string s = Line("hit", vid.GetId(), vid.GetName());
		s += ",\"attackerId\":\"" + PT_Util.Escape(attackerId) + "\"";
		s += ",\"attackerName\":\"" + PT_Util.Escape(attackerName) + "\"";
		s += ",\"ammo\":\"" + PT_Util.Escape(ammo) + "\"";
		s += ",\"zone\":\"" + PT_Util.Escape(zone) + "\"";
		s += "}";

		Add("hit", s);
	}

	// ------------------------------------------------------------- предметы ---

	// Кто сейчас владеет предметом (игрок-корень иерархии) или "".
	static PlayerBase OwnerOf(EntityAI item)
	{
		if (!item)
			return null;

		return PlayerBase.Cast(item.GetHierarchyRootPlayer());
	}

	// Записывает событие предмета: тип, класс предмета, слот и владелец.
	void ItemEvent(string type, EntityAI item, PlayerBase owner, string slot)
	{
		if (!PT_Config.ENABLE_EVENTS || !item)
			return;

		string className = item.GetType();

		// Фильтр по значимости: подбор яблока не пишем в режиме significant.
		if (!ShouldLogItem(className))
			return;

		string id = "";
		string name = "";

		if (owner)
		{
			PlayerIdentity ident = owner.GetIdentity();

			if (ident)
			{
				id = ident.GetId();
				name = ident.GetName();
			}
		}

		string s = Line(type, id, name);
		s += ",\"class\":\"" + PT_Util.Escape(className) + "\"";

		if (slot != "")
			s += ",\"slot\":\"" + PT_Util.Escape(slot) + "\"";

		// Позиция предмета — для карты событий.
		vector pos = item.GetPosition();
		s += ",\"x\":" + PT_Util.Num(pos[0], 1);
		s += ",\"z\":" + PT_Util.Num(pos[2], 1);
		s += "}";

		Add(type, s);
	}

	// ------------------------------------------------------------ постройки ---

	// Событие постройки или разбора. Источник — объект и игрок-владелец.
	void BuildEvent(string type, EntityAI target, PlayerBase owner, string playerId, string playerName)
	{
		if (!PT_Config.ENABLE_EVENTS || !target)
			return;

		string s = Line(type, playerId, playerName);
		s += ",\"class\":\"" + PT_Util.Escape(target.GetType()) + "\"";

		vector pos = target.GetPosition();
		s += ",\"x\":" + PT_Util.Num(pos[0], 1);
		s += ",\"z\":" + PT_Util.Num(pos[2], 1);
		s += "}";

		Add(type, s);
	}

	// Событие части постройки (стена, ступени, ворота): сборка, разбор, снос.
	void BuildPartEvent(string type, EntityAI target, Man player, string part)
	{
		if (!PT_Config.ENABLE_EVENTS || !target || !player)
			return;

		PlayerBase owner = PlayerBase.Cast(player);
		string id = "";
		string name = "";

		if (owner)
		{
			PlayerIdentity ident = owner.GetIdentity();

			if (ident)
			{
				id = ident.GetId();
				name = ident.GetName();
			}
		}

		string s = Line(type, id, name);
		s += ",\"class\":\"" + PT_Util.Escape(target.GetType()) + "\"";
		s += ",\"part\":\"" + PT_Util.Escape(part) + "\"";

		vector pos = target.GetPosition();
		s += ",\"x\":" + PT_Util.Num(pos[0], 1);
		s += ",\"z\":" + PT_Util.Num(pos[2], 1);
		s += "}";

		Add(type, s);
	}

	// ---------------------------------------------------------------- чат ---

	void Chat(PlayerIdentity identity, string text)
	{
		if (!PT_Config.ENABLE_EVENTS || !identity)
			return;

		string s = Line("chat", identity.GetId(), identity.GetName());
		s += ",\"text\":\"" + PT_Util.Escape(text) + "\"";
		s += "}";

		Add("chat", s);
	}

	// ------------------------------------------------------------- фильтр ---

	// Значимые предметы: оружие, патроны, медицина, инструменты, экипировка.
	// Сравниваем по classname (нижний регистр) — быстро, без конфигов.
	protected bool PT_EventsIsSignificant(string cls)
	{
		string c = cls;
		c.ToLower();

		// Важно: выражение начинается на той же строке, что и return.
		// Enforce Script считает перевод строки концом оператора, поэтому
		// "return" отдельной строкой превращался в "return;" и ломал парсер.
		return c.Contains("weapon") || c.Contains("rifle") || c.Contains("pistol") ||
			c.Contains("shotgun") || c.Contains("sniper") || c.Contains("ammo") ||
			c.Contains("magazine") || c.Contains("grenade") || c.Contains("explosive") ||
			c.Contains("knife") || c.Contains("axe") || c.Contains("hatchet") ||
			c.Contains("crowbar") || c.Contains("shovel") || c.Contains("sledge") ||
			c.Contains("bandage") || c.Contains("saline") || c.Contains("morphine") ||
			c.Contains("antibiotic") || c.Contains("charcoal") || c.Contains("painkiller") ||
			c.Contains("epinephrine") || c.Contains("bloodbag") || c.Contains("syringe") ||
			c.Contains("backpack") || c.Contains("vest") || c.Contains("platecarrier") ||
			c.Contains("helmet") || c.Contains("jacket") || c.Contains("pants") ||
			c.Contains("boots") || c.Contains("gloves") || c.Contains("mask") ||
			c.Contains("gorka") || c.Contains("tactical") || c.Contains("nvg") ||
			c.Contains("binocular") || c.Contains("compass") || c.Contains("gps") ||
			c.Contains("radio") || c.Contains("tent") || c.Contains("barrel") ||
			c.Contains("crate") || c.Contains("seachest") || c.Contains("codelock") ||
			c.Contains("battery") || c.Contains("generator") || c.Contains("kit");
	}
}
