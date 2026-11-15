// ============================================================================
//  PlayerTracker - перехват создания/удаления техники.
//
//  CarScript - базовый класс всего наземного транспорта DayZ (модуль 4_World).
//  Регистрируя технику при появлении в мире и снимая при удалении, панель
//  получает список транспорта без дорогого перебора объектов мира.
// ============================================================================

modded class CarScript
{
	override void EEInit()
	{
		super.EEInit();

		if (GetGame().IsServer())
			PT_VehicleRegistry.Get().Register(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			PT_VehicleRegistry.Get().Unregister(this);

		super.EEDelete(parent);
	}
}
