class CfgPatches
{
	class DayZPulseAntiCheat
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = { "DZ_Data", "DZ_Scripts" };
	};
};

class CfgMods
{
	class DayZPulseAntiCheat
	{
		dir = "DayZPulseAntiCheat";
		picture = "";
		action = "";
		hideEnter = 0;
		name = "DayZ Pulse AntiCheat";
		credits = "";
		author = "";
		version = "1.0";
		type = "mod";
		dependencies[] = { "Game", "World", "Mission" };

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = { "DayZPulseAntiCheat/Scripts/3_Game" };
			};
			class worldScriptModule
			{
				value = "";
				files[] = { "DayZPulseAntiCheat/Scripts/4_World" };
			};
			class missionScriptModule
			{
				value = "";
				files[] = { "DayZPulseAntiCheat/Scripts/5_Mission" };
			};
		};
	};
};
