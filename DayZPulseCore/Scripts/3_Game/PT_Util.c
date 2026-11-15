// ============================================================================
//  PlayerTracker - утилиты: экранирование JSON, форматирование чисел,
//  разбор плоского текстового протокола команд.
// ============================================================================

class PT_Util
{
	// Экранирование строки под JSON (кавычки, бэкслеши, переводы строк).
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

	// Форматирование float с фиксированным числом знаков после точки.
	// Своё, чтобы не зависеть от локали и не ловить "1.23456789" в JSON.
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
	static string TrimStr(string input)
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

	// Разбиение строки по разделителю.
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
}
