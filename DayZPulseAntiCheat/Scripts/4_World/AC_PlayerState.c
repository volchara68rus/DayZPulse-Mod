// ============================================================================
//  AntiCheat - состояние одного игрока.
// ============================================================================

class AC_PlayerState
{
	string id;          // GetId() - хеш, стабильный ключ
	string plainId;     // GetPlainId() - SteamID64 на PC
	string name;

	bool   whitelisted;
	bool   punished;
	bool   wasDead;

	int    connectTime;
	int    graceUntil;
	int    lastCheckTime;
	int    lastSeen;
	int    points;
	int    pointsWindowStart;
	int    speedStrikes;
	int    reports;

	vector lastPos;
	vector lastSafePos;

	float  lastHealth;

	// cooldown по типам нарушений
	ref array<string> violationTypes;
	ref array<int>    violationTimes;

	// окна частоты попаданий/убийств
	ref array<int>    hitTimes;
	ref array<int>    killTimes;

	void AC_PlayerState()
	{
		id = "";
		plainId = "";
		name = "";

		whitelisted = false;
		punished = false;
		wasDead = false;

		connectTime = 0;
		graceUntil = 0;
		lastCheckTime = 0;
		lastSeen = 0;
		points = 0;
		pointsWindowStart = 0;
		speedStrikes = 0;
		reports = 0;

		lastPos = Vector(0, 0, 0);
		lastSafePos = Vector(0, 0, 0);
		lastHealth = 100;

		violationTypes = new array<string>();
		violationTimes = new array<int>();
		hitTimes = new array<int>();
		killTimes = new array<int>();
	}

	bool IsOnCooldown(string type, int now, int cooldownMs)
	{
		for (int i = 0; i < violationTypes.Count(); i++)
		{
			if (violationTypes[i] == type)
				return (now - violationTimes[i]) < cooldownMs;
		}

		return false;
	}

	void MarkViolation(string type, int now)
	{
		for (int i = 0; i < violationTypes.Count(); i++)
		{
			if (violationTypes[i] == type)
			{
				violationTimes[i] = now;
				return;
			}
		}

		violationTypes.Insert(type);
		violationTimes.Insert(now);
	}

	// Убрать из массива отметки времени старше окна.
	static void PruneTimes(array<int> times, int now, int windowMs)
	{
		while (times.Count() > 0 && (now - times[0]) > windowMs)
			times.Remove(0);
	}
}
