// ============================================================================
//  PlayerTracker - команда и разбор ответа API.
//
//  Формат ответа GET /commands (text/plain, одна команда на строку):
//      <id>|<action>|<key>=<value>|<key>=<value>...
//
//  Пример:
//      42|spawn|class=AKM|x=7500.25|y=0|z=7412.75|qty=1
//      43|weather|overcast=0.9|rain=0.7|transition=30
//
//  Пустые строки и строки, начинающиеся с '#', игнорируются.
// ============================================================================

class PT_Command
{
	int    id;
	string action;

	ref array<string> keys;
	ref array<string> values;

	void PT_Command()
	{
		id = 0;
		action = "";
		keys = new array<string>();
		values = new array<string>();
	}

	bool Has(string key)
	{
		for (int i = 0; i < keys.Count(); i++)
		{
			if (keys[i] == key)
				return true;
		}

		return false;
	}

	string GetStr(string key, string def)
	{
		for (int i = 0; i < keys.Count(); i++)
		{
			if (keys[i] == key)
				return values[i];
		}

		return def;
	}

	int GetInt(string key, int def)
	{
		string s = GetStr(key, "");

		if (s == "")
			return def;

		return s.ToInt();
	}

	float GetFloat(string key, float def)
	{
		string s = GetStr(key, "");

		if (s == "")
			return def;

		return s.ToFloat();
	}

	bool GetBool(string key, bool def)
	{
		string s = GetStr(key, "");

		if (s == "")
			return def;

		if (s == "1" || s == "true" || s == "yes")
			return true;

		if (s == "0" || s == "false" || s == "no")
			return false;

		return def;
	}

	vector GetPos()
	{
		return Vector(GetFloat("x", 0), GetFloat("y", 0), GetFloat("z", 0));
	}
}

class PT_CommandParser
{
	static array<ref PT_Command> Parse(string body)
	{
		array<ref PT_Command> commands = new array<ref PT_Command>();
		array<string> lines = PT_Util.Split(body, "\n");

		foreach (string rawLine : lines)
		{
			string line = PT_Util.TrimStr(rawLine);

			if (line == "")
				continue;

			if (line.Substring(0, 1) == "#")
				continue;

			array<string> parts = PT_Util.Split(line, "|");

			if (parts.Count() < 2)
				continue;

			PT_Command cmd = new PT_Command();
			cmd.id = parts[0].ToInt();
			cmd.action = PT_Util.TrimStr(parts[1]);

			for (int i = 2; i < parts.Count(); i++)
			{
				string pair = parts[i];
				int eq = pair.IndexOf("=");

				if (eq <= 0)
					continue;

				cmd.keys.Insert(PT_Util.TrimStr(pair.Substring(0, eq)));
				cmd.values.Insert(PT_Util.TrimStr(pair.Substring(eq + 1, pair.Length() - eq - 1)));
			}

			commands.Insert(cmd);
		}

		return commands;
	}
}
