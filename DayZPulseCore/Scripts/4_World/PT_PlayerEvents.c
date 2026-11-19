// ============================================================================
//  PlayerTracker - хуки PlayerBase: киллфид, попадания, предметы в руках.
//
//  Все хуки вызываются на сервере. События складываются в PT_Events, который
//  сбрасывает их в events.jsonl раз в тик трекера.
// ============================================================================

modded class PlayerBase
{
	// Смерть игрока: киллфид.
	override void EEKilled(Object killer)
	{
		// Событие пишем до super: после него состояние игрока уже меняется.
		PT_Events.Get().OnPlayerKilled(this, killer);

		super.EEKilled(killer);
	}

	// Попадание по игроку.
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
		int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		PT_Events.Get().OnPlayerHit(this, source, ammo, dmgZone);
	}

	// Игрок взял предмет в руки.
	override void EEItemIntoHands(EntityAI item)
	{
		super.EEItemIntoHands(item);

		PT_Events.Get().ItemEvent("item_into_hands", item, this, "");
	}

	// Игрок убрал предмет из рук.
	override void EEItemOutOfHands(EntityAI item)
	{
		super.EEItemOutOfHands(item);

		PT_Events.Get().ItemEvent("item_out_hands", item, this, "");
	}

	// Надел предмет в слот (шлем, разгрузка, рюкзак и т.д.).
	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);

		PT_Events.Get().ItemEvent("item_attach", item, this, slot_name);
	}

	// Снял предмет из слота.
	override void EEItemDetached(EntityAI item, string slot_name)
	{
		super.EEItemDetached(item, slot_name);

		PT_Events.Get().ItemEvent("item_detach", item, this, slot_name);
	}
}
