// ============================================================================
//  PlayerTracker - реестр объектов мира (бочки, тенты, ящики, постройки).
//
//  Как и технику, объекты нельзя дёшево перебрать по миру, поэтому ведём
//  реестр: регистрируем при появлении контейнера/постройки и снимаем при
//  удалении. Панель получает список объектов для карты (бочки, тенты,
//  ящики, флаги, заборы, башни, схроны) с координатами и кратким описанием.
//
//  Регистрация идёт из modded Container_Base / BaseBuildingBase EEInit/EEDelete
//  (см. PT_WorldObjects.c).
// ============================================================================

class PT_WorldObject
{
	EntityAI entity;
	string   kind;    // barrel | tent | crate | flag | fence | watchtower | stash | build
	string   label;   // понятное имя (Бочка, Палатка, Ящик, Флаг, ...)
}

class PT_WorldObjectRegistry
{
	protected static ref PT_WorldObjectRegistry s_Instance;

	protected ref array<ref PT_WorldObject> m_Objects;

	static PT_WorldObjectRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new PT_WorldObjectRegistry();

		return s_Instance;
	}

	void PT_WorldObjectRegistry()
	{
		m_Objects = new array<ref PT_WorldObject>();
	}

	// Определяет тип объекта по имени класса.
	static string ClassifyKind(string cls)
	{
		string c = cls;

		if (c.IndexOf("Barrel") >= 0)
			return "barrel";

		if (c.IndexOf("Tent") >= 0 || c.IndexOf("Shelter") >= 0)
			return "tent";

		if (c.IndexOf("Crate") >= 0 || c.IndexOf("SeaChest") >= 0 || c.IndexOf("Case") >= 0 || c.IndexOf("FirstAidKit") >= 0 || c.IndexOf("AmmoBox") >= 0)
			return "crate";

		if (c.IndexOf("Stash") >= 0)
			return "stash";

		if (c.IndexOf("Flag") >= 0)
			return "flag";

		if (c.IndexOf("Watchtower") >= 0 || c.IndexOf("Tower") >= 0)
			return "watchtower";

		if (c.IndexOf("Fence") >= 0 || c.IndexOf("Wall") >= 0 || c.IndexOf("Gate") >= 0 || c.IndexOf("Barricade") >= 0)
			return "fence";

		return "build";
	}

	static string KindLabel(string kind)
	{
		if (kind == "barrel")
			return "Бочка";
		if (kind == "tent")
			return "Палатка";
		if (kind == "crate")
			return "Ящик";
		if (kind == "stash")
			return "Схрон";
		if (kind == "flag")
			return "Флаг";
		if (kind == "watchtower")
			return "Башня";
		if (kind == "fence")
			return "Забор";
		return "Постройка";
	}

	// Регистрирует объект, если он относится к «интересным» категориям.
	void RegisterObject(EntityAI obj)
	{
		if (!obj)
			return;

		string cls = obj.GetType();
		string kind = ClassifyKind(cls);

		// Прочие «build» (двери домов и т.п.) не плодим — только осмысленные.
		if (kind == "build")
			return;

		// Дубликаты — по сущности.
		foreach (PT_WorldObject wo : m_Objects)
		{
			if (wo.entity == obj)
				return;
		}

		PT_WorldObject rec = new PT_WorldObject();
		rec.entity = obj;
		rec.kind = kind;
		rec.label = KindLabel(kind);
		m_Objects.Insert(rec);
	}

	void Unregister(EntityAI obj)
	{
		if (!obj)
			return;

		for (int i = m_Objects.Count() - 1; i >= 0; i--)
		{
			if (m_Objects[i].entity == obj)
			{
				m_Objects.Remove(i);
				return;
			}
		}
	}

	void Prune()
	{
		for (int i = m_Objects.Count() - 1; i >= 0; i--)
		{
			if (!m_Objects[i] || !m_Objects[i].entity)
				m_Objects.Remove(i);
		}
	}

	int Count()
	{
		Prune();
		return m_Objects.Count();
	}

	// Сколько объектов каждого типа: {"barrel":N,...}
	string BuildCountsJson()
	{
		Prune();

		int barrels = 0;
		int tents = 0;
		int crates = 0;
		int flags = 0;
		int fences = 0;
		int towers = 0;
		int stashes = 0;
		int other = 0;

		foreach (PT_WorldObject wo : m_Objects)
		{
			if (wo.kind == "barrel") barrels++;
			else if (wo.kind == "tent") tents++;
			else if (wo.kind == "crate") crates++;
			else if (wo.kind == "flag") flags++;
			else if (wo.kind == "fence") fences++;
			else if (wo.kind == "watchtower") towers++;
			else if (wo.kind == "stash") stashes++;
			else other++;
		}

		string s = "{";
		s += "\"barrel\":" + barrels + ",";
		s += "\"tent\":" + tents + ",";
		s += "\"crate\":" + crates + ",";
		s += "\"flag\":" + flags + ",";
		s += "\"fence\":" + fences + ",";
		s += "\"watchtower\":" + towers + ",";
		s += "\"stash\":" + stashes + ",";
		s += "\"other\":" + other;
		s += "}";

		return s;
	}

	// JSON-массив объектов для карты (максимум maxObj записей).
	string BuildJson(int maxObj)
	{
		Prune();

		string list = "";
		int sent = 0;

		foreach (PT_WorldObject wo : m_Objects)
		{
			if (sent >= maxObj)
				break;

			EntityAI obj = wo.entity;

			if (!obj)
				continue;

			vector pos = obj.GetPosition();

			string entry = "{";
			entry += "\"kind\":\"" + wo.kind + "\",";
			entry += "\"label\":\"" + PT_Util.Escape(wo.label) + "\",";
			entry += "\"class\":\"" + PT_Util.Escape(obj.GetType()) + "\",";
			entry += "\"x\":" + PT_Util.Num(pos[0], 2) + ",";
			entry += "\"z\":" + PT_Util.Num(pos[2], 2);
			entry += "}";

			if (list != "")
				list += ",";

			list += entry;
			sent++;
		}

		return "[" + list + "]";
	}
}
