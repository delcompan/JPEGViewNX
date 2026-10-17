// File association support (per-user, HKCU only - no admin rights needed).
//
// For every supported extension we register:
//   * a ProgID with shell\open\command          (HKCU\Software\Classes\JPEGViewNX.Image)
//   * the ProgID in .ext\OpenWithProgids        (both Classes and Explorer\FileExts)
//   * the ProgID as the legacy .ext default value
//   * JPEGViewNX.exe in FileExts\.ext\OpenWithList
//   * an Applications\JPEGViewNX.exe entry with SupportedTypes
//
// Windows 10/11 keeps the *default* handler in a protected UserChoice hash, so
// the OS may still ask the user to confirm - the UI offers a shortcut to the
// Default Apps page for that.
#pragma once

#include <windows.h>

#include <string>

size_t AssocFamilyCount();
const wchar_t* AssocFamilyLabel(size_t index);

// True when every extension of the family is currently registered for us.
bool AssocFamilyActive(size_t index);

bool AssocSetFamily(size_t index, bool on);
void AssocSetAll(bool on);

void AssocNotifyShell();
bool AssocOpenDefaultApps();

std::wstring AssocProgId();
