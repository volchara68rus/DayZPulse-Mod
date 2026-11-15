// ============================================================================
//  PlayerTracker - реестр объектов, созданных модом.
//
//  Нужен для команды delete: сервер не выдаёт список всех объектов мира,
//  поэтому мод помнит то, что создал сам, и выдаёт каждому объекту oid.
//  oid живёт в пределах сессии сервера; реестр ограничен REGISTRY_MAX.
// ============================================================================

class PT_SpawnRegistry
{
	protected static ref PT_SpawnRegistry s_Instance;

	protected ref array<int>      m_Oids;
	protected ref array<EntityAI> m_Objects;
	protected int                 m_NextOid;

	static PT_SpawnRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new PT_SpawnRegistry();

		return s_Instance;
	}

	void PT_SpawnRegistry()
	{
		m_Oids = new array<int>();
		m_Objects = new array<EntityAI>();
		m_NextOid = 1;
	}

	int Register(EntityAI obj)
	{
		if (!obj)
			return -1;

		// Старые записи вытесняются: помним только последние REGISTRY_MAX.
		while (m_Objects.Count() >= PT_Config.REGISTRY_MAX)
		{
			m_Objects.Remove(0);
			m_Oids.Remove(0);
		}

		int oid = m_NextOid++;

		m_Oids.Insert(oid);
		m_Objects.Insert(obj);

		return oid;
	}

	EntityAI Find(int oid)
	{
		int idx = m_Oids.Find(oid);

		if (idx < 0)
			return null;

		return m_Objects[idx];
	}

	bool Delete(int oid)
	{
		int idx = m_Oids.Find(oid);

		if (idx < 0)
			return false;

		EntityAI obj = m_Objects[idx];

		m_Oids.Remove(idx);
		m_Objects.Remove(idx);

		if (obj)
			GetGame().ObjectDelete(obj);

		return true;
	}

	// Удаление записей по фильтрам. radius <= 0 - не фильтровать по позиции.
	int DeleteMatching(string type, vector pos, float radius)
	{
		int removed = 0;

		for (int i = m_Objects.Count() - 1; i >= 0; i--)
		{
			EntityAI obj = m_Objects[i];

			// Объект мог быть удалён игрой/CE - просто забываем про него.
			if (!obj)
			{
				m_Objects.Remove(i);
				m_Oids.Remove(i);
				continue;
			}

			if (type != "" && obj.GetType() != type)
				continue;

			if (radius > 0 && vector.Distance(obj.GetPosition(), pos) > radius)
				continue;

			GetGame().ObjectDelete(obj);
			m_Objects.Remove(i);
			m_Oids.Remove(i);

			removed++;
		}

		return removed;
	}

	int Count()
	{
		return m_Objects.Count();
	}

	void Clear()
	{
		m_Objects.Clear();
		m_Oids.Clear();
		m_NextOid = 1;
	}
}
