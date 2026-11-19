// ============================================================================
//  DayZPulseCore - счётчик FPS сервера.
//
//  GetGame().GetAvgFPS() и GetFps() считают кадры РЕНДЕРА. На выделенном
//  сервере рендера нет, поэтому эти функции возвращают мусор (постоянное
//  число вроде 14.3). Настоящий «серверный FPS» — это сколько раз в секунду
//  движок вызывает MissionServer.OnUpdate (тик мира).
//
//  Считаем сами: копим время и число кадров, раз в ~1 секунду получаем
//  frames / elapsed. Так же считает движок (logAverageFps) и чужой мод MetricZ.
// ============================================================================

class PT_FrameMonitor
{
	protected static float s_AccTime;
	protected static int   s_AccFrames;
	protected static float s_Fps;

	// Вызывается из MissionServer.OnUpdate на каждом кадре сервера.
	static void OnUpdate(float timeslice)
	{
		if (timeslice <= 0)
			return;

		s_AccTime += timeslice;
		s_AccFrames++;

		if (s_AccTime < 1.0)
			return;

		s_Fps = s_AccFrames / s_AccTime;

		// Оставляем дробный остаток, чтобы окно не «плыло».
		s_AccTime = s_AccTime - 1.0;
		s_AccFrames = 0;

		if (s_AccTime > 1.0)
			s_AccTime = 0;
	}

	// Последнее измеренное значение (кадров в секунду). 0 — ещё не измерили.
	static float Get()
	{
		return s_Fps;
	}

	// Среднее время кадра в миллисекундах (0, если измерений ещё не было).
	static float FrameMs()
	{
		if (s_Fps <= 0)
			return 0;

		return 1000.0 / s_Fps;
	}
}
