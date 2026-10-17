#include "shellassoc.h"

#include <shlobj.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <vector>

namespace {

const wchar_t* kProgId = L"JPEGViewNX.Image";
const wchar_t* kClasses = L"Software\\Classes\\";
const wchar_t* kFileExts =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\";

// ---------------------------------------------------------------- families

const wchar_t* const kJpeg[] = { L".jpg", L".jpeg", L".jpe", L".jfif" };
const wchar_t* const kPng[] = { L".png" };
const wchar_t* const kBmp[] = { L".bmp", L".dib" };
const wchar_t* const kGif[] = { L".gif" };
const wchar_t* const kTiff[] = { L".tif", L".tiff" };
const wchar_t* const kWebp[] = { L".webp" };
const wchar_t* const kTga[] = { L".tga" };
const wchar_t* const kJxr[] = { L".wdp", L".hdp", L".mdp", L".jxr" };
const wchar_t* const kJxl[] = { L".jxl" };
const wchar_t* const kHeif[] = { L".heic", L".heif", L".hif" };
const wchar_t* const kAvif[] = { L".avif", L".avifs" };
const wchar_t* const kDds[] = { L".dds" };
const wchar_t* const kIco[] = { L".ico", L".cur" };
const wchar_t* const kPcx[] = { L".pcx" };
const wchar_t* const kPnm[] = { L".pnm", L".ppm", L".pgm", L".pbm", L".pxm" };
const wchar_t* const kRaw[] = {
    L".3fr", L".ari", L".arw", L".bay", L".cap", L".cr2", L".cr3", L".crw",
    L".dcs", L".dcr", L".dng", L".drf", L".eip", L".erf", L".fff", L".gpr",
    L".iiq", L".k25", L".kdc", L".mef", L".mos", L".mrw", L".nef", L".nrw",
    L".orf", L".ori", L".pef", L".ptx", L".pxn", L".raf", L".raw", L".rw2",
    L".rwl", L".sr2", L".srf", L".srw", L".x3f"
};

struct Family {
    const wchar_t* label;
    const wchar_t* const* exts;
    size_t count;
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

const Family kFamilies[] = {
    { L"JPEG  (*.jpg *.jpeg *.jpe *.jfif)",              kJpeg, COUNT(kJpeg) },
    { L"PNG  (*.png)",                                   kPng,  COUNT(kPng)  },
    { L"BMP  (*.bmp *.dib)",                             kBmp,  COUNT(kBmp)  },
    { L"GIF  (*.gif)",                                   kGif,  COUNT(kGif)  },
    { L"TIFF  (*.tif *.tiff)",                           kTiff, COUNT(kTiff) },
    { L"WebP  (*.webp)",                                 kWebp, COUNT(kWebp) },
    { L"TGA  (*.tga)",                                   kTga,  COUNT(kTga)  },
    { L"JPEG XR  (*.wdp *.hdp *.mdp *.jxr)",             kJxr,  COUNT(kJxr)  },
    { L"JPEG XL  (*.jxl)",                               kJxl,  COUNT(kJxl)  },
    { L"HEIC / HEIF  (*.heic *.heif *.hif)",             kHeif, COUNT(kHeif) },
    { L"AVIF  (*.avif *.avifs)",                         kAvif, COUNT(kAvif) },
    { L"DDS  (*.dds)",                                   kDds,  COUNT(kDds)  },
    { L"ICO / CUR  (*.ico *.cur)",                       kIco,  COUNT(kIco)  },
    { L"PCX  (*.pcx)",                                   kPcx,  COUNT(kPcx)  },
    { L"PNM  (*.pnm *.ppm *.pgm *.pbm)",                 kPnm,  COUNT(kPnm)  },
    { L"Camera RAW  (*.cr2 *.cr3 *.nef *.arw *.dng ...)", kRaw, COUNT(kRaw) },
};

// ---------------------------------------------------------------- registry

std::wstring ExePath() {
    wchar_t buf[32768] = {};
    GetModuleFileNameW(nullptr, buf, 32768);
    return buf;
}

std::wstring ExeName() {
    std::wstring p = ExePath();
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? p : p.substr(slash + 1);
}

std::wstring Quote(const std::wstring& s) { return L"\"" + s + L"\""; }

bool RegWrite(HKEY root, const std::wstring& sub, const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr)
        != ERROR_SUCCESS) {
        return false;
    }
    const LONG r = RegSetValueExW(key, name, 0, REG_SZ,
                                  (const BYTE*)value,
                                  (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool RegWriteEmpty(HKEY root, const std::wstring& sub, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr)
        != ERROR_SUCCESS) {
        return false;
    }
    const LONG r = RegSetValueExW(key, name, 0, REG_NONE, nullptr, 0);
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool RegRead(HKEY root, const std::wstring& sub, const wchar_t* name, std::wstring& out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    wchar_t buf[2048] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG r = RegQueryValueExW(key, name, nullptr, &type, (BYTE*)buf, &size);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || type != REG_SZ || size < sizeof(wchar_t)) return false;
    out.assign(buf, size / sizeof(wchar_t) - 1);
    return true;
}

bool RegDeleteValueSafe(HKEY root, const std::wstring& sub, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;
    RegDeleteValueW(key, name);
    RegCloseKey(key);
    return true;
}

bool HasValue(HKEY root, const std::wstring& sub, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    BYTE buf[32];
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG r = RegQueryValueExW(key, name, nullptr, &type, buf, &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

void RegisterProgram() {
    const std::wstring exe = ExePath();
    const std::wstring command = Quote(exe) + L" \"%1\"";
    const std::wstring icon = Quote(exe) + L",0";

    const std::wstring progId = std::wstring(kClasses) + kProgId;
    RegWrite(HKEY_CURRENT_USER, progId, nullptr, L"JPEGViewNX image");
    RegWrite(HKEY_CURRENT_USER, progId + L"\\DefaultIcon", nullptr, icon.c_str());
    RegWrite(HKEY_CURRENT_USER, progId + L"\\shell\\open\\command", nullptr, command.c_str());

    const std::wstring app = std::wstring(kClasses) + L"Applications\\" + ExeName();
    RegWrite(HKEY_CURRENT_USER, app, L"FriendlyAppName", L"JPEGViewNX");
    RegWrite(HKEY_CURRENT_USER, app + L"\\DefaultIcon", nullptr, icon.c_str());
    RegWrite(HKEY_CURRENT_USER, app + L"\\shell\\open\\command", nullptr, command.c_str());
    for (const Family& f : kFamilies) {
        for (size_t i = 0; i < f.count; ++i) {
            RegWriteEmpty(HKEY_CURRENT_USER, app + L"\\SupportedTypes", f.exts[i]);
        }
    }
}

// --------------------------------------------------------- OpenWithList

// Registers the app in Settings > Default apps (Capabilities + RegisteredApplications).
void RegisterCapabilities() {
    const std::wstring exe = ExePath();
    const std::wstring icon = Quote(exe) + L",0";
    const std::wstring progId = std::wstring(kClasses) + kProgId;
    const std::wstring caps = progId + L"\\Capabilities";
    RegWrite(HKEY_CURRENT_USER, caps, L"ApplicationName", L"JPEGViewNX");
    RegWrite(HKEY_CURRENT_USER, caps, L"ApplicationDescription", L"JPEGViewNX image viewer");
    RegWrite(HKEY_CURRENT_USER, caps, L"ApplicationIcon", icon.c_str());
    for (const Family& f : kFamilies) {
        for (size_t i = 0; i < f.count; ++i) {
            RegWrite(HKEY_CURRENT_USER, caps + L"\\FileAssociations", f.exts[i], kProgId);
        }
    }
    // Value data is relative to HKCU\Software.
    RegWrite(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"JPEGViewNX",
             (std::wstring(L"Software\\Classes\\") + kProgId + L"\\Capabilities").c_str());
}

// Remember the value we are about to overwrite, so removing an association can
// restore the user's previous handler instead of leaving the extension unbound.
const wchar_t* kBackupKey = L"Software\\JPEGViewNX\\Backup";

void BackupDefaultValue(const std::wstring& ext) {
    const std::wstring cls = std::wstring(kClasses) + ext;
    std::wstring current;
    if (!RegRead(HKEY_CURRENT_USER, cls, nullptr, current)) return;
    if (current.empty() || _wcsicmp(current.c_str(), kProgId) == 0) return;
    std::wstring existing;
    if (RegRead(HKEY_CURRENT_USER, kBackupKey, ext.c_str(), existing)) return;
    RegWrite(HKEY_CURRENT_USER, kBackupKey, ext.c_str(), current.c_str());
}

void RestoreDefaultValue(const std::wstring& ext) {
    const std::wstring cls = std::wstring(kClasses) + ext;
    std::wstring backup;
    if (RegRead(HKEY_CURRENT_USER, kBackupKey, ext.c_str(), backup) && !backup.empty()) {
        RegWrite(HKEY_CURRENT_USER, cls, nullptr, backup.c_str());
        RegDeleteValueSafe(HKEY_CURRENT_USER, kBackupKey, ext.c_str());
        return;
    }
    std::wstring current;
    if (RegRead(HKEY_CURRENT_USER, cls, nullptr, current) &&
        _wcsicmp(current.c_str(), kProgId) == 0) {
        RegDeleteValueSafe(HKEY_CURRENT_USER, cls, nullptr);
    }
}

void RebuildMru(HKEY root, const std::wstring& key) {
    std::wstring mru;
    RegRead(root, key, L"MRUList", mru);
    std::wstring kept;
    for (wchar_t c : mru) {
        if (c < L'a' || c > L'z') continue;
        std::wstring v;
        const std::wstring name(1, c);
        if (RegRead(root, key, name.c_str(), v)) kept.push_back(c);
    }
    if (kept.empty()) RegDeleteValueSafe(root, key, L"MRUList");
    else RegWrite(root, key, L"MRUList", kept.c_str());
}

void SetOpenWithList(const std::wstring& fileExtsKey, bool add) {
    const std::wstring key = fileExtsKey + L"\\OpenWithList";
    const std::wstring exeName = ExeName();

    int ourLetter = -1;
    int firstFree = -1;
    for (int i = 0; i < 26; ++i) {
        const std::wstring name(1, (wchar_t)(L'a' + i));
        std::wstring v;
        if (RegRead(HKEY_CURRENT_USER, key, name.c_str(), v)) {
            if (_wcsicmp(v.c_str(), exeName.c_str()) == 0) ourLetter = i;
        } else if (firstFree < 0) {
            firstFree = i;
        }
    }

    if (add) {
        if (ourLetter < 0 && firstFree >= 0) {
            const std::wstring name(1, (wchar_t)(L'a' + firstFree));
            RegWrite(HKEY_CURRENT_USER, key, name.c_str(), exeName.c_str());
            ourLetter = firstFree;
        }
        if (ourLetter >= 0) {
            std::wstring mru;
            RegRead(HKEY_CURRENT_USER, key, L"MRUList", mru);
            const wchar_t c = (wchar_t)(L'a' + ourLetter);
            if (mru.find(c) == std::wstring::npos) mru.push_back(c);
            RegWrite(HKEY_CURRENT_USER, key, L"MRUList", mru.c_str());
        }
    } else {
        if (ourLetter >= 0) {
            const std::wstring name(1, (wchar_t)(L'a' + ourLetter));
            RegDeleteValueSafe(HKEY_CURRENT_USER, key, name.c_str());
        }
        RebuildMru(HKEY_CURRENT_USER, key);
    }
}

// ------------------------------------------------------------ association

bool IsExtensionActive(const std::wstring& ext) {
    std::wstring value;
    if (RegRead(HKEY_CURRENT_USER, std::wstring(kClasses) + ext, nullptr, value) &&
        _wcsicmp(value.c_str(), kProgId) == 0) {
        return true;
    }
    if (HasValue(HKEY_CURRENT_USER, std::wstring(kClasses) + ext + L"\\OpenWithProgids", kProgId)) {
        return true;
    }
    if (HasValue(HKEY_CURRENT_USER, std::wstring(kFileExts) + ext + L"\\OpenWithProgids", kProgId)) {
        return true;
    }
    return false;
}

void SetExtension(const std::wstring& ext, bool on) {
    RegisterProgram();
    RegisterCapabilities();

    const std::wstring cls = std::wstring(kClasses) + ext;
    const std::wstring fileExts = std::wstring(kFileExts) + ext;

    if (on) {
        BackupDefaultValue(ext);
        RegWrite(HKEY_CURRENT_USER, cls, nullptr, kProgId);
        RegWriteEmpty(HKEY_CURRENT_USER, cls + L"\\OpenWithProgids", kProgId);
        RegWriteEmpty(HKEY_CURRENT_USER, fileExts + L"\\OpenWithProgids", kProgId);
        SetOpenWithList(fileExts, true);
    } else {
        RestoreDefaultValue(ext);
        RegDeleteValueSafe(HKEY_CURRENT_USER, cls + L"\\OpenWithProgids", kProgId);
        RegDeleteValueSafe(HKEY_CURRENT_USER, fileExts + L"\\OpenWithProgids", kProgId);
        SetOpenWithList(fileExts, false);
    }
}

} // namespace

size_t AssocFamilyCount() { return COUNT(kFamilies); }

const wchar_t* AssocFamilyLabel(size_t index) {
    return index < COUNT(kFamilies) ? kFamilies[index].label : L"";
}

bool AssocFamilyActive(size_t index) {
    if (index >= COUNT(kFamilies)) return false;
    const Family& f = kFamilies[index];
    for (size_t i = 0; i < f.count; ++i) {
        if (!IsExtensionActive(f.exts[i])) return false;
    }
    return f.count > 0;
}

bool AssocSetFamily(size_t index, bool on) {
    if (index >= COUNT(kFamilies)) return false;
    const Family& f = kFamilies[index];
    for (size_t i = 0; i < f.count; ++i) SetExtension(f.exts[i], on);
    return true;
}

void AssocSetAll(bool on) {
    for (size_t i = 0; i < COUNT(kFamilies); ++i) AssocSetFamily(i, on);
    if (!on) {
        // Nothing references the ProgID any more: drop the program registration
        // too, so neither "Open with" nor Settings lists JPEGViewNX.
        RegDeleteTreeW(HKEY_CURRENT_USER, (std::wstring(kClasses) + kProgId).c_str());
        RegDeleteTreeW(HKEY_CURRENT_USER,
                       (std::wstring(kClasses) + L"Applications\\" + ExeName()).c_str());
        RegDeleteValueSafe(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"JPEGViewNX");
        RegDeleteTreeW(HKEY_CURRENT_USER, kBackupKey);
    }
}

void AssocNotifyShell() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

bool AssocOpenDefaultApps() {
    // Windows 11 understands registeredAppUser and jumps straight to our app.
    HINSTANCE r = ShellExecuteW(nullptr, L"open",
                                L"ms-settings:defaultapps?registeredAppUser=JPEGViewNX",
                                nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) {
        r = ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr,
                          SW_SHOWNORMAL);
    }
    if ((INT_PTR)r <= 32) {
        r = ShellExecuteW(nullptr, L"open", L"control.exe", L"/name Microsoft.DefaultPrograms",
                          nullptr, SW_SHOWNORMAL);
    }
    return (INT_PTR)r > 32;
}

std::wstring AssocProgId() { return kProgId; }
