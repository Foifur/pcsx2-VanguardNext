#include "pcsx2-qt/Vanguard/VanguardHelpers.h"
#include "pcsx2-qt/Vanguard/VanguardClientInitializer.h"
#include "pcsx2-qt/Vanguard/VanguardJsonParser.h"
#include "pcsx2-qt/Vanguard/VanguardEmuSettings.h"
#include <cstddef>
#include <Memory.h>
#include <Cache.h>
#include <MainWindow.h>
#include <SPU2/spu2.h>
#include <GS.h>
#include <IopMem.h>
#include "Cache.h"
#include <SPU2/defs.h>

void FormatJsonData(VanguardSettings& settings, std::ostringstream& json_string);

unsigned char Vanguard_peekbyte(long long addr, int selection)
{
	u8 byte = 0;
	u16 data = 0;
	long long mod = 0;
	long long newAddr = 0;

	switch (selection)
	{
		// EE RAM read
		case 0:
			byte = memRead8(addr);
			break;
		// GS Registers read
		case 1:
			byte = gsRead8(addr);
			break;
		// IOP RAM read
		case 2:
			byte = iopMemRead8(addr);
			break;
		default:
			break;
		// SPU2 Registers read
		case 3:
			mod = addr % 2;
			newAddr = addr - mod;
			data = SPU2read(newAddr);
			if (mod < 1)
			{
				byte = data & 0xFF;
			}
			else
			{
				byte = (data >> 8) & 0xFF;
			}
			break;
		// SPU2 RAM read
		case 4:
			mod = addr % 2;
			newAddr = addr - mod;
			data = spu2M_Read(newAddr);
			if (mod < 1)
			{
				byte = data & 0xFF;
			}
			else
			{
				byte = (data >> 8) & 0xFF;
			}
			break;
	}
	return byte;
}

void Vanguard_pokebyte(long long addr, unsigned char val, int selection)
{
	u16 value = 0;

	switch (selection)
	{
		// EERAM write
		case 0:
			memWrite8(addr, val);
			break;
		// GS Registers write
		case 1:
			gsWrite8(addr, val);
			break;
		// IOP RAM write
		case 2:
			iopMemWrite8(addr, val);
			break;
		// SPU2 Registers write
		case 3:
			if ((addr % 2) == 0)
			{
				value = (val << 8) | (Vanguard_peekbyte(addr + 1, 1));
			}
			else
			{
				addr -= 1;
				value = (Vanguard_peekbyte(addr, 1) << 8) | val;
			}
			SPU2write(addr, value);
			break;
		// SPU2 RAM write
		case 4:
			if ((addr % 2) == 0)
			{
				value = (val << 8) | (Vanguard_peekbyte(addr + 1, 1));
			}
			else
			{
				addr -= 1;
				value = (Vanguard_peekbyte(addr, 1) << 8) | val;
			}
			spu2M_Write(addr, value);
			break;
		default:
			break;

	}

}

bool VanguardClient::ok_to_corestep = false;
void Vanguard_pause(bool pauseUntilCorrupt)
{
	if (VMManager::HasValidVM())
	{
		VMManager::SetState(VMState::Paused);
	}
}

void Vanguard_resume()
{
	if (VMManager::HasValidVM())
	{
		VMManager::SetState(VMState::Running);
		VanguardClient::ok_to_corestep = true;
	}
}


void Vanguard_savesavestate(BSTR filename, bool wait)
{
    if (g_emu_thread->isRunning())
    {
		//Convert the BSTR sent by Vanguard to std::string
		std::string filename_converted = BSTRToString(filename);
		g_emu_thread->EmuThread::saveState(QString::fromStdString(filename_converted));
    }
}


void Vanguard_loadsavestate(BSTR filename)
{
  // Convert the BSTR sent by Vanguard to std::string
  std::string filename_converted = BSTRToString(filename);
  g_emu_thread->EmuThread::loadState(QString::fromStdString(filename_converted));
}


bool VanguardClient::loading = false;
void Vanguard_loadROM(BSTR filename)
{
  VanguardClient::loading = true;
  VanguardClient::ok_to_corestep = false;

  std::string converted_filename = BSTRToString(filename);

  if (VMManager::GetState() != VMState::Running && VMManager::GetState() != VMState::Paused)
	VanguardClientInitializer::win->MainWindow::doStartFile(std::nullopt, QString::fromStdString(converted_filename));
  else
  {
	VanguardClientInitializer::win->MainWindow::requestShutdown(false, false, false);
	VanguardClientInitializer::win->MainWindow::doStartFile(std::nullopt, QString::fromStdString(converted_filename));
  }


  MSG msg;
  // We have to do it this way to prevent deadlock due to synced calls. It sucks but it's required
  // at the moment
  while (VanguardClient::loading)
  {
    Sleep(20);
    //these lines of code perform the equivalent of the Application.DoEvents method
    ::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
    ::GetMessage(&msg, NULL, 0, 0);
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  Sleep(100);  // Give the emu thread a chance to recover
  
}

void Vanguard_finishLoading()
{
  VanguardClient::loading = false;
}

void Vanguard_closeGame()
{
	VanguardClientInitializer::win->MainWindow::requestShutdown(false, false, false);
}

void Vanguard_prepShutdown()
{
	VanguardClientInitializer::win->MainWindow::requestShutdown(false, false, false);
}

void Vanguard_forceStop()
{
	VanguardClientInitializer::win->MainWindow::requestExit(false);
}

std::string VanguardClient::system_core = "EMPTY";
char* Vanguard_getSystemCore()
{
	// store the output as a string, then convert it to char*
	std::string tmp = VanguardClient::system_core;

	std::vector<char> _output(tmp.begin(), tmp.end());
	_output.push_back('\0');

	char* output = (char*)LocalAlloc(LMEM_FIXED, _output.size() + 1);
	if (!output)
		return NULL;

	memcpy(output, _output.data(), _output.size() + 1);

	return output;
}

// Saves all required emulator settings and returns it to the hook DLL to store with the savestate
char* Vanguard_saveEmuSettings()
{
	// create a new settings class and store all values
	VanguardSettings _settings;
	_settings.SaveSettings();

	// write the json data to a stringstream
	std::ostringstream out;
	FormatJsonData(_settings, out);

	// store the output as a string, then convert it to char*
	std::string tmp = out.str();

	std::vector<char> _output(tmp.begin(), tmp.end());
	_output.push_back('\0');

	char* output = (char*)LocalAlloc(LMEM_FIXED, _output.size() + 1);
	if (!output)
		return NULL;

	memcpy(output, _output.data(), _output.size() + 1);
	return output;
}

// Loads all required emulator settings sent by the hook DLL before loading the savestate
void Vanguard_loadEmuSettings(BSTR settings)
{
	JsonParser::JsonValue parsed_settings = JsonParser::ParseJson(settings);

	VanguardSettings _settings;
	_settings.LoadSettings(parsed_settings);

	// make sure to reload the settings window to show the updates
	if (VanguardClientInitializer::win->getSettingsWindow())
		VanguardClientInitializer::win->recreateSettings();
}

//converts a BSTR received from the Vanguard client to std::string
std::string BSTRToString(BSTR string)
{
  std::wstring ws(string, SysStringLen(string));
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  std::string converted_string = converter.to_bytes(ws);
  return converted_string;
}

std::string getDirectory()
{
  char buffer[MAX_PATH] = {0};
  GetModuleFileNameA(NULL, buffer, MAX_PATH);
  std::string::size_type pos = std::string(buffer).find_last_of("\\/");
  return std::string(buffer).substr(0, pos);
}

// formats the saved settings into a JSON format
void FormatJsonData(VanguardSettings& settings, std::ostringstream& json_string)
{
	// beginning of json string
	json_string << "{\n";

	// iterate through all settings
	for (int i = 0; i < settings.array.size(); i++)
	{
		json_string << "  \"" << settings.array[i].first
					<< "\": " << settings.to_string(settings.array[i].second);

		if (std::holds_alternative<float>(settings.array[i].second))
		{
			json_string << ".0";
		}

		// only add a comma if there are more values to be parsed
		if (i + 1 < settings.array.size())
			json_string << ",\n";
		else
			json_string << "\n";
	}

	// end of json string
	json_string << "}";
}
