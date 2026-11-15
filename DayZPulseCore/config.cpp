class CfgPatches
{
	class DayZPulseCore
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = { "DZ_Data", "DZ_Scripts" };
	};
};

class CfgMods
{
	class DayZPulseCore
	{
		dir = "DayZPulseCore";
		picture = "";
		action = "";
		hideEnter = 0;
		name = "DayZ Pulse Core";
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
				files[] = { "DayZPulseCore/Scripts/3_Game" };
			};
			class worldScriptModule
			{
				value = "";
				files[] = { "DayZPulseCore/Scripts/4_World" };
			};
			class missionScriptModule
			{
				value = "";
				files[] = { "DayZPulseCore/Scripts/5_Mission" };
			};
		};
	};
};
