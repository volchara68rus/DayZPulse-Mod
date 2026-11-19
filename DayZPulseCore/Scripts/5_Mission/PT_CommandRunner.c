// ============================================================================
//  PlayerTracker - выполнение команд на сервере.
//
//  Все действия выполняются только на сервере, клиенту ничего не уходит.
//  Список действий:
//    ping                                   - проверка канала
//    spawn   class [x y z | container] [qty health persist attachments]
//    give    id class [qty attachments]
//    preset  name [id | oid | x+z]          - предмет(ы) из файла пресета
//    delete  [oid | all | class x z radius]
//    weather [overcast rain fog snow wind storm transition freeze]
//    time    [year month day hour minute]
//    teleport id x z [y]
//    heal    id [value percent]
//    damage  id [value percent]
//    kill    id
//
//  health: -1 - заводское, 0..1 - доля, 1..100 - проценты.
// ============================================================================

class PT_Result
{
	bool   ok;
	string message;
	ref array<int> oids;

	// Имена параметров НЕ должны совпадать с полями класса: Enforce в
	// static-методе пытается трактовать такое имя как нестатический член.
	static PT_Result Make(bool okValue, string messageValue)
	{
		PT_Result result = new PT_Result();
		result.Set(okValue, messageValue);

		return result;
	}

	static PT_Result MakeWithOids(bool okValue, string messageValue, array<int> oidsValue)
	{
		PT_Result result = Make(okValue, messageValue);
		result.SetOids(oidsValue);

		return result;
	}

	// Заполнение через методы: static-метод не может обращаться к
	// нестатическим полям напрямую (ошибка Enforce).
	void Set(bool okValue, string messageValue)
	{
		ok = okValue;
		message = messageValue;
	}

	void SetOids(array<int> oidsValue)
	{
		oids = oidsValue;
	}
}

class PT_CommandRunner
{
	static PT_Result Execute(PT_Command cmd)
	{
		if (!cmd)
			return PT_Result.Make(false, "null command");

		string action = cmd.action;

		if (action == "ping")
			return PT_Result.Make(true, "pong");

		if (action == "spawn")
			return Spawn(cmd);

		if (action == "give")
			return Give(cmd);

		if (action == "preset")
			return PresetCmd(cmd);

		if (action == "delete")
			return DeleteCmd(cmd);

		if (action == "weather")
			return WeatherCmd(cmd);

		if (action == "time")
			return TimeCmd(cmd);

		if (action == "teleport")
			return TeleportCmd(cmd);

		if (action == "heal")
			return HealthCmd(cmd, true);

		if (action == "damage")
			return HealthCmd(cmd, false);

		if (action == "kill")
			return KillCmd(cmd);

		if (action == "mute")
			return MuteCmd(cmd, true);

		if (action == "unmute")
			return MuteCmd(cmd, false);

		if (action == "warn")
			return WarnCmd(cmd);

		if (action == "message")
			return MessageCmd(cmd);

		if (action == "broadcast")
			return BroadcastCmd(cmd);

		if (action == "inventory")
			return InventoryCmd(cmd);

		if (action == "vehicle")
			return VehicleCmd(cmd);

		return PT_Result.Make(false, "unknown action: " + action);
	}

	// ----------------------------------------------------------------- spawn ---

	static PT_Result Spawn(PT_Command cmd)
	{
		string type = cmd.GetStr("class", "");

		if (type == "")
			return PT_Result.Make(false, "spawn: 'class' is required");

		if (type.Length() > 64)
			return PT_Result.Make(false, "spawn: class name too long");

		EntityAI container = null;

		if (cmd.Has("container"))
		{
			container = PT_SpawnRegistry.Get().Find(cmd.GetInt("container", -1));

			if (!container)
				return PT_Result.Make(false, "spawn: container not found (oid " + cmd.GetStr("container", "?") + ")");
		}

		vector pos = cmd.GetPos();
		int qty = ClampQty(cmd.GetInt("qty", 1));

		int flags = ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_UPDATEPATHGRAPH;
		if (!cmd.GetBool("persist", false))
			flags = flags | ECE_NOLIFETIME;

		float health = cmd.GetFloat("health", -1);
		string attachments = cmd.GetStr("attachments", "");

		array<int> oids = new array<int>();
		int created = 0;

		for (int i = 0; i < qty; i++)
		{
			EntityAI item = CreateItem(type, pos, flags, container);

			if (!item)
				continue;

			ApplyHealth(item, health);
			AttachAll(item, attachments);

			int oid = PT_SpawnRegistry.Get().Register(item);
			if (oid > 0)
				oids.Insert(oid);

			created++;
		}

		if (created == 0)
			return PT_Result.Make(false, "spawn: cannot create '" + type + "' (unknown class?)");

		string where = "";

		if (container)
			where = " in container";

		return PT_Result.MakeWithOids(true, "spawned " + created + "x " + type + where, oids);
	}

	// ------------------------------------------------------------------ give ---

	static PT_Result Give(PT_Command cmd)
	{
		string type = cmd.GetStr("class", "");

		if (type == "")
			return PT_Result.Make(false, "give: 'class' is required");

		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "give: player not found");

		int qty = ClampQty(cmd.GetInt("qty", 1));
		string attachments = cmd.GetStr("attachments", "");
		string dest = cmd.GetStr("target", "");
		string slotName = cmd.GetStr("slot", "");

		// Куда положить: в руки, в конкретный слот, в карго или на землю рядом.
		bool toHands = (dest == "hands");
		bool toGround = (dest == "ground");

		array<int> oids = new array<int>();
		int created = 0;

		for (int i = 0; i < qty; i++)
		{
			EntityAI item = null;

			if (toGround)
				item = SpawnAtPlayer(player, type);
			else if (slotName != "")
				item = player.GetInventory().CreateAttachment(type);
			else
				item = EntityAI.Cast(player.GetInventory().CreateInInventory(type));

			if (!item)
				continue;

			AttachAll(item, attachments);

			// В руки — только первый предмет (двуручный слот), остальное в карго.
			if (toHands && i == 0)
				player.LocalTakeEntityToHands(item);

			int oid = PT_SpawnRegistry.Get().Register(item);
			if (oid > 0)
				oids.Insert(oid);

			created++;
		}

		if (created == 0)
			return PT_Result.Make(false, "give: cannot add '" + type + "' to inventory");

		string where = "";

		if (toGround)
			where = " on ground";
		else if (slotName != "")
			where = " to slot " + slotName;
		else if (toHands)
			where = " to hands";

		return PT_Result.MakeWithOids(true, "gave " + created + "x " + type + where, oids);
	}

	// Спавн предмета на земле у ног игрока (для перетаскивания «на землю»).
	static EntityAI SpawnAtPlayer(PlayerBase player, string type)
	{
		vector pos = player.GetPosition();
		vector dir = player.GetDirection();
		pos = pos + dir * 1.5;

		int flags = ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_UPDATEPATHGRAPH | ECE_NOLIFETIME;

		return EntityAI.Cast(GetGame().CreateObjectEx(type, pos, flags));
	}

	// ---------------------------------------------------------------- preset ---

	static PT_Result PresetCmd(PT_Command cmd)
	{
		string name = cmd.GetStr("name", "");

		if (name == "")
			return PT_Result.Make(false, "preset: 'name' is required");

		if (!IsSafeName(name))
			return PT_Result.Make(false, "preset: bad name (a-z, 0-9, _, -)");

		PT_Preset preset = PT_PresetLoader.Load(name);

		if (!preset)
			return PT_Result.Make(false, "preset: '" + name + "' not found in " + PT_Config.PRESET_DIR);

		if (preset.items.Count() == 0)
			return PT_Result.Make(false, "preset: '" + name + "' is empty");

		EntityAI container = null;
		vector pos = cmd.GetPos();

		if (cmd.Has("id"))
		{
			PlayerBase player = FindPlayer(cmd.GetStr("id", ""));

			if (!player)
				return PT_Result.Make(false, "preset: player not found");

			container = player;
		}
		else if (cmd.Has("oid"))
		{
			int oid = cmd.GetInt("oid", -1);
			container = PT_SpawnRegistry.Get().Find(oid);

			if (!container)
				return PT_Result.Make(false, "preset: container not found (oid " + cmd.GetStr("oid", "?") + ")");
		}
		else if (!(cmd.Has("x") && cmd.Has("z")))
		{
			return PT_Result.Make(false, "preset: target required (id / oid / x+z)");
		}

		int flags = ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_UPDATEPATHGRAPH | ECE_NOLIFETIME;

		array<int> oids = new array<int>();
		int created = 0;

		foreach (PT_PresetItem spec : preset.items)
		{
			created += SpawnSpec(spec, pos, flags, container, oids, 0);
		}

		if (created == 0)
			return PT_Result.Make(false, "preset: nothing created (check classnames)");

		string where = "on ground";
		if (container)
			where = "in container";

		return PT_Result.MakeWithOids(true, "preset '" + name + "': " + created + " item(s) " + where, oids);
	}

	// ---------------------------------------------------------------- delete ---

	static PT_Result DeleteCmd(PT_Command cmd)
	{
		PT_SpawnRegistry registry = PT_SpawnRegistry.Get();

		if (cmd.Has("oid"))
		{
			int oid = cmd.GetInt("oid", -1);

			if (!registry.Delete(oid))
				return PT_Result.Make(false, "delete: oid " + oid + " not found");

			return PT_Result.Make(true, "deleted oid " + oid);
		}

		bool all = cmd.GetBool("all", false);
		string type = cmd.GetStr("class", "");
		bool hasPos = cmd.Has("x") && cmd.Has("z");

		if (!all && type == "" && !hasPos)
			return PT_Result.Make(false, "delete: provide oid, all=1, class and/or x+z+radius");

		float radius = cmd.GetFloat("radius", 0);
		if (radius < 0)
			radius = 0;
		if (radius > 500)
			radius = 500;

		int removed = registry.DeleteMatching(type, cmd.GetPos(), radius);

		return PT_Result.Make(true, "deleted " + removed + " object(s)");
	}

	// --------------------------------------------------------------- weather ---

	static PT_Result WeatherCmd(PT_Command cmd)
	{
		Weather weather = GetGame().GetWeather();

		if (!weather)
			return PT_Result.Make(false, "weather: controller is not available");

		float transition = Math.Max(cmd.GetFloat("transition", 0), 0);

		// Закрепить погоду: CE перестанет пересчитывать прогноз сам.
		if (cmd.Has("freeze"))
			weather.SetWeatherUpdateFreeze(cmd.GetBool("freeze", false));

		bool changed = false;

		if (cmd.Has("overcast"))
		{
			weather.GetOvercast().Set(Math.Clamp(cmd.GetFloat("overcast", 0), 0, 1), transition, 0);
			changed = true;
		}

		if (cmd.Has("rain"))
		{
			weather.GetRain().Set(Math.Clamp(cmd.GetFloat("rain", 0), 0, 1), transition, 0);
			changed = true;
		}

		if (cmd.Has("fog"))
		{
			weather.GetFog().Set(Math.Clamp(cmd.GetFloat("fog", 0), 0, 1), transition, 0);
			changed = true;
		}

		if (cmd.Has("snow"))
		{
			weather.GetSnowfall().Set(Math.Clamp(cmd.GetFloat("snow", 0), 0, 1), transition, 0);
			changed = true;
		}

		if (cmd.Has("wind"))
		{
			weather.GetWindMagnitude().Set(Math.Clamp(cmd.GetFloat("wind", 0), 0, 1), transition, 0);
			changed = true;
		}

		if (cmd.Has("storm"))
		{
			weather.SetStorm(Math.Clamp(cmd.GetFloat("storm", 0), 0, 1), Math.Clamp(cmd.GetFloat("stormThreshold", 0.5), 0, 1), Math.Max(cmd.GetFloat("stormTimeout", 0), 0));
			changed = true;
		}

		if (!changed)
			return PT_Result.Make(false, "weather: no fields (overcast/rain/fog/snow/wind/storm/freeze)");

		return PT_Result.Make(true, "weather applied");
	}

	// ------------------------------------------------------------------ time ---

	static PT_Result TimeCmd(PT_Command cmd)
	{
		World world = GetGame().GetWorld();

		if (!world)
			return PT_Result.Make(false, "time: world is not available");

		int year;
		int month;
		int day;
		int hour;
		int minute;

		world.GetDate(year, month, day, hour, minute);

		year = cmd.GetInt("year", year);
		month = cmd.GetInt("month", month);
		day = cmd.GetInt("day", day);
		hour = cmd.GetInt("hour", hour);
		minute = cmd.GetInt("minute", minute);

		hour = Math.Clamp(hour, 0, 23);
		minute = Math.Clamp(minute, 0, 59);

		world.SetDate(year, month, day, hour, minute);

		return PT_Result.Make(true, "server time set to " + hour + ":" + minute);
	}

	// -------------------------------------------------------------- teleport ---

	static PT_Result TeleportCmd(PT_Command cmd)
	{
		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "teleport: player not found");

		if (!cmd.Has("x") || !cmd.Has("z"))
			return PT_Result.Make(false, "teleport: 'x' and 'z' are required");

		vector pos = player.GetPosition();
		pos[0] = cmd.GetFloat("x", pos[0]);
		pos[2] = cmd.GetFloat("z", pos[2]);

		if (cmd.Has("y"))
			pos[1] = cmd.GetFloat("y", pos[1]);

		player.SetPosition(pos);
		SetVelocity(player, Vector(0, 0, 0));

		// Если вместе с PlayerTracker установлен аддон AntiCheat, на время
		// телепорта стоит снять с игрока проверки движения. Раскомментируйте:
		// AC_Guard.Get().Exempt(player, 15000);

		return PT_Result.Make(true, "teleported to " + PT_Util.Num(pos[0], 1) + " / " + PT_Util.Num(pos[1], 1) + " / " + PT_Util.Num(pos[2], 1));
	}

	// ---------------------------------------------------------------- health ---

	// Лечение: при heal восстанавливаем не только здоровье, но и запас еды,
	// воды, стамины, крови, тепла, убираем болезни и кровотечение.
	static PT_Result HealthCmd(PT_Command cmd, bool heal)
	{
		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "player not found");

		float maxHealth = player.GetMaxHealth("", "Health");
		float value = cmd.GetFloat("value", -1);
		float target;

		if (value < 0)
		{
			if (heal)
				target = maxHealth;
			else
				target = 0;
		}
		else if (cmd.GetBool("percent", true))
			target = maxHealth * (value / 100.0);
		else
			target = value;

		target = Math.Clamp(target, 0, maxHealth);

		// Полное лечение: сначала восстанавливаем кровь и снимаем болезни,
		// и только потом ставим здоровье. Иначе здоровье подскакивает при
		// нулевой крови — игра пересчитывает шок и игрок падает в унку.
		if (heal && value < 0)
			RestoreSurvival(player);

		player.SetHealth("", "Health", target);

		return PT_Result.Make(true, "health set to " + PT_Util.Num(target, 1));
	}

	// Восстанавливает все показатели выживания до максимума и снимает болезни.
	static void RestoreSurvival(PlayerBase player)
	{
		if (!player)
			return;

		// Порядок важен: сначала причины унки (кровь, шок, болезни), затем
		// восстанавливающие статы. Иначе игра успевает пересчитать бессознательное
		// состояние по нулевой крови и игрок «падает».

		// Кровь — это зона урона (DamageSystem), а не PlayerStat.
		player.SetHealth("", "Blood", player.GetMaxHealth("", "Blood"));

		// Кровотечение снимаем сразу.
		player.SetBleedingBits(0);

		// ВАЖНО: зона "Shock" в DayZ работает «наоборот» — это НЕ урон, а
		// устойчивость к обмороку. Мало шока (<=25) — игрок падает в унку,
		// много (>=50) — в сознании. Для лечения шок нужно ЗАПОЛНИТЬ до
		// максимума, иначе SetHealth("Shock", 0) сам валит игрока в унку.
		player.SetHealth("", "Shock", player.GetMaxHealth("", "Shock"));

		// Болезни и травмы, включая бессознательное состояние. DeactivateModifier()
		// внутри делает m_ModifierList.Get(id).Deactivate() без проверки на null:
		// если модификатор не зарегистрирован в этой версии игры (например,
		// устаревший MDF_WOUND_INFECTION), движок бросает исключение и весь
		// RestoreSurvival обрывается. Поэтому снимаем только активные модификаторы
		// через GetModifier() с проверкой.
		ModifiersManager mods = player.GetModifiersManager();

		if (mods)
		{
			int diseases[] = {
				eModifiers.MDF_BLEEDING, eModifiers.MDF_BLINDED, eModifiers.MDF_HEART_ATTACK,
				eModifiers.MDF_POISONING, eModifiers.MDF_HEMOLYTIC_REACTION,
				eModifiers.MDF_INFLUENZA, eModifiers.MDF_CHOLERA, eModifiers.MDF_SALMONELLA,
				eModifiers.MDF_BRAIN, eModifiers.MDF_WOUND_INFECTION, eModifiers.MDF_WOUND_INFECTION1,
				eModifiers.MDF_WOUND_INFECTION2, eModifiers.MDF_BROKEN_LEGS, eModifiers.MDF_BROKEN_ARMS,
				eModifiers.MDF_FEVER, eModifiers.MDF_COMMON_COLD, eModifiers.MDF_HEAVYMETAL1,
				eModifiers.MDF_HEAVYMETAL2, eModifiers.MDF_HEAVYMETAL3, eModifiers.MDF_PNEUMONIA,
				eModifiers.MDF_UNCONSCIOUSNESS, eModifiers.MDF_STUFFED, eModifiers.MDF_VOMITSTUFFED,
				eModifiers.MDF_FATIGUE, eModifiers.MDF_HEAT_STROKE1, eModifiers.MDF_HEAT_STROKE2,
				eModifiers.MDF_HEAT_STROKE3, eModifiers.MDF_HEAT_STROKE4
			};

			foreach (int id : diseases)
			{
				ModifierBase modifier = mods.GetModifier(id);

				if (modifier && modifier.IsActive())
					mods.DeactivateModifier(id, false);
			}
		}

		// Основные статы (вода, еда, стамина, тепло). HeatComfort может быть
		// отрицательным (холодно) — ставим максимум, чтобы игрок не мёрз.
		player.GetStatWater().Set(player.GetStatWater().GetMax());
		player.GetStatEnergy().Set(player.GetStatEnergy().GetMax());
		player.GetStatStamina().Set(player.GetStatStamina().GetMax());
		player.GetStatHeatComfort().Set(player.GetStatHeatComfort().GetMax());
		player.GetStatHeatBuffer().Set(player.GetStatHeatBuffer().GetMax());
		player.GetStatToxicity().Set(0);
	}

	// ------------------------------------------------------------------ kill ---

	static PT_Result KillCmd(PT_Command cmd)
	{
		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "kill: player not found");

		player.SetHealth("", "Health", 0);

		return PT_Result.Make(true, "player killed");
	}

	// ---------------------------------------------------------------- mute ---

	// Мут хранится на стороне панели (таблица mutes). Мод только фиксирует
	// состояние в реестре, чтобы не отправлять сообщения замьюченного игрока.
	static ref array<string> s_Muted;

	static PT_Result MuteCmd(PT_Command cmd, bool on)
	{
		if (!s_Muted)
			s_Muted = new array<string>();

		string id = cmd.GetStr("id", cmd.GetStr("player", ""));

		if (id == "")
			return PT_Result.Make(false, "mute: 'id' is required");

		if (on)
		{
			if (s_Muted.Find(id) < 0)
				s_Muted.Insert(id);

			return PT_Result.Make(true, "muted " + id);
		}

		s_Muted.RemoveItem(id);
		return PT_Result.Make(true, "unmuted " + id);
	}

	static bool IsMutedId(string id)
	{
		if (!s_Muted || id == "")
			return false;

		return s_Muted.Find(id) >= 0;
	}

	// ---------------------------------------------------------------- warn ---

	// Предупреждение игроку панель отправляет через RCON (say N). Здесь —
	// только подтверждение, чтобы действие можно было поставить в очередь мода.
	static PT_Result WarnCmd(PT_Command cmd)
	{
		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "warn: player not found");

		string text = cmd.GetStr("text", "Предупреждение от администрации");

		Print("[DayZPulseCore] warn -> " + text);

		return PT_Result.Make(true, "warned " + player.GetIdentity().GetName());
	}

	// Личное сообщение игроку. Ванильный RPC_USER_ACTION_MESSAGE есть у клиента
	// в любой сборке DayZ, поэтому игрок увидит текст без нашего мода у себя.
	static PT_Result MessageCmd(PT_Command cmd)
	{
		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "message: player not found");

		string text = cmd.GetStr("text", "");

		if (text == "")
			return PT_Result.Make(false, "message: 'text' is required");

		Param1<string> param = new Param1<string>(text);
		GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE, param, true, player.GetIdentity());

		return PT_Result.Make(true, "message sent to " + player.GetIdentity().GetName());
	}

	// Broadcast всем игрокам. RCON (say -1) на серверах без BattlEye недоступен,
	// поэтому шлём ванильный RPC_USER_ACTION_MESSAGE каждому игроку: его клиент
	// показывает текст без нашего мода у себя.
	static PT_Result BroadcastCmd(PT_Command cmd)
	{
		string text = cmd.GetStr("text", "");

		if (text == "")
			return PT_Result.Make(false, "broadcast: 'text' is required");

		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		int sent = 0;

		foreach (Man man : players)
		{
			PlayerBase pb = PlayerBase.Cast(man);

			if (!pb)
				continue;

			PlayerIdentity ident = pb.GetIdentity();

			if (!ident)
				continue;

			Param1<string> param = new Param1<string>(text);
			GetGame().RPCSingleParam(pb, ERPCs.RPC_USER_ACTION_MESSAGE, param, true, ident);
			sent++;
		}

		if (sent == 0)
			return PT_Result.Make(true, "broadcast: no players online");

		return PT_Result.Make(true, "broadcast sent to " + sent + " player(s)");
	}

	// ------------------------------------------------------------- vehicle ---

	// Обслуживание техники по координатам (ближайшая машина): заправить,
	// починить, поставить колёса/аккумулятор/радиатор. Панель присылает
	// x/z из карточки машины; мод находит ближайшую технику в радиусе.
	static PT_Result VehicleCmd(PT_Command cmd)
	{
		if (!cmd.Has("x") || !cmd.Has("z"))
			return PT_Result.Make(false, "vehicle: x+z required");

		vector target = cmd.GetPos();
		float radius = cmd.GetFloat("radius", 8);

		if (radius < 1)
			radius = 1;
		if (radius > 60)
			radius = 60;

		CarScript car = FindVehicleNear(target, radius);

		if (!car)
			return PT_Result.Make(false, "vehicle: not found near target");

		string op = cmd.GetStr("op", "");

		if (op == "refuel")
		{
			car.Fill(CarFluid.FUEL, 1.0);
			return PT_Result.Make(true, "vehicle refueled (" + car.GetType() + ")");
		}

		if (op == "repair")
		{
			car.SetHealth("", "", car.GetMaxHealth("", ""));
			return PT_Result.Make(true, "vehicle repaired (" + car.GetType() + ")");
		}

		if (op == "fixparts")
		{
			return VehicleFixParts(car);
		}

		return PT_Result.Make(false, "vehicle: unknown op '" + op + "'");
	}

	// Ставит недостающие детали и приводит их в порядок.
	static PT_Result VehicleFixParts(CarScript car)
	{
		if (!car)
			return PT_Result.Make(false, "vehicle: no car");

		// Привести имеющиеся детали в порядок.
		EntityAI battery = car.GetBattery();

		if (battery)
			battery.SetHealth("", "Health", battery.GetMaxHealth("", ""));

		EntityAI radiator = car.GetRadiator();

		if (radiator)
			radiator.SetHealth("", "Health", radiator.GetMaxHealth("", ""));

		// Если детали нужны, но их нет — доставить стандартные.
		if (car.IsVitalCarBattery() && !battery)
		{
			EntityAI bat = car.GetInventory().CreateAttachment("CarBattery");

			if (bat)
				bat.SetHealth("", "Health", bat.GetMaxHealth("", ""));
		}

		if (car.HasRadiator() && !radiator)
		{
			EntityAI rad = car.GetInventory().CreateAttachment("CarRadiator");

			if (rad)
				rad.SetHealth("", "Health", rad.GetMaxHealth("", ""));
		}

		// Полные жидкости.
		car.Fill(CarFluid.FUEL, 1.0);
		car.Fill(CarFluid.COOLANT, 1.0);
		car.Fill(CarFluid.OIL, 1.0);
		car.Fill(CarFluid.BRAKE, 1.0);

		return PT_Result.Make(true, "vehicle parts fixed");
	}

	// Ближайшая техника к точке в радиусе.
	static CarScript FindVehicleNear(vector pos, float radius)
	{
		CarScript best = null;
		float bestDist = radius + 1;

		// Ищем среди зарегистрированной техники (без дорогого перебора мира).
		foreach (EntityAI veh : PT_VehicleRegistry.Get().All())
		{
			CarScript car = CarScript.Cast(veh);

			if (!car)
				continue;

			float d = vector.Distance(car.GetPosition(), pos);

			if (d <= radius && d < bestDist)
			{
				bestDist = d;
				best = car;
			}
		}

		return best;
	}

	// ------------------------------------------------------------ inventory ---

	// Сборка снимка инвентаря игрока.
	//
	// Формат = объект с разделами, а не плоский список, чтобы панель могла
	// нарисовать визуальный инвентарь (слоты экипировки + карго с вложенностью):
	//   {
	//     "hands":  {...} | null,          - предмет в руках
	//     "slots":  [ {slot, item}, ... ], - экипировка по слотам
	//     "cargo":  [ {cat:'item'|'cargo', ...}, ... ] - содержимое игрока/рюкзака
	//   }
	// Каждый item: {class, qty, health, cargo:[...]}
	static PT_Result InventoryCmd(PT_Command cmd)
	{
		PlayerBase player = FindPlayer(cmd.GetStr("id", cmd.GetStr("player", "")));

		if (!player)
			return PT_Result.Make(false, "inventory: player not found");

		string json = BuildInventoryJson(player);
		PT_Tracker.Get().PostInventory(player.GetIdentity().GetName(), player.GetIdentity().GetId(), json);

		return PT_Result.Make(true, "inventory sent");
	}

	// Слоты экипировки персонажа (порядок как в интерфейсе игры).
	// Имена — это имена Slot_<name> из DZ/data; их понимает GetSlotIdFromString.
	// Важно: головной слот называется "Headgear" (а не "Head"), слота "Neck" у
	// персонажа нет, зато есть "Armband".
	static TStringArray EquipmentSlots()
	{
		TStringArray slots = new TStringArray();
		slots.Insert("Headgear");
		slots.Insert("Mask");
		slots.Insert("Eyewear");
		slots.Insert("Body");
		slots.Insert("Vest");
		slots.Insert("Back");
		slots.Insert("Hips");
		slots.Insert("Legs");
		slots.Insert("Feet");
		slots.Insert("Gloves");
		slots.Insert("Armband");
		return slots;
	}

	static string BuildInventoryJson(PlayerBase player)
	{
		if (!player)
			return "{}";

		string hands = "null";
		EntityAI inHands = player.GetItemInHands();

		if (inHands)
			hands = BuildItemJson(inHands, 0);

		// Экипировка по слотам.
		string slots = "";
		TStringArray eq = EquipmentSlots();

		for (int i = 0; i < eq.Count(); i++)
		{
			string slotName = eq[i];
			EntityAI att = player.GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString(slotName));

			if (!att)
				continue;

			if (slots != "")
				slots += ",";

			slots += "{\"slot\":\"" + PT_Util.Escape(slotName) + "\",";
			slots += "\"item\":" + BuildItemJson(att, 0) + "}";
		}

		// Содержимое «на игроке» — карго самого персонажа (надетые контейнеры:
		// рюкзак/жилет идут в slots, их содержимое вложено в item.cargo).
		string cargo = BuildCargoJson(player, 0);

		string json = "{";
		json += "\"hands\":" + hands + ",";
		json += "\"slots\":[" + slots + "],";
		json += "\"cargo\":[" + cargo + "]";
		json += "}";

		return json;
	}

	// Содержимое контейнера: прямое карго + карго надетого (рюкзак/жилет),
	// в виде плоского списка с пометкой контейнера-родителя.
	static string BuildCargoJson(EntityAI owner, int depth)
	{
		if (!owner || depth > 4)
			return "";

		string list = "";
		list = AppendCargo(owner, list, depth);

		return list;
	}

	static string AppendCargo(EntityAI container, string list, int depth)
	{
		if (!container || depth > 4)
			return list;

		CargoBase cargo = container.GetInventory().GetCargo();

		if (cargo)
		{
			int count = cargo.GetItemCount();

			for (int i = 0; i < count; i++)
			{
				EntityAI it = cargo.GetItem(i);

				if (!it)
					continue;

				if (list != "")
					list += ",";

				list += "{\"cat\":\"item\",\"owner\":\"" + PT_Util.Escape(container.GetType()) + "\",";
				list += "\"item\":" + BuildItemJson(it, depth + 1) + "}";
			}
		}

		return list;
	}

	// Предмет + (рекурсивно) его содержимое (вложенные контейнеры).
	static string BuildItemJson(EntityAI item, int depth)
	{
		if (!item)
			return "null";

		string cls = item.GetType();
		float health = item.GetHealth("", "");
		float maxHealth = item.GetMaxHealth("", "");
		int qty = 1;

		ItemBase ib = ItemBase.Cast(item);

		if (ib)
		{
			if (ib.GetQuantity() > 0)
				qty = ib.GetQuantity();
		}

		string json = "{";
		json += "\"class\":\"" + PT_Util.Escape(cls) + "\",";
		json += "\"qty\":" + qty + ",";
		json += "\"health\":" + PT_Util.Num(health, 1);

		// Содержимое (если контейнер и не слишком глубоко).
		if (depth < 3 && item.GetInventory() && item.GetInventory().GetCargo())
		{
			string inner = BuildCargoJson(item, depth + 1);
			json += ",\"cargo\":[" + inner + "]";
		}

		json += "}";

		return json;
	}


	static int ClampQty(int qty)
	{
		if (qty < 1)
			return 1;

		if (qty > PT_Config.SPAWN_MAX_QTY)
			return PT_Config.SPAWN_MAX_QTY;

		return qty;
	}

	static bool IsSafeName(string name)
	{
		if (name.Length() > 64)
			return false;

		string allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";

		for (int i = 0; i < name.Length(); i++)
		{
			if (allowed.IndexOf(name.Substring(i, 1)) < 0)
				return false;
		}

		return true;
	}

	static EntityAI CreateItem(string type, vector pos, int flags, EntityAI parent)
	{
		if (parent)
			return EntityAI.Cast(parent.GetInventory().CreateInInventory(type));

		return EntityAI.Cast(GetGame().CreateObjectEx(type, pos, flags));
	}

	static void ApplyHealth(EntityAI item, float spec)
	{
		if (spec < 0)
			return;

		float ratio = spec;

		// 0..1 - доля, больше 1 - проценты.
		if (spec > 1)
			ratio = spec / 100.0;

		ratio = Math.Clamp(ratio, 0, 1);
		item.SetHealth("", "Health", item.GetMaxHealth("", "Health") * ratio);
	}

	static int AttachAll(EntityAI item, string csv)
	{
		if (csv == "")
			return 0;

		int attached = 0;
		array<string> names = PT_Util.Split(csv, ",");

		foreach (string rawName : names)
		{
			string name = PT_Util.TrimStr(rawName);

			if (name == "")
				continue;

			if (item.GetInventory().CreateAttachment(name))
				attached++;
		}

		return attached;
	}

	// Рекурсивный спавн одного элемента пресета (с attachments и cargo).
	static int SpawnSpec(PT_PresetItem spec, vector pos, int flags, EntityAI parent, array<int> oids, int depth)
	{
		if (!spec || depth > 3)
			return 0;

		string type = spec.className;

		if (type == "")
			return 0;

		int qty = ClampQty(spec.quantity);
		int created = 0;

		for (int i = 0; i < qty; i++)
		{
			EntityAI item = CreateItem(type, pos, flags, parent);

			if (!item)
				continue;

			ApplyHealth(item, spec.health);

			if (spec.attachments)
			{
				foreach (string att : spec.attachments)
				{
					if (att != "")
						item.GetInventory().CreateAttachment(att);
				}
			}

			if (spec.cargo)
			{
				foreach (PT_PresetItem child : spec.cargo)
				{
					SpawnSpec(child, pos, flags, item, oids, depth + 1);
				}
			}

			int oid = PT_SpawnRegistry.Get().Register(item);
			if (oid > 0)
				oids.Insert(oid);

			created++;
		}

		return created;
	}

	static PlayerBase FindPlayer(string id)
	{
		if (id == "")
			return null;

		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);

			if (!player)
				continue;

			PlayerIdentity identity = player.GetIdentity();

			if (!identity)
				continue;

			if (identity.GetPlainId() == id || identity.GetId() == id || identity.GetName() == id)
				return player;
		}

		return null;
	}
}
