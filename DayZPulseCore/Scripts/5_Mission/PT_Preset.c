// ============================================================================
//  PlayerTracker - пресеты предметов.
//
//  Файл: $profile:DayZPulseCore/presets/<name>.json
//
//  {
//      "name": "akm_kit",
//      "items": [
//          {
//              "className": "AKM",
//              "quantity": 1,
//              "health": -1,
//              "attachments": [ "Mag_AKM_30Rnd", "AK_Suppressor" ],
//              "cargo": []
//          },
//          {
//              "className": "PlateCarrierVest",
//              "quantity": 1,
//              "health": -1,
//              "attachments": [],
//              "cargo": [
//                  { "className": "BandageDressing", "quantity": 2 }
//              ]
//          }
//      ]
//  }
//
//  quantity:  -1 или 0 - один предмет, иначе количество (ограничено SPAWN_MAX_QTY)
//  health:    -1 - заводское, 0..1 - доля, 1..100 - проценты
//  attachments: список classname, ставится в слоты предмета
//  cargo:     вложенные предметы (рекурсивно, максимум 3 уровня)
// ============================================================================

class PT_PresetItem
{
	string className;
	int    quantity;
	float  health;

	ref array<string> attachments;
	ref array<ref PT_PresetItem> cargo;
}

class PT_Preset
{
	string name;
	ref array<ref PT_PresetItem> items;
}

class PT_PresetLoader
{
	static PT_Preset Load(string name)
	{
		string path = PT_Config.PRESET_DIR + "/" + name + ".json";

		if (!FileExist(path))
			return null;

		PT_Preset preset = new PT_Preset();
		string errorMessage;
		JsonFileLoader<PT_Preset>.LoadFile(path, preset, errorMessage);

		if (!preset || !preset.items)
			return null;

		return preset;
	}
}
