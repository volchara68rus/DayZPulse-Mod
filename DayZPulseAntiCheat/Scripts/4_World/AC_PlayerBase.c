// ============================================================================
//  AntiCheat - серверные хуки игрока.
//
//  EEHitBy/EEKilled вызываются на сервере при попадании и смерти.
//  Клиенту ничего не отправляется.
// ============================================================================

modded class PlayerBase
{
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		AC_Guard.Get().OnHit(this, source, ammo, dmgZone, damageResult);
	}

	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);

		AC_Guard.Get().OnKilled(this, killer);
	}
}
