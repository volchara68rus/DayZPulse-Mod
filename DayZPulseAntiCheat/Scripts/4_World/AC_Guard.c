// ============================================================================
//  AntiCheat - ядро.
//
//  Работает только на выделенном сервере. Проверяет то, что сервер и так
//  знает: позиции игроков и события урона. Клиенту ничего не отправляет.
//
//  Нарушения копят очки. При достижении KICK_POINTS игрок кикается,
//  при BAN_POINTS (ENFORCEMENT = 2) - добавляется в bans.txt и кикается.
// ============================================================================

class AC_Guard
{
	protected static ref AC_Guard s_Instance;

	protected ref array<ref AC_PlayerState> m_States;
	protected ref array<string> m_Bans;
	protected ref array<string> m_Whitelist;

	protected bool m_Running;
	protected int  m_StartedAt;
	protected int  m_LastReload;
	protected int  m_Reports;
	protected int  m_Kicks;
	protected int  m_BansIssued;

	static AC_Guard Get()
	{
		if (!s_Instance)
			s_Instance = new AC_Guard();

		return s_Instance;
	}

	// ------------------------------------------------------------- lifecycle ---

	void Start()
	{
		if (m_Running)
			return;

		if (!GetGame().IsDedicatedServer())
		{
			Print("[DayZPulseAntiCheat] Not a dedicated server - disabled.");
			return;
		}

		// Читаем config.json (или создаём с умолчаниями).
		AC_Config.Load();

		if (!AC_Config.ENABLED)
		{
			Print("[DayZPulseAntiCheat] Disabled by AC_Config.ENABLED.");
			return;
		}

		m_States = new array<ref AC_PlayerState>();
		m_Bans = new array<string>();
		m_Whitelist = new array<string>();
		m_StartedAt = GetGame().GetTime();
		m_LastReload = 0;
		m_Reports = 0;
		m_Kicks = 0;
		m_BansIssued = 0;

		MakeDirectory(AC_Config.DIR);
		AC_Log.Init();
		ReloadLists();

		m_Running = true;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, AC_Config.CHECK_INTERVAL_MS, true);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ProcessCommands, AC_Config.COMMAND_INTERVAL_MS, true);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ReloadLists, AC_Config.RELOAD_LISTS_MS, true);

		AC_Log.Info("Started | enforce=" + AC_Config.ENFORCEMENT + " | check=" + AC_Config.CHECK_INTERVAL_MS + "ms | bans=" + m_Bans.Count() + " | whitelist=" + m_Whitelist.Count());
	}

	void Stop()
	{
		if (!m_Running)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Tick);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ProcessCommands);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ReloadLists);

		m_Running = false;

		AC_Log.Info("Stopped | reports=" + m_Reports + " | kicks=" + m_Kicks + " | bans=" + m_BansIssued + " | httpQueue=" + AC_Log.PendingHttp());
	}

	// ---------------------------------------------------------------- events ---

	void OnClientReady(PlayerIdentity identity, PlayerBase player)
	{
		if (!m_Running || !identity || !player)
			return;

		string id = identity.GetId();
		string plainId = identity.GetPlainId();
		string name = identity.GetName();

		if (IsBanned(id) || IsBanned(plainId))
		{
			AC_Log.Info("BANNED connect rejected | name=\"" + AC_Util.Sanitize(name) + "\" id=" + id + " plain=" + plainId);
			GetGame().DisconnectPlayer(identity);
			return;
		}

		int now = GetGame().GetTime();
		AC_PlayerState st = FindStateById(id);

		if (!st)
		{
			st = new AC_PlayerState();
			st.id = id;
			st.plainId = plainId;
			st.name = name;
			st.connectTime = now;
			st.graceUntil = now + AC_Config.STARTUP_GRACE_MS;
			st.lastSeen = now;
			st.lastPos = player.GetPosition();
			st.lastSafePos = st.lastPos;
			st.lastCheckTime = now;
			st.lastHealth = player.GetHealth("", "Health");
			st.whitelisted = IsWhitelisted(id) || IsWhitelisted(plainId);
			m_States.Insert(st);
		}
		else
		{
			st.plainId = plainId;
			st.name = name;
			st.whitelisted = IsWhitelisted(id) || IsWhitelisted(plainId);
			st.punished = false;
			st.wasDead = false;
			st.speedStrikes = 0;
			st.graceUntil = now + AC_Config.STARTUP_GRACE_MS;
			st.lastSeen = now;
			st.lastPos = player.GetPosition();
			st.lastSafePos = st.lastPos;
			st.lastCheckTime = now;
			st.lastHealth = player.GetHealth("", "Health");
		}

		if (AC_Config.LOG_CONNECTS)
			AC_Log.Info("CONNECT | name=\"" + AC_Util.Sanitize(name) + "\" id=" + id + " plain=" + plainId + " whitelisted=" + AC_Util.BoolStr(st.whitelisted));
	}

	void OnClientDisconnected(PlayerIdentity identity, PlayerBase player)
	{
		if (!m_Running)
			return;

		string id = "";

		if (identity)
			id = identity.GetId();

		if (id == "" && player && player.GetIdentity())
			id = player.GetIdentity().GetId();

		if (id == "")
			return;

		AC_PlayerState st = FindStateById(id);

		if (!st)
			return;

		st.lastSeen = GetGame().GetTime();

		if (AC_Config.LOG_CONNECTS)
			AC_Log.Info("DISCONNECT | name=\"" + AC_Util.Sanitize(st.name) + "\" id=" + st.id);

		// Состояние не удаляем сразу: очки должны пережить переподключение
		// на STATE_TTL_MS. Tick() уберёт его позже.
	}

	void OnRespawn(PlayerIdentity identity, PlayerBase player)
	{
		if (!m_Running || !player)
			return;

		AC_PlayerState st = EnsureState(player);

		if (!st)
			return;

		ResetBaseline(player, st, GetGame().GetTime() + AC_Config.RESPAWN_GRACE_MS);
	}

	// Вызывается из modded PlayerBase.EEHitBy.
	void OnHit(PlayerBase victim, EntityAI source, string ammo, string dmgZone, TotalDamageResult damageResult)
	{
		if (!m_Running || !AC_Config.CHECK_COMBAT || !victim || !source)
			return;

		if (!victim.IsAlive())
			return;

		PlayerBase attacker = PlayerBase.Cast(source);

		if (!attacker || attacker == victim)
			return;

		AC_PlayerState st = EnsureState(attacker);

		if (!st || st.whitelisted)
			return;

		int now = GetGame().GetTime();
		float dist = AC_Util.Dist3D(attacker.GetPosition(), victim.GetPosition());
		float dmg = 0;

		if (damageResult)
			dmg = damageResult.GetDamage(dmgZone, "Health");

		// Enforce не разрешает повторно объявлять переменную в одной функции
		// (даже в разных блоках), поэтому объявляем её один раз здесь.
		string details;

		if (AC_Config.CHECK_DEAD_ATTACKER)
		{
			if (!attacker.IsAlive())
			{
				details = "victim=\"" + SafeName(victim) + "\" ammo=" + ammo + " dmg=" + AC_Util.Num(dmg, 1) + " dist=" + AC_Util.Num(dist, 1);
				Report("dead_attacker", attacker, st, details, AC_Config.POINTS_DEAD_ATTACKER, attacker.GetPosition());
			}
			else if (attacker.IsUnconscious())
			{
				details = "victim=\"" + SafeName(victim) + "\" ammo=" + ammo + " dmg=" + AC_Util.Num(dmg, 1) + " dist=" + AC_Util.Num(dist, 1);
				Report("unconscious_attacker", attacker, st, details, AC_Config.POINTS_DEAD_ATTACKER, attacker.GetPosition());
			}
		}

		bool bullet = ammo.IndexOf("Bullet") >= 0;

		if (bullet)
		{
			if (dist > AC_Config.MAX_FIREARM_HIT_DISTANCE)
			{
				details = "victim=\"" + SafeName(victim) + "\" ammo=" + ammo + " dist=" + AC_Util.Num(dist, 1) + " limit=" + AC_Util.Num(AC_Config.MAX_FIREARM_HIT_DISTANCE, 1);
				Report("hit_distance", attacker, st, details, AC_Config.POINTS_HIT_DISTANCE, attacker.GetPosition());
			}

			if (AC_Config.CHECK_WEAPON_IN_HANDS)
			{
				ItemBase hands = attacker.GetItemInHands();

				if (!hands || !hands.IsWeapon())
				{
					details = "victim=\"" + SafeName(victim) + "\" ammo=" + ammo + " hands=" + ItemName(hands);
					Report("no_weapon", attacker, st, details, AC_Config.POINTS_NO_WEAPON, attacker.GetPosition());
				}
			}

			if (AC_Config.CHECK_LINE_OF_SIGHT && dist > 3.0)
			{
				if (IsWallBetween(attacker, victim))
				{
					details = "victim=\"" + SafeName(victim) + "\" ammo=" + ammo + " dist=" + AC_Util.Num(dist, 1);
					Report("wallbang", attacker, st, details, AC_Config.POINTS_WALLBANG, attacker.GetPosition());
				}
			}
		}
		else if (ammo == "")
		{
			if (dist > AC_Config.MAX_MELEE_HIT_DISTANCE)
			{
				details = "victim=\"" + SafeName(victim) + "\" melee dist=" + AC_Util.Num(dist, 1) + " limit=" + AC_Util.Num(AC_Config.MAX_MELEE_HIT_DISTANCE, 1);
				Report("hit_distance", attacker, st, details, AC_Config.POINTS_HIT_DISTANCE, attacker.GetPosition());
			}
		}

		AC_PlayerState.PruneTimes(st.hitTimes, now, AC_Config.HIT_WINDOW_MS);
		st.hitTimes.Insert(now);

		if (st.hitTimes.Count() > AC_Config.MAX_HITS_PER_WINDOW)
		{
			details = "hits=" + st.hitTimes.Count() + " in " + (AC_Config.HIT_WINDOW_MS / 1000) + "s";
			Report("hit_rate", attacker, st, details, AC_Config.POINTS_HIT_RATE, attacker.GetPosition());
		}
	}

	// Вызывается из modded PlayerBase.EEKilled.
	void OnKilled(PlayerBase victim, Object killer)
	{
		if (!m_Running || !AC_Config.CHECK_COMBAT || !victim)
			return;

		AC_PlayerState victimState = EnsureState(victim);

		if (victimState)
		{
			victimState.wasDead = true;
			UpdateBaseline(victim, victimState, GetGame().GetTime());
		}

		PlayerBase attacker = PlayerBase.Cast(killer);

		if (!attacker || attacker == victim)
			return;

		AC_PlayerState st = EnsureState(attacker);

		if (!st || st.whitelisted)
			return;

		int now = GetGame().GetTime();
		float dist = AC_Util.Dist3D(attacker.GetPosition(), victim.GetPosition());

		string details;

		if (dist > AC_Config.MAX_KILL_DISTANCE)
		{
			details = "victim=\"" + SafeName(victim) + "\" dist=" + AC_Util.Num(dist, 1) + " limit=" + AC_Util.Num(AC_Config.MAX_KILL_DISTANCE, 1);
			Report("kill_distance", attacker, st, details, AC_Config.POINTS_KILL_DISTANCE, attacker.GetPosition());
		}

		AC_PlayerState.PruneTimes(st.killTimes, now, AC_Config.KILL_WINDOW_MS);
		st.killTimes.Insert(now);

		if (st.killTimes.Count() > AC_Config.MAX_KILLS_PER_WINDOW)
		{
			details = "kills=" + st.killTimes.Count() + " in " + (AC_Config.KILL_WINDOW_MS / 1000) + "s";
			Report("kill_rate", attacker, st, details, AC_Config.POINTS_KILL_RATE, attacker.GetPosition());
		}
	}

	// ---------------------------------------------------------------- checks ---

	protected void Tick()
	{
		if (!m_Running)
			return;

		int now = GetGame().GetTime();

		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		// Убираем состояния игроков, которых давно нет на сервере.
		// Состояние живёт STATE_TTL_MS после последнего онлайна, чтобы очки
		// переживали переподключение и ENFORCEMENT = 2 мог дойти до бана.
		for (int i = m_States.Count() - 1; i >= 0; i--)
		{
			AC_PlayerState state = m_States[i];

			if (IsOnline(state, players))
			{
				state.lastSeen = now;
				continue;
			}

			if ((now - state.lastSeen) > AC_Config.STATE_TTL_MS)
				m_States.Remove(i);
		}

		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);

			if (!player)
				continue;

			AC_PlayerState st = EnsureState(player);

			if (!st)
				continue;

			CheckPlayer(player, st, now);
		}
	}

	protected void CheckPlayer(PlayerBase player, AC_PlayerState st, int now)
	{
		st.lastSeen = now;

		if (st.whitelisted)
		{
			UpdateBaseline(player, st, now);
			return;
		}

		if (!player.IsAlive())
		{
			st.wasDead = true;
			UpdateBaseline(player, st, now);
			return;
		}

		if (st.wasDead)
		{
			st.wasDead = false;
			st.graceUntil = now + AC_Config.RESPAWN_GRACE_MS;
			UpdateBaseline(player, st, now);
			return;
		}

		if (now < st.graceUntil)
		{
			UpdateBaseline(player, st, now);
			return;
		}

		if (IsStateExempt(player))
		{
			UpdateBaseline(player, st, now);
			return;
		}

		if (!AC_Config.CHECK_MOVEMENT)
		{
			UpdateBaseline(player, st, now);
			return;
		}

		float dt = (now - st.lastCheckTime) / 1000.0;

		if (dt < 0.25)
			return;

		vector pos = player.GetPosition();
		vector prev = st.lastPos;

		float dist = AC_Util.Dist3D(pos, prev);
		float speed = dist / dt;

		string details;

		if (dist > AC_Config.TELEPORT_DISTANCE && speed > AC_Config.TELEPORT_SPEED)
		{
			details = "dist=" + AC_Util.Num(dist, 1) + "m speed=" + AC_Util.Num(speed, 1) + "m/s dt=" + AC_Util.Num(dt, 2) + "s from=" + AC_Util.Vec(prev) + " to=" + AC_Util.Vec(pos);
			Report("teleport", player, st, details, AC_Config.POINTS_TELEPORT, pos);

			if (AC_Config.REVERT_TELEPORT && IsSafePos(st.lastSafePos))
			{
				player.SetPosition(st.lastSafePos);
				pos = st.lastSafePos;
			}

			st.speedStrikes = 0;
			UpdateBaseline(player, st, now);
			return;
		}

		float hspeed = AC_Util.Dist2D(pos, prev) / dt;

		if (hspeed > AC_Config.MAX_HORIZONTAL_SPEED)
		{
			st.speedStrikes++;

			if (st.speedStrikes >= AC_Config.SPEED_STRIKES)
			{
				details = "speed=" + AC_Util.Num(hspeed, 1) + "m/s limit=" + AC_Util.Num(AC_Config.MAX_HORIZONTAL_SPEED, 1) + " dt=" + AC_Util.Num(dt, 2) + "s";
				Report("speed", player, st, details, AC_Config.POINTS_SPEED, pos);
				st.speedStrikes = 0;
			}
		}
		else
		{
			st.speedStrikes = 0;
		}

		float vspeed = (pos[1] - prev[1]) / dt;

		if (vspeed > AC_Config.MAX_UP_SPEED)
		{
			details = "up=" + AC_Util.Num(vspeed, 1) + "m/s limit=" + AC_Util.Num(AC_Config.MAX_UP_SPEED, 1) + " dt=" + AC_Util.Num(dt, 2) + "s";
			Report("fly", player, st, details, AC_Config.POINTS_FLY, pos);
		}

		if (AC_Config.CHECK_UNDERGROUND)
		{
			float ground = GetGame().SurfaceY(pos[0], pos[2]);

			if (!GetGame().SurfaceIsSea(pos[0], pos[2]) && pos[1] < ground - AC_Config.MAX_UNDERGROUND_DEPTH)
			{
				details = "y=" + AC_Util.Num(pos[1], 1) + " ground=" + AC_Util.Num(ground, 1) + " depth=" + AC_Util.Num(ground - pos[1], 1) + "m";
				Report("underground", player, st, details, AC_Config.POINTS_UNDERGROUND, pos);
			}
		}

		// Запоминаем безопасную позицию только при нормальном движении.
		if (hspeed <= AC_Config.MAX_HORIZONTAL_SPEED && vspeed <= AC_Config.MAX_UP_SPEED && IsSafePos(pos))
			st.lastSafePos = pos;

		UpdateBaseline(player, st, now);
	}

	protected bool IsStateExempt(PlayerBase player)
	{
		if (player.IsInVehicle())
			return true;

		if (player.IsSwimming())
			return true;

		if (player.IsClimbing())
			return true;

		if (player.IsClimbingLadder())
			return true;

		if (player.IsFalling())
			return true;

		if (player.IsUnconscious())
			return true;

		if (player.IsRestrained())
			return true;

		return false;
	}

	protected bool IsSafePos(vector pos)
	{
		if (pos[0] == 0 && pos[1] == 0 && pos[2] == 0)
			return false;

		if (GetGame().SurfaceIsSea(pos[0], pos[2]))
			return false;

		float ground = GetGame().SurfaceY(pos[0], pos[2]);

		if (pos[1] < ground - AC_Config.MAX_UNDERGROUND_DEPTH)
			return false;

		return true;
	}

	protected void UpdateBaseline(PlayerBase player, AC_PlayerState st, int now)
	{
		st.lastPos = player.GetPosition();
		st.lastCheckTime = now;
		st.lastHealth = player.GetHealth("", "Health");
	}

	protected void ResetBaseline(PlayerBase player, AC_PlayerState st, int graceUntil)
	{
		int now = GetGame().GetTime();
		st.graceUntil = graceUntil;
		st.lastPos = player.GetPosition();
		st.lastSafePos = st.lastPos;
		st.lastCheckTime = now;
		st.lastHealth = player.GetHealth("", "Health");
		st.speedStrikes = 0;
		st.wasDead = false;
	}

	// --------------------------------------------------------------- report ---

	protected void Report(string type, PlayerBase player, AC_PlayerState st, string details, int points, vector pos)
	{
		if (!m_Running || !st || st.whitelisted)
			return;

		int now = GetGame().GetTime();

		if (st.IsOnCooldown(type, now, AC_Config.VIOLATION_COOLDOWN_MS))
			return;

		st.MarkViolation(type, now);

		if (now - st.pointsWindowStart > AC_Config.POINT_WINDOW_MS)
		{
			st.points = 0;
			st.pointsWindowStart = now;
		}

		st.points += points;
		st.reports++;

		string action = "log";

		if (AC_Config.ENFORCEMENT >= 1 && st.points >= AC_Config.KICK_POINTS)
			action = "kick";

		if (AC_Config.ENFORCEMENT >= 2 && st.points >= AC_Config.BAN_POINTS)
			action = "ban";

		AC_Violation v = new AC_Violation();
		v.type = type;
		v.id = st.id;
		v.plainId = st.plainId;
		v.name = st.name;
		v.details = details;
		v.pos = pos;
		v.points = points;
		v.totalPoints = st.points;
		v.action = action;

		AC_Log.Write(v);
		m_Reports++;

		if (action == "ban")
		{
			BanPlayer(player, st, type + ": " + details);
			return;
		}

		if (action == "kick")
		{
			KickPlayer(player, st, type + ": " + details);
			return;
		}
	}

	// ------------------------------------------------------------ enforcement ---

	protected void KickPlayer(PlayerBase player, AC_PlayerState st, string reason)
	{
		if (!player || !st || st.punished)
			return;

		PlayerIdentity identity = player.GetIdentity();

		if (!identity)
			return;

		st.punished = true;
		m_Kicks++;

		AC_Log.Info("KICK | name=\"" + AC_Util.Sanitize(st.name) + "\" id=" + st.id + " plain=" + st.plainId + " points=" + st.points + " reason=" + AC_Util.Sanitize(reason));
		GetGame().DisconnectPlayer(identity);
	}

	protected void BanPlayer(PlayerBase player, AC_PlayerState st, string reason)
	{
		string banId = st.id;

		if (AC_Config.BAN_BY_PLAIN_ID && st.plainId != "")
			banId = st.plainId;

		if (banId != "")
		{
			AddBan(banId);
			m_BansIssued++;
			AC_Log.Info("BAN | name=\"" + AC_Util.Sanitize(st.name) + "\" id=" + st.id + " plain=" + st.plainId + " banId=" + banId + " points=" + st.points + " reason=" + AC_Util.Sanitize(reason));
		}

		KickPlayer(player, st, reason);
	}

	protected void AddBan(string id)
	{
		if (id == "" || m_Bans.Find(id) >= 0)
			return;

		m_Bans.Insert(id);

		FileHandle fh = OpenFile(AC_Config.BAN_FILE, FileMode.APPEND);

		if (!fh)
			return;

		FPrintln(fh, id);
		CloseFile(fh);
	}

	protected bool RemoveBan(string id)
	{
		if (m_Bans.Find(id) < 0)
			return false;

		m_Bans.RemoveItem(id);

		FileHandle fh = OpenFile(AC_Config.BAN_FILE, FileMode.WRITE);

		if (fh)
		{
			foreach (string banId : m_Bans)
				FPrintln(fh, banId);

			CloseFile(fh);
		}

		return true;
	}

	// --------------------------------------------------------------- lists ---

	void ReloadLists()
	{
		if (!m_Bans)
			m_Bans = new array<string>();

		if (!m_Whitelist)
			m_Whitelist = new array<string>();

		LoadList(AC_Config.BAN_FILE, m_Bans);
		LoadList(AC_Config.WHITELIST_FILE, m_Whitelist);

		if (m_States)
		{
			foreach (AC_PlayerState st : m_States)
				st.whitelisted = IsWhitelisted(st.id) || IsWhitelisted(st.plainId);
		}

		m_LastReload = GetGame().GetTime();
	}

	protected void LoadList(string path, array<string> list)
	{
		list.Clear();

		if (!FileExist(path))
			return;

		FileHandle fh = OpenFile(path, FileMode.READ);

		if (!fh)
			return;

		string line;

		while (FGets(fh, line) > 0)
		{
			line = AC_Util.Trim(line);

			if (line == "" || line.Substring(0, 1) == "#")
				continue;

			list.Insert(line);
		}

		CloseFile(fh);
	}

	protected bool IsBanned(string id)
	{
		if (id == "" || !m_Bans)
			return false;

		return m_Bans.Find(id) >= 0;
	}

	protected bool IsWhitelisted(string id)
	{
		if (id == "" || !m_Whitelist)
			return false;

		return m_Whitelist.Find(id) >= 0;
	}

	// ------------------------------------------------------- admin commands ---

	// Команды читаются из $profile:DayZPulseAntiCheat/commands.txt и файл очищается.
	// Формат: одна команда на строку.
	void ProcessCommands()
	{
		if (!m_Running || !FileExist(AC_Config.COMMAND_FILE))
			return;

		FileHandle fh = OpenFile(AC_Config.COMMAND_FILE, FileMode.READ);

		if (!fh)
			return;

		array<string> lines = new array<string>();
		string line;

		while (FGets(fh, line) > 0)
		{
			line = AC_Util.Trim(line);

			if (line != "" && line.Substring(0, 1) != "#")
				lines.Insert(line);
		}

		CloseFile(fh);

		FileHandle clear = OpenFile(AC_Config.COMMAND_FILE, FileMode.WRITE);

		if (clear)
			CloseFile(clear);

		foreach (string cmdLine : lines)
			ExecuteCommand(cmdLine);
	}

	protected void ExecuteCommand(string line)
	{
		array<string> raw = AC_Util.Split(line, " ");
		array<string> args = new array<string>();

		foreach (string part : raw)
		{
			string trimmed = AC_Util.Trim(part);

			if (trimmed != "")
				args.Insert(trimmed);
		}

		if (args.Count() == 0)
			return;

		string cmd = args[0];

		if (cmd == "status")
		{
			AC_Log.Info("STATUS | " + StatusLine());
			return;
		}

		if (cmd == "reload")
		{
			ReloadLists();
			AC_Log.Info("Lists reloaded | bans=" + m_Bans.Count() + " whitelist=" + m_Whitelist.Count());
			return;
		}

		if (cmd == "help")
		{
			AC_Log.Info("Commands: kick <id> [reason] | ban <id> [reason] | unban <id> | exempt <id> <seconds> | status | reload");
			return;
		}

		if (args.Count() < 2)
		{
			AC_Log.Info("Admin: missing id for '" + cmd + "'");
			return;
		}

		string id = args[1];
		string reason = "admin";

		// Объявляем один раз: Enforce не разрешает повторные объявления в функции.
		PlayerBase player;
		AC_PlayerState st;

		if (args.Count() > 2)
		{
			reason = "";

			for (int i = 2; i < args.Count(); i++)
			{
				if (reason != "")
					reason += " ";

				reason += args[i];
			}
		}

		if (cmd == "kick")
		{
			player = FindPlayer(id);

			if (!player)
			{
				AC_Log.Info("Admin: player not found for kick: " + id);
				return;
			}

			st = EnsureState(player);

			if (st)
				KickPlayer(player, st, reason);

			return;
		}

		if (cmd == "ban")
		{
			player = FindPlayer(id);
			string banId = id;

			if (player)
			{
				PlayerIdentity identity = player.GetIdentity();

				if (identity)
				{
					if (AC_Config.BAN_BY_PLAIN_ID && identity.GetPlainId() != "")
						banId = identity.GetPlainId();
					else
						banId = identity.GetId();
				}
			}

			AddBan(banId);
			m_BansIssued++;
			AC_Log.Info("Admin: ban " + banId + " reason=" + AC_Util.Sanitize(reason));

			if (player)
			{
				st = EnsureState(player);

				if (st)
					KickPlayer(player, st, reason);
			}

			return;
		}

		if (cmd == "unban")
		{
			if (RemoveBan(id))
				AC_Log.Info("Admin: unban " + id);
			else
				AC_Log.Info("Admin: not in ban list: " + id);

			return;
		}

		if (cmd == "exempt")
		{
			int seconds = 300;

			if (args.Count() > 2)
				seconds = args[2].ToInt();

			if (seconds < 1)
				seconds = 1;

			player = FindPlayer(id);

			if (!player)
			{
				AC_Log.Info("Admin: player not found for exempt: " + id);
				return;
			}

			st = EnsureState(player);

			if (st)
			{
				st.graceUntil = GetGame().GetTime() + (seconds * 1000);
				AC_Log.Info("Admin: exempt " + id + " for " + seconds + "s");
			}

			return;
		}

		AC_Log.Info("Admin: unknown command '" + cmd + "'");
	}

	// ---------------------------------------------------------------- lookup ---

	protected AC_PlayerState EnsureState(PlayerBase player)
	{
		if (!player)
			return null;

		PlayerIdentity identity = player.GetIdentity();

		if (!identity)
			return null;

		string id = identity.GetId();

		if (id == "")
			return null;

		int now = GetGame().GetTime();
		AC_PlayerState st = FindStateById(id);

		if (st)
		{
			st.name = identity.GetName();
			st.lastSeen = now;
			return st;
		}

		st = new AC_PlayerState();
		st.id = id;
		st.plainId = identity.GetPlainId();
		st.name = identity.GetName();
		st.connectTime = now;
		st.graceUntil = now + AC_Config.STARTUP_GRACE_MS;
		st.lastSeen = now;
		st.lastPos = player.GetPosition();
		st.lastSafePos = st.lastPos;
		st.lastCheckTime = now;
		st.lastHealth = player.GetHealth("", "Health");
		st.whitelisted = IsWhitelisted(st.id) || IsWhitelisted(st.plainId);
		m_States.Insert(st);

		return st;
	}

	protected AC_PlayerState FindStateById(string id)
	{
		if (!m_States || id == "")
			return null;

		foreach (AC_PlayerState st : m_States)
		{
			if (st.id == id)
				return st;
		}

		return null;
	}

	protected bool IsOnline(AC_PlayerState st, array<Man> players)
	{
		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);

			if (!player)
				continue;

			PlayerIdentity identity = player.GetIdentity();

			if (!identity)
				continue;

			if (identity.GetId() == st.id)
				return true;
		}

		return false;
	}

	protected PlayerBase FindPlayer(string id)
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

			if (identity.GetId() == id || identity.GetPlainId() == id)
				return player;
		}

		return null;
	}

	protected string SafeName(PlayerBase player)
	{
		if (!player)
			return "?";

		PlayerIdentity identity = player.GetIdentity();

		if (!identity)
			return "?";

		return AC_Util.Sanitize(identity.GetName());
	}

	protected string ItemName(ItemBase item)
	{
		if (!item)
			return "none";

		return item.GetType();
	}

	// ------------------------------------------------------------- raycast ---

	protected bool IsWallBetween(PlayerBase attacker, PlayerBase victim)
	{
		vector from = attacker.GetPosition();
		from[1] = from[1] + 1.5;

		vector to = victim.GetPosition();
		to[1] = to[1] + 1.0;

		RaycastRVParams rvParams = new RaycastRVParams(from, to, null, 0.0);
		rvParams.flags = CollisionFlags.NEARESTCONTACT;
		rvParams.type = ObjIntersectView;

		array<ref RaycastRVResult> results = new array<ref RaycastRVResult>();
		array<Object> excluded = new array<Object>();
		excluded.Insert(attacker);
		excluded.Insert(victim);

		DayZPhysics.RaycastRVProxy(rvParams, results, excluded);

		return results.Count() > 0;
	}

	// ------------------------------------------------------------- public API ---

	// Другие серверные моды могут временно снять проверки с игрока,
	// например после административного телепорта.
	void Exempt(PlayerBase player, int durationMs)
	{
		if (!m_Running || !player || durationMs <= 0)
			return;

		AC_PlayerState st = EnsureState(player);

		if (!st)
			return;

		st.graceUntil = GetGame().GetTime() + durationMs;
		AC_Log.Info("Exempt | name=\"" + AC_Util.Sanitize(st.name) + "\" id=" + st.id + " for " + durationMs + "ms");
	}

	void ExemptId(string id, int durationMs)
	{
		PlayerBase player = FindPlayer(id);

		if (player)
			Exempt(player, durationMs);
	}

	string StatusLine()
	{
		int states = 0;
		int bans = 0;
		int whitelist = 0;

		if (m_States)
			states = m_States.Count();

		if (m_Bans)
			bans = m_Bans.Count();

		if (m_Whitelist)
			whitelist = m_Whitelist.Count();

		return "running=" + AC_Util.BoolStr(m_Running) + " players=" + states + " reports=" + m_Reports + " kicks=" + m_Kicks + " bansIssued=" + m_BansIssued + " bans=" + bans + " whitelist=" + whitelist + " httpQueue=" + AC_Log.PendingHttp();
	}
}
