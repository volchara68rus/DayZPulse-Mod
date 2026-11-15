// ============================================================================
//  PlayerTracker - перехват создания/удаления заражённых (AI).
//
//  ZombieBase - базовый класс заражённых в модуле 4_World. Регистрируя их при
//  появлении и снимая при удалении, панель получает счётчик AI без дорогого
//  перебора объектов мира.
//
//  ВАЖНО: модифицировать нужно именно ZombieBase (4_World), а не
//  DayZInfected (3_Game) - при `modded class DayZInfected` сервер падает на
//  загрузке модуля World (minidump).
// ============================================================================

modded class ZombieBase
{
	override void EEInit()
	{
		super.EEInit();

		if (GetGame().IsServer())
			PT_InfectedRegistry.Get().Register(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			PT_InfectedRegistry.Get().Unregister(this);

		super.EEDelete(parent);
	}
}
