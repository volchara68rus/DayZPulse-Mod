// ============================================================================
//  PlayerTracker - перехват создания/удаления объектов мира.
//
//  Хранилища (бочки, ящики, морские сундуки) наследуют
//  DeployableContainer_Base, палатки — TentBase, постройки (флаги, заборы,
//  башни) — BaseBuildingBase. Регистрируя их при появлении и снимая при
//  удалении, панель получает список объектов мира без дорогого перебора
//  (как и с техникой). Container_Base намеренно НЕ трогаем: его наследует и
//  одежда/рюкзаки — это дало бы тысячи лишних объектов.
// ============================================================================

modded class DeployableContainer_Base
{
	override void EEInit()
	{
		super.EEInit();

		if (GetGame().IsServer())
			PT_WorldObjectRegistry.Get().RegisterObject(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			PT_WorldObjectRegistry.Get().Unregister(this);

		super.EEDelete(parent);
	}
}

modded class BaseBuildingBase
{
	override void EEInit()
	{
		super.EEInit();

		if (GetGame().IsServer())
			PT_WorldObjectRegistry.Get().RegisterObject(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			PT_WorldObjectRegistry.Get().Unregister(this);

		super.EEDelete(parent);
	}
}

// TentBase (палатки) наследует ItemBase, а не Container_Base — ловим отдельно.
modded class TentBase
{
	override void EEInit()
	{
		super.EEInit();

		if (GetGame().IsServer())
			PT_WorldObjectRegistry.Get().RegisterObject(this);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			PT_WorldObjectRegistry.Get().Unregister(this);

		super.EEDelete(parent);
	}
}
