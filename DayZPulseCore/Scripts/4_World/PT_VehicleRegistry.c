// ============================================================================
//  PlayerTracker - реестр техники.
//
//  Панель не может «обойти мир»: полный перебор объектов в Enforce слишком
//  дорог. Поэтому мод запоминает технику в момент её появления и забывает при
//  удалении, как рекомендует официальный гайд по оптимизации.
//
//  Регистрация идёт из modded CarScript.EEInit/EEDelete (см. PT_CarScript.c).
//  Здесь же для карты собирается подробный отчёт по машине: топливо и
//  жидкости, состояние кузова и деталей, что стоит/сломано/отсутствует.
// ============================================================================

class PT_VehicleRegistry
{
	protected static ref PT_VehicleRegistry s_Instance;

	protected ref array<EntityAI> m_Vehicles;

	static PT_VehicleRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new PT_VehicleRegistry();

		return s_Instance;
	}

	void PT_VehicleRegistry()
	{
		m_Vehicles = new array<EntityAI>();
	}

	void Register(EntityAI veh)
	{
		if (!veh)
			return;

		if (m_Vehicles.Find(veh) >= 0)
			return;

		m_Vehicles.Insert(veh);
	}

	void Unregister(EntityAI veh)
	{
		int idx = m_Vehicles.Find(veh);

		if (idx >= 0)
			m_Vehicles.Remove(idx);
	}

	// Чистим мёртвые ссылки (объект мог быть удалён без EEDelete).
	void Prune()
	{
		for (int i = m_Vehicles.Count() - 1; i >= 0; i--)
		{
			if (!m_Vehicles[i])
				m_Vehicles.Remove(i);
		}
	}

	int Count()
	{
		Prune();
		return m_Vehicles.Count();
	}

	// Доступ к списку техники (для поиска машины по координатам).
	array<EntityAI> All()
	{
		Prune();
		return m_Vehicles;
	}

	// Процент здоровья детали (0..100) либо -1, если детали нет.
	static float PartHealthPct(EntityAI part)
	{
		if (!part)
			return -1;

		float h = part.GetHealth("", "Health");
		float m = part.GetMaxHealth("", "Health");

		if (m <= 0)
			return 100;

		return Math.Round((h / m) * 100);
	}

	// JSON-массив позиций и состояния техники для карты.
	// Максимум maxVeh записей, чтобы не раздувать payload.
	string BuildJson(int maxVeh)
	{
		Prune();

		string list = "";
		int sent = 0;

		foreach (EntityAI veh : m_Vehicles)
		{
			if (sent >= maxVeh)
				break;

			vector pos = veh.GetPosition();
			string cls = veh.GetType();

			float health = veh.GetHealth("", "");
			float maxHealth = veh.GetMaxHealth("", "");
			float healthPct = 100;

			if (maxHealth > 0)
				healthPct = Math.Round((health / maxHealth) * 100);

			string entry = "{";
			entry += "\"class\":\"" + PT_Util.Escape(cls) + "\",";
			entry += "\"x\":" + PT_Util.Num(pos[0], 2) + ",";
			entry += "\"y\":" + PT_Util.Num(pos[1], 2) + ",";
			entry += "\"z\":" + PT_Util.Num(pos[2], 2) + ",";
			entry += "\"health\":" + PT_Util.Num(healthPct, 0);

			CarScript car = CarScript.Cast(veh);

			if (!car)
			{
				entry += "}";

				if (list != "")
					list += ",";

				list += entry;
				sent++;
				continue;
			}

			// Жидкости (топливо/охлаждайка/масло/тормоз) — доли 0..1 → проценты.
			float fuel = car.GetFluidFraction(CarFluid.FUEL) * 100;
			float coolant = car.GetFluidFraction(CarFluid.COOLANT) * 100;
			float oil = car.GetFluidFraction(CarFluid.OIL) * 100;
			float brake = car.GetFluidFraction(CarFluid.BRAKE) * 100;

			entry += ",\"fuel\":" + PT_Util.Num(fuel, 0);
			entry += ",\"coolant\":" + PT_Util.Num(coolant, 0);
			entry += ",\"oil\":" + PT_Util.Num(oil, 0);
			entry += ",\"brake\":" + PT_Util.Num(brake, 0);

			// Колёса: стоят из всех.
			int wheelsTotal = car.WheelCount();
			int wheelsPresent = car.WheelCountPresent();

			entry += ",\"wheels\":" + wheelsPresent;
			entry += ",\"wheelsMax\":" + wheelsTotal;

			// Аккумулятор и радиатор — есть/нет + процент здоровья.
			EntityAI battery = car.GetBattery();
			EntityAI radiator = car.GetRadiator();

			entry += ",\"hasBattery\":" + PT_Util.BoolStr(battery != null);
			entry += ",\"batteryHealth\":" + PT_Util.Num(PartHealthPct(battery), 0);
			entry += ",\"hasRadiator\":" + PT_Util.BoolStr(radiator != null);
			entry += ",\"radiatorHealth\":" + PT_Util.Num(PartHealthPct(radiator), 0);
			entry += ",\"needsBattery\":" + PT_Util.BoolStr(car.IsVitalCarBattery());
			entry += ",\"needsRadiator\":" + PT_Util.BoolStr(car.HasRadiator());

			// Двери: сколько на месте, сколько открыто.
			int doorsMissing = 0;
			int doorsOpen = 0;
			int crew = car.CrewSize();

			for (int i = 0; i < crew; i++)
			{
				string doorSlot = car.GetDoorInvSlotNameFromSeatPos(i);

				if (doorSlot == "")
					continue;

				CarDoorState ds = car.GetCarDoorsState(doorSlot);

				if (ds == CarDoorState.DOORS_MISSING)
					doorsMissing++;
				else if (ds == CarDoorState.DOORS_OPEN)
					doorsOpen++;
			}

			entry += ",\"doorsMissing\":" + doorsMissing;
			entry += ",\"doorsOpen\":" + doorsOpen;
			entry += ",\"crew\":" + crew;

			entry += "}";

			if (list != "")
				list += ",";

			list += entry;
			sent++;
		}

		return "[" + list + "]";
	}
}
