// nwn5e.exe (EE) — starts NWN: Enhanced Edition's 64-bit nwmain.exe with
// nwn5e.dll loaded before any game code runs.
//
// The game files are never modified: launching EE normally (Steam) is still
// vanilla. Instead of a fixed build, the DLL finds the engine functions by
// their exported names and logs any that a build lacks (docs/EE-RECON.md).
//
// Game folder: first argument "--game <dir>", else nwn5e.ini [loader]
// game_dir, else the Steam default. Like Steam, it starts the game in
// bin\win32 (EE finds its data folder from there) with Steam's app ID in the
// environment; the Steam client should be running.

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>

#include <string>

#include "inject.h"

#pragma comment(lib, "shlwapi.lib")

namespace {

constexpr const wchar_t* kDefaultGameDir = L"X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
constexpr const wchar_t* kBinInGameDir = L"\\bin\\win32";
constexpr const wchar_t* kSteamAppId = L"704450";  // NWN: EE on Steam (steam_appid.txt)

void Fail(const wchar_t* message) {
  MessageBoxW(nullptr, message, L"nwn5e (EE)", MB_ICONERROR | MB_OK);
  ExitProcess(1);
}

std::wstring ModuleDirectory() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  PathRemoveFileSpecW(path);
  return path;
}

std::wstring GameDir(int argc, wchar_t** argv, const std::wstring& ini) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (lstrcmpiW(argv[i], L"--game") == 0) return argv[i + 1];
  }
  wchar_t value[MAX_PATH] = {};
  GetPrivateProfileStringW(L"loader", L"game_dir", L"", value, MAX_PATH, ini.c_str());
  return value[0] ? value : kDefaultGameDir;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  const std::wstring here = ModuleDirectory();
  const std::wstring ini = here + L"\\nwn5e.ini";
  const std::wstring dll = here + L"\\nwn5e.dll";
  const std::wstring game = GameDir(argc, argv, ini);
  const std::wstring bin = game + kBinInGameDir;
  const std::wstring exe = bin + L"\\nwmain.exe";
  if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) Fail(L"nwn5e.dll is missing next to nwn5e.exe.");
  if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
    Fail((L"No NWN: Enhanced Edition at " + exe + L". Set [loader] game_dir in nwn5e.ini.").c_str());
  }

  // Pass the game's own arguments through (everything except --game <dir>).
  std::wstring command = L"\"" + exe + L"\"";
  for (int i = 1; i < argc; ++i) {
    if (lstrcmpiW(argv[i], L"--game") == 0) {
      ++i;
      continue;
    }
    command += L" \"";
    command += argv[i];
    command += L"\"";
  }

  // What Steam sets when it starts the game (inherited by nwmain.exe).
  if (!GetEnvironmentVariableW(L"SteamAppId", nullptr, 0)) SetEnvironmentVariableW(L"SteamAppId", kSteamAppId);
  if (!GetEnvironmentVariableW(L"SteamGameId", nullptr, 0)) SetEnvironmentVariableW(L"SteamGameId", kSteamAppId);

  PROCESS_INFORMATION process = {};
  DWORD error = 0;
  if (const wchar_t* step = StartWithDll(exe, command, bin, dll, &process, &error)) {
    wchar_t message[512];
    swprintf_s(message, L"Could not start nwmain.exe with nwn5e.dll: %s failed with Windows error %lu.", step, error);
    Fail(message);
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  LocalFree(argv);
  return 0;
}
