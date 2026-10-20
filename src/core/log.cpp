#include "log.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

namespace {
CRITICAL_SECTION g_lock;
HANDLE g_file = INVALID_HANDLE_VALUE;
}  // namespace

void LogOpen(const wchar_t* directory) {
  InitializeCriticalSection(&g_lock);
  wchar_t path[MAX_PATH];
  swprintf_s(path, L"%s\\nwn5e.log", directory);
  g_file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Log(const char* format, ...) {
  if (g_file == INVALID_HANDLE_VALUE) return;
  char line[1024];
  SYSTEMTIME now;
  GetLocalTime(&now);
  int used = sprintf_s(line, "%02d:%02d:%02d.%03d ", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
  va_list args;
  va_start(args, format);
  int written = vsnprintf(line + used, sizeof(line) - used - 2, format, args);
  va_end(args);
  used += written < 0 ? 0 : written;
  if (used > static_cast<int>(sizeof(line)) - 2) used = sizeof(line) - 2;
  line[used++] = '\r';
  line[used++] = '\n';
  EnterCriticalSection(&g_lock);
  DWORD ignored;
  WriteFile(g_file, line, used, &ignored, nullptr);
  FlushFileBuffers(g_file);
  LeaveCriticalSection(&g_lock);
}
