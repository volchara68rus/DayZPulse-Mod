// ============================================================================
//  AntiCheat - утилиты: строки, числа, векторы, время.
// ============================================================================

class AC_Util
{
	// Экранирование строки под JSON.
	static string Escape(string input)
	{
		string result = "";
		int len = input.Length();

		for (int i = 0; i < len; i++)
		{
			string ch = input.Substring(i, 1);

			if (ch == "\"")
				result += "\\\"";
			else if (ch == "\\")
				result += "\\\\";
			else if (ch == "\n")
				result += "\\n";
			else if (ch == "\r")
				result += "\\r";
			else if (ch == "\t")
				result += "\\t";
			else
				result += ch;
		}

		return result;
	}

	// Форматирование float с фиксированным числом знаков.
	static string Num(float value, int decimals)
	{
		int factor = 1;
		for (int i = 0; i < decimals; i++)
			factor = factor * 10;

		bool negative = value < 0;
		float absValue = Math.AbsFloat(value);
		int scaled = Math.Round(absValue * factor);
		int whole = scaled / factor;
		int frac = scaled - (whole * factor);

		string result = "" + whole;

		if (decimals > 0)
		{
			string fracStr = "" + frac;
			while (fracStr.Length() < decimals)
				fracStr = "0" + fracStr;

			result = result + "." + fracStr;
		}

		if (negative && (whole != 0 || frac != 0))
			result = "-" + result;

		return result;
	}

	static string BoolStr(bool value)
	{
		if (value)
			return "true";

		return "false";
	}

	// Обрезка пробельных символов с обоих концов.
	static string Trim(string input)
	{
		int start = 0;
		int end = input.Length();

		while (start < end)
		{
			string ch = input.Substring(start, 1);
			if (ch == " " || ch == "\t" || ch == "\r" || ch == "\n")
				start++;
			else
				break;
		}

		while (end > start)
		{
			string chEnd = input.Substring(end - 1, 1);
			if (chEnd == " " || chEnd == "\t" || chEnd == "\r" || chEnd == "\n")
				end--;
			else
				break;
		}

		return input.Substring(start, end - start);
	}

	// Замена символов, ломающих текстовый лог.
	static string Sanitize(string input)
	{
		string result = "";
		int len = input.Length();

		for (int i = 0; i < len; i++)
		{
			string ch = input.Substring(i, 1);

			if (ch == "|" || ch == "\r" || ch == "\n" || ch == "\t")
				result += " ";
			else if (ch == "\"")
				result += "'";
			else
				result += ch;
		}

		return result;
	}

	static array<string> Split(string input, string separator)
	{
		array<string> parts = new array<string>();
		int sepLen = separator.Length();

		if (sepLen == 0)
		{
			parts.Insert(input);
			return parts;
		}

		string rest = input;

		while (true)
		{
			int idx = rest.IndexOf(separator);

			if (idx < 0)
			{
				parts.Insert(rest);
				break;
			}

			parts.Insert(rest.Substring(0, idx));
			rest = rest.Substring(idx + sepLen, rest.Length() - idx - sepLen);
		}

		return parts;
	}

	static string Join(array<string> parts, string separator)
	{
		string result = "";

		foreach (string part : parts)
		{
			if (result != "")
				result += separator;

			result += part;
		}

		return result;
	}

	// Расстояние по горизонтали (X/Z).
	static float Dist2D(vector a, vector b)
	{
		vector d = a - b;
		d[1] = 0;
		return d.Length();
	}

	static float Dist3D(vector a, vector b)
	{
		return (a - b).Length();
	}

	static string Vec(vector v)
	{
		return Num(v[0], 1) + " / " + Num(v[1], 1) + " / " + Num(v[2], 1);
	}

	static string Two(int value)
	{
		if (value < 0)
			return "00";

		if (value < 10)
			return "0" + value;

		return "" + value;
	}

	// Локальное игровое время для строки лога.
	static string Timestamp()
	{
		World world = GetGame().GetWorld();

		if (!world)
			return "0000-00-00 00:00:00";

		int year;
		int month;
		int day;
		int hour;
		int minute;

		world.GetDate(year, month, day, hour, minute);

		return "" + year + "-" + Two(month) + "-" + Two(day) + " " + Two(hour) + ":" + Two(minute);
	}
}
