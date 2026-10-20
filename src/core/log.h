#pragma once

// Appends to nwn5e.log next to nwn5e.dll. Safe to call from any thread.
void LogOpen(const wchar_t* directory);
void Log(const char* format, ...);
