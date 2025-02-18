#pragma once

#include "common/SettingsInterface.h"
#include "pcsx2/Host.h"
#include "pcsx2-qt/Vanguard/VanguardJsonParser.h"
#include "pcsx2-qt/Vanguard/VanguardClientInitializer.h"
#include <string>
#include <variant>

#define VAR_NAME(Variable) (#Variable)

using SettingsTypes = std::variant<bool, int, float>;
using SettingsArray = std::vector< std::pair<std::string, SettingsTypes>>;

class VanguardSettings
{
public:
  SettingsArray array;
	std::pair<std::string, bool> CdvdPrecache;
	std::pair<std::string, bool> EnableThreadPinning;

	std::pair<std::string, int> EECycleRate;

	std::pair<std::string, bool> VsyncEnable;
	std::pair<std::string, int> VsyncQueueSize;
	std::pair<std::string, int> AspectRatio;
	std::pair<std::string, int> FMVAspectRatioSwitch;

  // save the current value of required settings
  void SaveSettings()
  {
	  save_setting<bool>("EmuCore", "CdvdPrecache", CdvdPrecache);
	  
	  save_setting<bool>("EmuCore", "EnableThreadPinning", EnableThreadPinning);




	  save_setting<int>("EmuCore/Speedhacks", "EECycleRate", EECycleRate);




	  save_setting<bool>("EmuCore/GS", "VsyncEnable", VsyncEnable);

	  save_setting<int>("EmuCore/GS", "VsyncQueueSize", VsyncQueueSize);

	  save_setting<char*>("EmuCore/GS",
		  "AspectRatio", AspectRatio, Pcsx2Config::GSOptions::AspectRatioNames, static_cast<size_t>(AspectRatioType::MaxCount));

	  save_setting<char*>("EmuCore/GS",
		  "FMVAspectRatioSwitch", FMVAspectRatioSwitch, Pcsx2Config::GSOptions::AspectRatioNames, static_cast<size_t>(FMVAspectRatioSwitchType::MaxCount));

  }

  // load the settings values sent from the dll hook into the emulator's settings
  void LoadSettings(JsonParser::JsonValue settings)
  {
	  load_setting<bool>("EmuCore", "CdvdPrecache", (*settings.json)[VAR_NAME(CdvdPrecache)]);

	  load_setting<bool>("EmuCore", "EnableThreadPinning", (*settings.json)[VAR_NAME(EnableThreadPinning)]);

	  
	  

	  load_setting<int>("EmuCore/Speedhacks", "EECycleRate", (*settings.json)[VAR_NAME(EECycleRate)]);




	  load_setting<bool>("EmuCore/GS", "VsyncEnable", (*settings.json)[VAR_NAME(VsyncEnable)]);

	  load_setting<int>("EmuCore/GS", "VsyncQueueSize", (*settings.json)[VAR_NAME(VsyncQueueSize)]);

	  load_setting<char*>("EmuCore/GS", "AspectRatio", (*settings.json)[VAR_NAME(AspectRatio)], Pcsx2Config::GSOptions::AspectRatioNames);

	  load_setting<char*>("EmuCore/GS", "FMVAspectRatioSwitch", (*settings.json)[VAR_NAME(FMVAspectRatioSwitch)], Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames);

	  VMManager::ApplySettings();
  }

  // Visits the variant value in a pair, determines the correct data type and returns it as a string
  std::string to_string(SettingsTypes var)
  {
    return std::visit([](auto arg) { return std::format("{}", arg); }, var);
  }

private:
  // saves the name and value of the setting to a pair, then push it onto the array
  template<typename T>
	void save_setting(std::string section, std::string var, std::pair<std::string, SettingsTypes> variable, const char* enumNames[] = nullptr, size_t size = 0)
  {
    variable.first = var;

    if (typeid(T) == typeid(bool))
		variable.second = Host::GetBaseBoolSettingValue(section.c_str(), var.c_str());

	else if (typeid(T) == typeid(int))
		variable.second = Host::GetBaseIntSettingValue(section.c_str(), var.c_str());

	else if (typeid(T) == typeid(float))
		variable.second = Host::GetBaseFloatSettingValue(section.c_str(), var.c_str());

	else if (typeid(T) == typeid(char*))
	{
		std::string str = Host::GetBaseStringSettingValue(section.c_str(), var.c_str());

		variable.second = matchEnum(str.data(), enumNames, size);
	}
		

    array.push_back(variable);
  }

  // loads the value of the parsed setting based on the requested data type
  template<typename T>
  void load_setting(std::string section, std::string var, JsonParser::JsonValue value, const char* enumNames[] = nullptr)
  {
	if (typeid(T) == typeid(bool))
		Host::SetBaseBoolSettingValue(section.c_str(), var.c_str(), value.b);

	else if (typeid(T) == typeid(int))
		Host::SetBaseIntSettingValue(section.c_str(), var.c_str(), value.i);

	else if (typeid(T) == typeid(float))
		Host::SetBaseFloatSettingValue(section.c_str(), var.c_str(), value.d);

	else if (typeid(T) == typeid(char*))
	{
		Host::SetBaseStringSettingValue(section.c_str(), var.c_str(), enumNames[value.i]);
	}
		

	Host::CommitBaseSettingChanges();
  }

  // special case where an enum value setting is stored as a char. Finds the correct char element and stores it as the enum position
  int matchEnum(char* name, const char* enumNames[], unsigned size)
  {
	  for (int i = 0; i < size; i++)
	  {
		  if (strcmp(name, enumNames[i]) == 0)
		  {
			  return i;
		  }
	  }
	  return 0;
  }
};
