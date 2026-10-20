#pragma once

#include <windows.h>

#include <string>

// Starts `exe` suspended and queues LoadLibraryW(dll) on its main thread, so
// the DLL loads during process initialisation — after the static imports,
// before the executable's entry point — then resumes it.
//
// Returns nullptr on success, or the name of the step that failed, with the
// Windows error in *error (the child is terminated if it was created).
inline const wchar_t* StartWithDll(const std::wstring& exe, std::wstring command, const std::wstring& directory,
                                   const std::wstring& dll, PROCESS_INFORMATION* out, DWORD* error) {
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr,
                      directory.empty() ? nullptr : directory.c_str(), &startup, &process)) {
    *error = GetLastError();
    return L"CreateProcess";
  }
  const wchar_t* failed = nullptr;
  const SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(process.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  auto load_library = reinterpret_cast<PAPCFUNC>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  if (!remote) failed = L"VirtualAllocEx";
  else if (!WriteProcessMemory(process.hProcess, remote, dll.c_str(), bytes, nullptr)) failed = L"WriteProcessMemory";
  else if (!QueueUserAPC(load_library, process.hThread, reinterpret_cast<ULONG_PTR>(remote))) failed = L"QueueUserAPC";
  if (failed) {
    *error = GetLastError();
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return failed;
  }
  ResumeThread(process.hThread);
  *out = process;
  return nullptr;
}
