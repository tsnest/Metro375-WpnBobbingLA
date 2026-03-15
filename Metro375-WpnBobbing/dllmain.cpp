#define _CRT_SECURE_NO_WARNINGS 1

#include "stdio.h"
#include <windows.h>
#include "MinHook.h"

#include "wpn_bobbing_la.h"

#define PSAPI_VERSION 1
#include <psapi.h>
#pragma comment (lib, "psapi.lib")

// signature scanner
MODULEINFO miUGame;

MODULEINFO GetModuleData(const char* moduleName)
{
	MODULEINFO currentModuleInfo = { 0 };
	HMODULE moduleHandle = GetModuleHandle(moduleName);
	if (moduleHandle == NULL)
	{
		return currentModuleInfo;
	}
	GetModuleInformation(GetCurrentProcess(), moduleHandle, &currentModuleInfo, sizeof(MODULEINFO));
	return currentModuleInfo;
}

bool DataCompare(const BYTE* pData, const BYTE* pattern, const char* mask)
{
	for (; *mask; mask++, pData++, pattern++)
		if (*mask == 'x' && *pData != *pattern)
			return false;
	return (*mask) == NULL;
}

DWORD FindPattern(DWORD start_address, DWORD length, BYTE* pattern, char* mask)
{
	for (DWORD i = 0; i < length; i++)
		if (DataCompare((BYTE*)(start_address + i), pattern, mask))
			return (DWORD)(start_address + i);
	return NULL;
}

typedef void*(__thiscall* _cmd_float)(void* _this, const char* name, float* flt_ptr, float min_value, float max_value);
typedef void(__thiscall* _command_add)(void* _console, void* C);

void RestoreFovCMD(DWORD uGame)
{
	float* g_fov = (float*)(uGame + 0xBCC58);
	void* g_console = *(void**)GetProcAddress(NULL, "?g_console@@3PAVserver@uconsole@@A");

	DWORD OldFovProtect = NULL;
	VirtualProtect(g_fov, sizeof(float), PAGE_READWRITE, &OldFovProtect);

	void* cmd_float_fov = malloc(0x18);
	_cmd_float cmd_float = (_cmd_float)GetProcAddress(NULL, "??0cmd_float@uconsole@@QAE@PBDPAMMM@Z");
	cmd_float(cmd_float_fov, "fov", g_fov, 5.0f, 180.0f);

	_command_add command_add = *(_command_add*)((*(DWORD*)g_console) + 0x4);
	command_add(g_console, cmd_float_fov);
}

DWORD WINAPI InstallThread(HMODULE hModule)
{
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);

	Beep(1000, 200);

	bool minhook = (MH_Initialize() == MH_OK);
	if (!minhook)
	{
		MessageBox(NULL, "MinHook not initialized!", "MinHook", MB_OK | MB_ICONERROR);
	} else {
		DWORD uGame = NULL;

	snova:
		if (!(uGame = (DWORD)GetModuleHandle("uGame.dll")))
			goto snova;

		miUGame = GetModuleData("uGame.dll");

		RestoreFovCMD(uGame);

		//typedef void(__cdecl* _msg)(char* Format, ...);
		//_msg msg = (_msg)GetProcAddress(GetModuleHandle("uCore.dll"), "?msg@@YAXPBDZZ");

		// 50 FF D2 51 8B 4E 0C 8B C4 89 64 24 0C E8 ? ? ? ? A1 ? ? ? ? 8B 08 8B 11 8B 42 24 FF D0 5E 5B 59 C3
		LPVOID eaxMatrix = (LPVOID)FindPattern(
			(DWORD)miUGame.lpBaseOfDll,
			miUGame.SizeOfImage,
			(BYTE*)"\x50\xFF\xD2\x51\x8B\x4E\x0C\x8B\xC4\x89\x64\x24\x0C\xE8\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x8B\x08\x8B\x11\x8B\x42\x24\xFF\xD0\x5E\x5B\x59\xC3",
			"xxxxxxxxxxxxxx????x????xxxxxxxxxxxxx");

		// F3 0F 10 01 F3 0F 59 02 F3 0F 10 49 ? F3 0F 59 4A ? F3 0F 58 C1
		//LPVOID CWeaponHUD_UpdatePosition_Address = (LPVOID)FindPattern(
		//	(DWORD)miUGame.lpBaseOfDll,
		//	miUGame.SizeOfImage,
		//	(BYTE*)"\xF3\x0F\x10\x01\xF3\x0F\x59\x02\xF3\x0F\x10\x49\x00\xF3\x0F\x59\x4A\x00\xF3\x0F\x58\xC1",
		//	"xxxxxxxxxxxx?xxxx?xxxx");

		install_wpn_bobbing(eaxMatrix);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
	if(reason == DLL_PROCESS_ATTACH)
	{
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)InstallThread, hModule, NULL, NULL);
	}
	
	return TRUE;
}