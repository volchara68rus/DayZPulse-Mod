// ============================================================================
//  AntiCheat - нарушение и журнал.
//
//  Пишем в:
//    - консоль сервера (Print);
//    - $profile:DayZPulseAntiCheat/violations.log (текст, по строке на нарушение);
//    - опционально в HTTP-очередь для API.
// ============================================================================

class AC_Violation
{
	int    seq;
	int    time;
	string type;
	string id;
	string plainId;
	string name;
	string details;
	vector pos;
	int    points;
	int    totalPoints;
	string action;

	string ToJson()
	{
		string json = "{";
		json += "\"seq\":" + seq + ",";
		json += "\"ts\":" + time + ",";
		json += "\"type\":\"" + AC_Util.Escape(type) + "\",";
		json += "\"id\":\"" + AC_Util.Escape(id) + "\",";
		json += "\"plainId\":\"" + AC_Util.Escape(plainId) + "\",";
		json += "\"name\":\"" + AC_Util.Escape(name) + "\",";
		json += "\"pos\":{\"x\":" + AC_Util.Num(pos[0], 2) + ",\"y\":" + AC_Util.Num(pos[1], 2) + ",\"z\":" + AC_Util.Num(pos[2], 2) + "},";
		json += "\"points\":" + points + ",";
		json += "\"totalPoints\":" + totalPoints + ",";
		json += "\"action\":\"" + AC_Util.Escape(action) + "\",";
		json += "\"details\":\"" + AC_Util.Escape(details) + "\"";
		json += "}";

		return json;
	}
}

class AC_Log
{
	protected static int s_Seq;
	protected static ref array<string> s_HttpQueue;

	static void Init()
	{
		MakeDirectory(AC_Config.DIR);
	}

	static void Write(AC_Violation v)
	{
		if (!v)
			return;

		s_Seq++;
		v.seq = s_Seq;

		if (v.time <= 0)
			v.time = GetGame().GetTime();

		string line = AC_Util.Timestamp();
		line += " | " + v.action;
		line += " | " + v.type;
		line += " | id=" + v.id;
		line += " | plain=" + v.plainId;
		line += " | name=\"" + AC_Util.Sanitize(v.name) + "\"";
		line += " | pos=" + AC_Util.Vec(v.pos);
		line += " | +" + v.points + " pts (total " + v.totalPoints + ")";
		line += " | " + AC_Util.Sanitize(v.details);

		if (AC_Config.LOG_TO_CONSOLE)
			Print("[DayZPulseAntiCheat] " + line);

		if (AC_Config.LOG_TO_FILE)
			AppendFile(AC_Config.LOG_FILE, line);

		if (AC_Config.ENABLE_HTTP)
			EnqueueHttp(v.ToJson());
	}

	// Служебные сообщения: старт, кик, бан, команды администратора.
	static void Info(string message)
	{
		string line = AC_Util.Timestamp() + " | INFO | " + AC_Util.Sanitize(message);

		if (AC_Config.LOG_TO_CONSOLE)
			Print("[DayZPulseAntiCheat] " + line);

		if (AC_Config.LOG_TO_FILE)
			AppendFile(AC_Config.ADMIN_LOG, line);
	}

	static void AppendFile(string path, string line)
	{
		FileHandle fh = OpenFile(path, FileMode.APPEND);

		if (!fh)
			return;

		FPrintln(fh, line);
		CloseFile(fh);
	}

	// ------------------------------------------------------------ HTTP queue ---

	static void EnqueueHttp(string json)
	{
		if (!s_HttpQueue)
			s_HttpQueue = new array<string>();

		if (s_HttpQueue.Count() >= AC_Config.HTTP_QUEUE_LIMIT)
			s_HttpQueue.Remove(0);

		s_HttpQueue.Insert(json);
	}

	static array<string> TakeHttpBatch(int max)
	{
		array<string> batch = new array<string>();

		if (!s_HttpQueue)
			return batch;

		while (s_HttpQueue.Count() > 0 && batch.Count() < max)
		{
			batch.Insert(s_HttpQueue[0]);
			s_HttpQueue.Remove(0);
		}

		return batch;
	}

	static void RequeueHttp(array<string> batch)
	{
		if (!s_HttpQueue)
			s_HttpQueue = new array<string>();

		foreach (string item : batch)
		{
			if (s_HttpQueue.Count() >= AC_Config.HTTP_QUEUE_LIMIT)
				s_HttpQueue.Remove(0);

			s_HttpQueue.Insert(item);
		}
	}

	static int PendingHttp()
	{
		if (!s_HttpQueue)
			return 0;

		return s_HttpQueue.Count();
	}
}
