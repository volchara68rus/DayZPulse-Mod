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
		PT_Events.Get().OnPlayerConnected(identity, player);
	}

	// Игрок отключился.
	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
	{
		PT_Events.Get().OnPlayerDisconnected(identity, player);

		super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
	}

	// Игрок возродился после смерти — разрешаем выдать набор снова.
	override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientRespawnEvent(identity, player);

		PT_SpawnGear.Get().OnRespawn(player);
	}
}
