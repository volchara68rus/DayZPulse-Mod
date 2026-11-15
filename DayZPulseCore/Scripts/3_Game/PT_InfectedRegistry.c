// ============================================================================
//  PlayerTracker - счётчик заражённых (AI).
//
//  Как и технику, заражённых нельзя «перебрать» по миру дёшево, поэтому
//  ведём реестр: регистрируем в modded ZombieBase.EEInit и снимаем при
//  удалении. Панель получает количество AI без дорогого сканирования мира.
// ============================================================================

class PT_InfectedRegistry
{
	protected static ref PT_InfectedRegistry s_Instance;

	protected ref array<EntityAI> m_Infected;

	static PT_InfectedRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new PT_InfectedRegistry();

		return s_Instance;
	}

	void PT_InfectedRegistry()
	{
		m_Infected = new array<EntityAI>();
	}

	void Register(EntityAI zed)
	{
		if (!zed)
			return;

		if (m_Infected.Find(zed) >= 0)
			return;

		m_Infected.Insert(zed);
	}

	void Unregister(EntityAI zed)
	{
		int idx = m_Infected.Find(zed);

		if (idx >= 0)
			m_Infected.Remove(idx);
	}

	void Prune()
	{
		for (int i = m_Infected.Count() - 1; i >= 0; i--)
		{
			if (!m_Infected[i])
				m_Infected.Remove(i);
		}
	}

	int Count()
	{
		Prune();
		return m_Infected.Count();
	}
}
