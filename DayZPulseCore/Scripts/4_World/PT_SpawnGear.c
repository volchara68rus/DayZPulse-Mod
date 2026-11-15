// ============================================================================
//  DayZPulseCore - выдача набора конкретным игрокам при спавне.
//
//  Панель DayZ Pulse кладёт в $profile:DayZPulseCore/spawngear.json правила:
//    {
//      "assign": [ { "id": "<SteamID64 или GetId()>",
//                    "attachmentSlotItemSets": [...], "discreteUnsortedItemSets": [...] } ],
//      "exclude": [ "<SteamID64>", ... ]
//    }
//
//  При появлении нового персонажа:
//    - если игрок в списке исключений — ничего не выдаём;
//    - если для игрока есть назначенный набор — выдаём его;
//    - иначе ничего не делаем (срабатывает обычная система spawn-gear DayZ).
//
//  Набор применяется как DayZ spawn-gear: одежда по слотам + предметы в
//  инвентаре (карго). Значения состояния (health/quantity) — доли 0..1.
// ============================================================================

class PT_SpawnGearAttrs
{
	float healthMin;
	float healthMax;
	float quantityMin;
	float quantityMax;
}

class PT_SpawnGearSlotItem
{
	string itemType;
	int    spawnWeight;
	int    quickBarSlot;
	ref PT_SpawnGearAttrs attributes;

	void PT_SpawnGearSlotItem()
	{
		spawnWeight = 1;
		quickBarSlot = -1;
		attributes = new PT_SpawnGearAttrs();
	}
}

class PT_SpawnGearSlot
{
	string slotName;
	ref array<ref PT_SpawnGearSlotItem> discreteItemSets;

	void PT_SpawnGearSlot()
	{
		discreteItemSets = new array<ref PT_SpawnGearSlotItem>();
	}
}

class PT_SpawnGearAssign
{
	string id;
	string file;
	string label;
	ref array<ref PT_SpawnGearSlot> attachmentSlotItemSets;
	ref array<ref PT_SpawnGearSlot> discreteUnsortedItemSets;

	void PT_SpawnGearAssign()
	{
		attachmentSlotItemSets = new array<ref PT_SpawnGearSlot>();
		discreteUnsortedItemSets = new array<ref PT_SpawnGearSlot>();
	}
}

class PT_SpawnGearRules
{
	ref array<ref PT_SpawnGearAssign> assign;
	ref array<string> exclude;

	void PT_SpawnGearRules()
	{
		assign = new array<ref PT_SpawnGearAssign>();
		exclude = new array<string>();
	}
}

class PT_SpawnGear
{
	protected static ref PT_SpawnGear s_Instance;

	protected ref PT_SpawnGearRules m_Rules;
	protected ref array<string> m_Given;   // кому уже выдали в этой сессии
	protected bool m_Loaded;

	static const string FILE_PATH = "$profile:DayZPulseCore/spawngear.json";

	static PT_SpawnGear Get()
	{
		if (!s_Instance)
			s_Instance = new PT_SpawnGear();

		return s_Instance;
	}

	void PT_SpawnGear()
	{
		m_Given = new array<string>();
	}

	// Перечитывает файл один раз за сессию (панель пишет его при остановленном
	// сервере, поэтому повторное чтение не требуется).
	void Ensure()
	{
		if (m_Loaded)
			return;

		m_Loaded = true;

		if (!FileExist(FILE_PATH))
		{
			m_Rules = null;
			return;
		}

		PT_SpawnGearRules rules = new PT_SpawnGearRules();
		string err;
		JsonFileLoader<PT_SpawnGearRules>.LoadFile(FILE_PATH, rules, err);

		if (err != "")
		{
			Print("[DayZPulseCore] spawngear.json: " + err);
			return;
		}

		m_Rules = rules;
	}

	// Вызывается при входе игрока в мир. Выдаём набор один раз за сессию.
	// После смерти (OnClientRespawnEvent) запись сбрасывается — набор выдаётся снова.
	bool ApplyFor(PlayerBase player)
	{
		if (!player)
			return false;

		Ensure();

		if (!m_Rules)
			return false;

		PlayerIdentity ident = player.GetIdentity();

		if (!ident)
			return false;

		string plainId = ident.GetPlainId();
		string hashedId = ident.GetId();
		string key = hashedId;

		if (key == "")
			key = plainId;

		// Уже выдавали в этой сессии — не дублируем.
		if (m_Given.Find(key) >= 0)
			return false;

		// Исключения — не выдаём ничего.
		foreach (string ex : m_Rules.exclude)
		{
			if (ex == "")
				continue;

			if (ex == plainId || ex == hashedId)
			{
				m_Given.Insert(key);
				return false;
			}
		}

		// Ищем назначенный набор: сначала точный id игрока (пресет по игроку),
		// затем общий/по полу ("all"/"male"/"female").
		PT_SpawnGearAssign personal = null;
		PT_SpawnGearAssign common = null;
		PT_SpawnGearAssign bySex = null;

		string sex = "male";
		if (!player.IsMale())
			sex = "female";

		foreach (PT_SpawnGearAssign a : m_Rules.assign)
		{
			if (a.id == "")
				continue;

			if (a.id == plainId || a.id == hashedId)
			{
				personal = a;
				break;
			}

			if (a.id == "all")
				common = a;

			if (a.id == sex)
				bySex = a;
		}

		// Приоритет: персональный пресет → набор по полу → общий.
		PT_SpawnGearAssign chosen = personal;

		if (!chosen)
			chosen = bySex;

		if (!chosen)
			chosen = common;

		if (chosen)
		{
			ApplyAssign(player, chosen);
			m_Given.Insert(key);
			return true;
		}

		return false;
	}

	// Игрок возродился после смерти — разрешаем выдать набор снова.
	void OnRespawn(PlayerBase player)
	{
		if (!player)
			return;

		PlayerIdentity ident = player.GetIdentity();

		if (!ident)
			return;

		string key = ident.GetId();
		if (key == "")
			key = ident.GetPlainId();

		int idx = m_Given.Find(key);

		if (idx >= 0)
			m_Given.Remove(idx);
	}

	protected void ApplyAssign(PlayerBase player, PT_SpawnGearAssign a)
	{
		// Надеть одежду по слотам.
		foreach (PT_SpawnGearSlot slot : a.attachmentSlotItemSets)
		{
			if (!slot || slot.discreteItemSets.Count() == 0)
				continue;

			PT_SpawnGearSlotItem chosen = Pick(slot.discreteItemSets);

			if (!chosen || chosen.itemType == "")
				continue;

			int slotId = InventorySlots.GetSlotIdFromString(slot.slotName);
			EntityAI worn = player.GetInventory().CreateAttachmentEx(chosen.itemType, slotId);

			if (worn)
				ApplyState(worn, chosen);
		}

		// Предметы в инвентаре (карго первого уровня).
		int quickIndex = 0;

		foreach (PT_SpawnGearSlot bundle : a.discreteUnsortedItemSets)
		{
			if (!bundle)
				continue;

			foreach (PT_SpawnGearSlotItem it : bundle.discreteItemSets)
			{
				if (!it || it.itemType == "")
					continue;

				EntityAI item = EntityAI.Cast(player.GetInventory().CreateInInventory(it.itemType));

				if (item)
					ApplyState(item, it);

				// Предметы быстрого доступа кладём на панель быстрого доступа.
				if (item && it.quickBarSlot >= 0)
				{
					player.SetQuickBarEntityShortcut(item, quickIndex, true);
					quickIndex++;
				}
			}
		}
	}

	protected PT_SpawnGearSlotItem Pick(array<ref PT_SpawnGearSlotItem> list)
	{
		if (!list || list.Count() == 0)
			return null;

		int total = 0;

		foreach (PT_SpawnGearSlotItem it : list)
		{
			int w = it.spawnWeight;
			if (w < 1)
				w = 1;
			total += w;
		}

		if (total <= 0)
			return list[0];

		int roll = Math.RandomInt(0, total);

		foreach (PT_SpawnGearSlotItem it : list)
		{
			int w = it.spawnWeight;
			if (w < 1)
				w = 1;
			roll -= w;

			if (roll < 0)
				return it;
		}

		return list[list.Count() - 1];
	}

	protected void ApplyState(EntityAI item, PT_SpawnGearSlotItem spec)
	{
		if (!spec.attributes)
			return;

		float h = spec.attributes.healthMin;

		if (h <= 0 && spec.attributes.healthMax <= 0)
			h = 1;
		else if (h <= 0)
			h = spec.attributes.healthMax;

		item.SetHealth("", "Health", item.GetMaxHealth("", "Health") * Math.Clamp(h, 0, 1));

		float q = spec.attributes.quantityMin;

		if (q > 0 && q <= 1)
			item.SetQuantity(item.GetQuantityMax() * q);
	}
}
