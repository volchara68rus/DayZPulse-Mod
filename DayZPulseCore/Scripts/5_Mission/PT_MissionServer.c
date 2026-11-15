// ============================================================================
//  PlayerTracker - точка входа на сервере.
// ============================================================================

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		PT_Tracker.Get().Start();
	}

	override void OnMissionFinish()
	{
		PT_Tracker.Get().Stop();

		super.OnMissionFinish();
	}

	// Новый персонаж готов — выдаём назначенный ему набор (если есть правила).
	override void OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReadyEvent(identity, player);

		PT_SpawnGear.Get().ApplyFor(player);
	}

	// Игрок возродился после смерти — разрешаем выдать набор снова.
	override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientRespawnEvent(identity, player);

		PT_SpawnGear.Get().OnRespawn(player);
	}
}
