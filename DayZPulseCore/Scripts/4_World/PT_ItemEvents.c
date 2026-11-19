// ============================================================================
//  PlayerTracker - хуки ItemBase.
//
//  Мод модифицирует только ItemBase (класс модуля 4_World). Модифицировать
//  EntityAI нельзя: он объявлен в модуле 3_Game, и World-модуль его не видит
//  как точку расширения ("Unknown type 'EntityAI'").
//
//  EECargoIn/EECargoOut — предмет положили в контейнер и достали обратно.
//  Владельца берём по иерархии: если контейнер внутри игрока (рюкзак за
//  спиной) — это его действие.
//
//  OnPlacementComplete — установка объекта игроком (палатка, бочка, ящик).
// ============================================================================

modded class ItemBase
{
	// Предмет положили в контейнер (карго).
	override void EECargoIn(EntityAI item)
	{
		super.EECargoIn(item);

		PlayerBase owner = PlayerBase.Cast(GetHierarchyRootPlayer());

		if (owner)
			PT_Events.Get().ItemEvent("item_cargo_in", item, owner, "");
	}

	// Предмет достали из контейнера.
	override void EECargoOut(EntityAI item)
	{
		super.EECargoOut(item);

		PlayerBase owner = PlayerBase.Cast(GetHierarchyRootPlayer());

		if (owner)
			PT_Events.Get().ItemEvent("item_cargo_out", item, owner, "");
	}

	// Игрок установил объект (палатка, бочка, ящик, набор).
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);

		PlayerBase owner = PlayerBase.Cast(player);

		if (!owner)
			return;

		PlayerIdentity ident = owner.GetIdentity();
		string id = "";
		string name = "";

		if (ident)
		{
			id = ident.GetId();
			name = ident.GetName();
		}

		PT_Events.Get().BuildEvent("build", this, owner, id, name);
	}
}

// Постройки с основанием (забор, вышка, флаг): сборка, разбор и снос частей.
modded class BaseBuildingBase
{
	// Игрок построил часть (стену, ступени, ворота).
	override void OnPartBuiltServer(notnull Man player, string part_name, int action_id)
	{
		super.OnPartBuiltServer(player, part_name, action_id);

		PT_Events.Get().BuildPartEvent("build", this, player, part_name);
	}

	// Игрок разобрал часть.
	override void OnPartDismantledServer(notnull Man player, string part_name, int action_id)
	{
		super.OnPartDismantledServer(player, part_name, action_id);

		PT_Events.Get().BuildPartEvent("dismantle", this, player, part_name);
	}

	// Часть уничтожена (снесли).
	override void OnPartDestroyedServer(Man player, string part_name, int action_id, bool destroyed_by_connected_part = false)
	{
		super.OnPartDestroyedServer(player, part_name, action_id, destroyed_by_connected_part);

		if (player)
			PT_Events.Get().BuildPartEvent("destroy", this, player, part_name);
	}
}
