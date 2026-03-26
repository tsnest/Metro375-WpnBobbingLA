#define _CRT_SECURE_NO_WARNINGS 1

#include "stdio.h"
#include <windows.h>
#include "MinHook.h"

#include "wpn_bobbing_la.h"

#define PSAPI_VERSION 1
#include <psapi.h>
#pragma comment (lib, "psapi.lib")

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

void ASMWrite(void* address, BYTE* code, size_t size)
{
	DWORD OldProtect = NULL;
	VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &OldProtect);
	memcpy(address, code, size);
	VirtualProtect(address, size, OldProtect, &OldProtect);
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
	DWORD uGame = NULL;
snova:
	if (!(uGame = (DWORD)GetModuleHandle("uGame.dll")))
		goto snova;

	MODULEINFO miUGame = GetModuleData("uGame.dll");

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

	return 0;
}

void* render_Orig = nullptr;

static void __declspec(naked) detour(void)
{
	__asm
	{
		// fix selflights
		and dword ptr[esi + 0E8h], 0FFF3FFFFh

		jmp render_Orig
	}
}

DWORD WINAPI InstallThreadRender(HMODULE hModule)
{
	DWORD uRender = NULL;
snova:
	if (!(uRender = (DWORD)GetModuleHandle("uRender.dll")))
		goto snova;

	MODULEINFO miURender = GetModuleData("uRender.dll");

	// 8B 3D ? ? ? ? 68 ? ? ? ? 50 FF D7 83 C4 08 85 C0 74 0A 81 A6
	void* instr = (void*)FindPattern(
		(DWORD)miURender.lpBaseOfDll,
		miURender.SizeOfImage,
		(BYTE*)"\x8B\x3D\x00\x00\x00\x00\x68\x00\x00\x00\x00\x50\xFF\xD7\x83\xC4\x08\x85\xC0\x74\x0A\x81\xA6",
		"xx????x????xxxxxxxxxxxx");

	MH_STATUS status = MH_CreateHook(instr, (LPVOID)&detour, (void**)&render_Orig);

	if (status == MH_OK) {
		if (MH_EnableHook(instr) != MH_OK) {
			MessageBox(NULL, "MH_EnableHook() != MH_OK", "selflights", MB_OK | MB_ICONERROR);
		}
	} else {
		MessageBox(NULL, "MH_CreateHook() != MH_OK", "selflights", MB_OK | MB_ICONERROR);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
	if(reason == DLL_PROCESS_ATTACH)
	{
		Beep(1000, 200);

		//AllocConsole();
		//freopen("CONOUT$", "w", stdout);

		bool minhook = (MH_Initialize() == MH_OK);
		if (!minhook) {
			MessageBox(NULL, "MinHook not initialized!", "MinHook", MB_OK | MB_ICONERROR);
		} else {
			CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)InstallThread, hModule, NULL, NULL);
			CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)InstallThreadRender, hModule, NULL, NULL);
		}
	}
	
	return TRUE;
}