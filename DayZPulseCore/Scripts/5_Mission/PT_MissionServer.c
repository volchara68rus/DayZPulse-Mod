// ============================================================================
//  PlayerTracker - точка входа на сервере.
//
//  Новый персонаж создаётся в OnClientNewEvent: там движок создаёт существо и
//  выдаёт ванильный набор (StartingEquipSetup). Наш стартовый набор выдаём
//  сразу после этого — иначе он бы не применился: на некоторых конфигурациях
//  OnClientReadyEvent не вызывается (в логах видно, что предметы выдаются, а
//  события входа — нет).
//
//  Наборы и события дублируются в OnClientReadyEvent на случай конфигураций,
//  где порядок событий обратный: и выдача набора, и запись входа защищены от
//  повторов (по идентификатору игрока).
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

	// Каждый кадр сервера. Служит для честного замера FPS: движковые
	// GetAvgFPS()/GetFps() считают кадры рендера и на выделенном сервере
	// отдают мусор (см. PT_FrameMonitor).
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		PT_FrameMonitor.OnUpdate(timeslice);
	}

	// Персонаж только что создан (первый вход и возрождение после смерти).
	override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
	{
		PlayerBase player = super.OnClientNewEvent(identity, pos, ctx);

		if (!player)
			return player;

		// Новый персонаж: сбрасываем отметку о выдаче — набор должен выпасть
		// снова (в том числе после смерти и возрождения).
		PT_SpawnGear.Get().OnRespawn(player);
		PT_SpawnGear.Get().ApplyFor(player);

		// Вход игрока. Повторная запись из OnClientReadyEvent не пройдёт:
		// OnPlayerConnected помнит, кто уже записан.
		PT_Events.Get().OnPlayerConnected(identity, player);

		return player;
	}

	// Персонаж готов к игре (на части конфигураций вызывается вместо/после
	// OnClientNewEvent).
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
