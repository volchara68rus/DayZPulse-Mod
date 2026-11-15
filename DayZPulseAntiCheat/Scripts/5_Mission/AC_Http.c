// ============================================================================
//  AntiCheat - необязательная отправка нарушений в API.
//
//  Нарушения копятся в AC_Log и раз в HTTP_INTERVAL_MS уходят одним POST
//  на /ac/ingest. Если API недоступен, батч возвращается в очередь.
//
//  RestContext синхронный: запрос блокирует игровой поток до таймаута.
//  Держите API на localhost и не ставьте интервал меньше 5 секунд.
//
//  Особенности API DayZ 1.29:
//    - SetHeader() задаёт ТОЛЬКО Content-Type, произвольные заголовки
//      (X-Api-Token) отправить нельзя, поэтому токен идёт в query (?token=);
//    - POST_now() возвращает тело ответа строкой и не отдаёт HTTP-код,
//      поэтому успех определяется по содержимому ("ok":true).
// ============================================================================

class AC_Http
{
	protected static ref AC_Http s_Instance;

	// RestApi/RestContext нельзя держать как ref/autoptr-члены: у них
	// приватный деструктор, движок выдаёт "Method '~RestApi' is private".
	// Обычные (не ref) члены работают.
	protected RestApi     m_Api;
	protected RestContext m_Ctx;

	protected bool m_Running;
	protected int  m_Fail;
	protected int  m_Sent;

	static AC_Http Get()
	{
		if (!s_Instance)
			s_Instance = new AC_Http();

		return s_Instance;
	}

	void Start()
	{
		if (m_Running || !AC_Config.ENABLED || !AC_Config.ENABLE_HTTP)
			return;

		if (!GetGame().IsDedicatedServer())
			return;

		m_Api = CreateRestApi();

		if (!m_Api)
		{
			AC_Log.Info("HTTP: CreateRestApi() failed - reporting disabled.");
			return;
		}

		m_Ctx = m_Api.GetRestContext(AuthUrl(AC_Config.HTTP_URL));

		if (!m_Ctx)
		{
			AC_Log.Info("HTTP: GetRestContext() failed for " + AC_Config.HTTP_URL + " - reporting disabled.");
			return;
		}

		m_Ctx.SetHeader("application/json");

		m_Running = true;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Flush, AC_Config.HTTP_INTERVAL_MS, true);

		AC_Log.Info("HTTP reporting enabled: " + AC_Config.HTTP_URL);
	}

	void Stop()
	{
		if (!m_Running)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Flush);

		m_Running = false;

		AC_Log.Info("HTTP stopped | sent=" + m_Sent + " pending=" + AC_Log.PendingHttp());
	}

	void Flush()
	{
		if (!m_Running || !m_Ctx)
			return;

		array<string> batch = AC_Log.TakeHttpBatch(AC_Config.HTTP_MAX_BATCH);

		if (batch.Count() == 0)
			return;

		string payload = "{\"server\":\"" + AC_Util.Escape(AC_Config.SERVER_ID) + "\",\"violations\":[" + AC_Util.Join(batch, ",") + "]}";
		string response = m_Ctx.POST_now("", payload);

		if (IsOk(response))
		{
			m_Sent += batch.Count();
			m_Fail = 0;
			return;
		}

		AC_Log.RequeueHttp(batch);
		m_Fail++;

		if (m_Fail == 1 || (m_Fail % 30) == 0)
			AC_Log.Info("HTTP POST failed (attempts " + m_Fail + ", pending " + AC_Log.PendingHttp() + "): " + response);
	}

	// ------------------------------------------------------------- helpers ---

	// Токен нельзя положить в заголовок (SetHeader задаёт только Content-Type),
	// поэтому добавляем его в query: ?token=... или &token=...
	protected string AuthUrl(string url)
	{
		if (AC_Config.API_TOKEN == "")
			return url;

		string sep = "?";

		if (url.IndexOf("?") >= 0)
			sep = "&";

		return url + sep + "token=" + AC_Config.API_TOKEN;
	}

	// POST_now() не возвращает HTTP-код, поэтому успех - это ответ API
	// вида {"ok":true,...}. Пустая строка и {"error":...} считаются ошибкой.
	protected bool IsOk(string response)
	{
		if (response == "")
			return false;

		if (response.IndexOf("\"error\"") >= 0)
			return false;

		return response.IndexOf("\"ok\":true") >= 0 || response.IndexOf("\"ok\": true") >= 0;
	}
}
