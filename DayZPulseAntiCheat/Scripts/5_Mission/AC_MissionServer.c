// ============================================================================
//  AntiCheat - точка входа на сервере.
// ============================================================================

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		AC_Guard.Get().Start();
		AC_Http.Get().Start();
	}

	override void OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReadyEvent(identity, player);

		AC_Guard.Get().OnClientReady(identity, player);
	}

	override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientRespawnEvent(identity, player);

		AC_Guard.Get().OnRespawn(identity, player);
	}

	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
	{
		AC_Guard.Get().OnClientDisconnected(identity, player);

		super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
	}

	override void OnMissionFinish()
	{
		AC_Guard.Get().Stop();
		AC_Http.Get().Stop();

		super.OnMissionFinish();
	}
}
