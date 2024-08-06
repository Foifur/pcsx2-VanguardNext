#include "Vanguard/VanguardHelpers.h"
#include "Vanguard/VanguardClientInitializer.h"
#include <cstddef>
#include <Memory.h>
#include <Cache.h>
#include <MainWindow.h>
#include <SPU2/spu2.h>
#include <GS.h>
#include <IopMem.h>


unsigned char Vanguard_peekbyte(long long addr, int selection)
{
	u8 byte = 0;
	u16 data = 0;

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
		// SPU2 RAM read
		case 2:
			data = SPU2read(addr);

			if ((addr % 2) == 0)
			{
				byte = data & 0xFF;
			}
			else
			{
				byte = (data >> 8) & 0xFF;
			}
			break;
		// SPU2 Register read
		case 3:
			data = iopMemRead8(addr);
		default:
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
		// SPU2RAM write
		case 2:
			if ((addr % 2) == 0)
			{
				value = (val << 8) | (Vanguard_peekbyte(addr + 1, 1));
			}
			else
			{
				addr -= 1;
				value = (Vanguard_peekbyte(addr, 1) << 8) | val;
			}
			SPU2write(static_cast<u32>(addr), value);
			break;
		// SPU2 Register write
		case 3:
			iopMemWrite8(addr, val);
		default:
			break;

	}

}


// pauses the emulator. If we're applying a corruption from cold boot,
// make sure we don't resume the emulator after loading the game.
void Vanguard_pause(bool pauseUntilCorrupt)
{
	if (VMManager::HasValidVM())
	    VMManager::SetState(VMState::Paused);
}

void Vanguard_resume()
{
	if (VMManager::HasValidVM())
	    VMManager::SetState(VMState::Running);
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
