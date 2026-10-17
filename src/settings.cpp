#include "settings.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace {

std::wstring IniPath(const std::wstring& dir) {
    return dir + L"\\JPEGViewNX.ini";
}

std::wstring ReadStr(const std::wstring& ini, const wchar_t* key, const wchar_t* def) {
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(L"Settings", key, def, buf, 512, ini.c_str());
    return buf;
}

int ReadInt(const std::wstring& ini, const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntW(L"Settings", key, def, ini.c_str());
}

bool ReadBool(const std::wstring& ini, const wchar_t* key, bool def) {
    std::wstring v = ReadStr(ini, key, def ? L"true" : L"false");
    if (v.size() == 1) return v[0] == L'1' || v[0] == L't' || v[0] == L'T' || v[0] == L'y';
    return _wcsicmp(v.c_str(), L"true") == 0 || _wcsicmp(v.c_str(), L"yes") == 0 ||
           _wcsicmp(v.c_str(), L"on") == 0;
}

// "R G B" -> COLORREF, same textual format JPEGView uses.
bool ParseColor(const std::wstring& text, COLORREF* out) {
    int r = 0, g = 0, b = 0;
    if (swscanf_s(text.c_str(), L"%d %d %d", &r, &g, &b) != 3) return false;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    *out = RGB(r, g, b);
    return true;
}

void WriteColor(const std::wstring& ini, const wchar_t* key, COLORREF c) {
    wchar_t buf[64];
    swprintf_s(buf, L"%d %d %d", (int)GetRValue(c), (int)GetGValue(c), (int)GetBValue(c));
    WritePrivateProfileStringW(L"Settings", key, buf, ini.c_str());
}

TransparencyMode ParseMode(const std::wstring& text) {
    if (text == L"0" || _wcsicmp(text.c_str(), L"solid") == 0)   return TM_SOLID;
    if (text == L"2" || _wcsicmp(text.c_str(), L"none") == 0)    return TM_NONE;
    return TM_CHECKER;   // "1", "checker", anything else -> the fixed behaviour
}

} // namespace

void Settings::Load(const std::wstring& iniDir) {
    const std::wstring ini = IniPath(iniDir);
    const std::wstring legacy = iniDir + L"\\JPEGView.ini";

    // Prefer our own file; fall back to a stock JPEGView.ini next to the exe so
    // an existing install keeps its colours.
    auto Pick = [&](const wchar_t* key, const wchar_t* def) -> std::wstring {
        std::wstring v = ReadStr(ini, key, L"\x01missing");
        if (v == L"\x01missing") v = ReadStr(legacy, key, def);
        return v;
    };

    ParseColor(Pick(L"BackgroundColor", L"0 0 0"), &backgroundColor);
    ParseColor(Pick(L"TransparencyColor", L"0 0 0"), &transparencyColor);

    transparencyMode = ParseMode(ReadStr(ini, L"TransparencyMode", L"checker"));
    checkerCellSize = ReadInt(ini, L"CheckerCellSize", 8);
    if (checkerCellSize < 2)   checkerCellSize = 2;
    if (checkerCellSize > 128) checkerCellSize = 128;
    ParseColor(ReadStr(ini, L"CheckerColor1", L"64 64 64"), &checkerColor1);
    ParseColor(ReadStr(ini, L"CheckerColor2", L"96 96 96"), &checkerColor2);

    const std::wstring lang = ReadStr(ini, L"Language", L"zh");
    language = (_wcsicmp(lang.c_str(), L"en") == 0) ? 1 : 0;

    smoothResampling = ReadBool(ini, L"SmoothResampling", true);
    showFileName     = ReadBool(ini, L"ShowFileName", true);

    const std::wstring raw = ReadStr(ini, L"RawMode", L"auto");
    if (_wcsicmp(raw.c_str(), L"full") == 0)         rawMode = 1;
    else if (_wcsicmp(raw.c_str(), L"preview") == 0) rawMode = 2;
    else                                             rawMode = 0;

    int x = ReadInt(ini, L"WindowX", windowRect.left);
    int y = ReadInt(ini, L"WindowY", windowRect.top);
    int w = ReadInt(ini, L"WindowWidth",  windowRect.right  - windowRect.left);
    int h = ReadInt(ini, L"WindowHeight", windowRect.bottom - windowRect.top);
    if (w < 200) w = 200;
    if (h < 150) h = 150;
    windowRect = { x, y, x + w, y + h };
    windowMaximized = ReadBool(ini, L"WindowMaximized", false);
}

void Settings::Save(const std::wstring& iniDir) const {
    const std::wstring ini = IniPath(iniDir);
    WriteColor(ini, L"BackgroundColor", backgroundColor);
    WriteColor(ini, L"TransparencyColor", transparencyColor);
    WritePrivateProfileStringW(L"Settings", L"TransparencyMode", ModeName(), ini.c_str());
    wchar_t buf[64];
    swprintf_s(buf, L"%d", checkerCellSize);
    WritePrivateProfileStringW(L"Settings", L"CheckerCellSize", buf, ini.c_str());
    WriteColor(ini, L"CheckerColor1", checkerColor1);
    WriteColor(ini, L"CheckerColor2", checkerColor2);
    WritePrivateProfileStringW(L"Settings", L"SmoothResampling", smoothResampling ? L"true" : L"false", ini.c_str());
    WritePrivateProfileStringW(L"Settings", L"ShowFileName", showFileName ? L"true" : L"false", ini.c_str());
    WritePrivateProfileStringW(L"Settings", L"RawMode", RawModeName(), ini.c_str());
    WritePrivateProfileStringW(L"Settings", L"Language", language == 1 ? L"en" : L"zh", ini.c_str());
    swprintf_s(buf, L"%d", (int)windowRect.left);
    WritePrivateProfileStringW(L"Settings", L"WindowX", buf, ini.c_str());
    swprintf_s(buf, L"%d", (int)windowRect.top);
    WritePrivateProfileStringW(L"Settings", L"WindowY", buf, ini.c_str());
    swprintf_s(buf, L"%d", (int)(windowRect.right - windowRect.left));
    WritePrivateProfileStringW(L"Settings", L"WindowWidth", buf, ini.c_str());
    swprintf_s(buf, L"%d", (int)(windowRect.bottom - windowRect.top));
    WritePrivateProfileStringW(L"Settings", L"WindowHeight", buf, ini.c_str());
    WritePrivateProfileStringW(L"Settings", L"WindowMaximized", windowMaximized ? L"true" : L"false", ini.c_str());
}

const wchar_t* Settings::ModeName() const {
    switch (transparencyMode) {
        case TM_SOLID:   return L"solid";
        case TM_NONE:    return L"none";
        default:         return L"checker";
    }
}

const wchar_t* Settings::RawModeName() const {
    switch (rawMode) {
        case 1:  return L"full";
        case 2:  return L"preview";
        default: return L"auto";
    }
}

void Settings::CycleMode() {
    transparencyMode = (TransparencyMode)(((int)transparencyMode + 1) % 3);
}
