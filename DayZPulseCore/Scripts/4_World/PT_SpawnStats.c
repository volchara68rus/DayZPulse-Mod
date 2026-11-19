// ============================================================================
//  DayZPulseCore - стартовые показатели нового персонажа.
//
//  Панель DayZ Pulse задаёт в config.json:
//      SPAWN_FOOD, SPAWN_WATER, SPAWN_WARMTH   (доли 0..1; -1 = не задано)
//
//  Значения применяются при создании персонажа. Для ВЕРНУВШЕГОСЯ игрока движок
//  затем восстанавливает сохранённые статы из хранилища (PlayerBase.OnStoreLoad),
//  поэтому перезапись «свежим» значениям не мешает.
//
//  Используются только проверенные методы PlayerBase:
//      GetStatEnergy() / GetStatWater() / GetStatHeatComfort()  (PlayerStat: Get/GetMax/Set)
// ============================================================================

modded class PlayerBase
{
	override void EEInit()
	{
		super.EEInit();

		if (!GetGame() || !GetGame().IsServer())
			return;

		float food = PT_Config.SPAWN_FOOD;
		float water = PT_Config.SPAWN_WATER;
		float warmth = PT_Config.SPAWN_WARMTH;

		if (food >= 0.0)
			GetStatEnergy().Set(food * GetStatEnergy().GetMax());
		if (water >= 0.0)
			GetStatWater().Set(water * GetStatWater().GetMax());
		if (warmth >= 0.0)
			GetStatHeatComfort().Set(warmth * GetStatHeatComfort().GetMax());
	}
}
