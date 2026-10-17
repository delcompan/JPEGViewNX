// JPEGViewNX - settings (ini compatible with JPEGView where it overlaps)
#pragma once

#include <windows.h>
#include <string>

enum TransparencyMode {
    TM_SOLID   = 0,   // flatten onto TransparencyColor (JPEGView's only behaviour)
    TM_CHECKER = 1,   // checkerboard behind the image (fix for the reported defect)
    TM_NONE    = 2    // show the window background through the image
};

struct Settings {
    COLORREF         backgroundColor   = RGB(0, 0, 0);
    COLORREF         transparencyColor = RGB(0, 0, 0);
    TransparencyMode transparencyMode  = TM_CHECKER;   // NEW default: checkerboard
    int              checkerCellSize   = 8;            // device pixels per cell
    COLORREF         checkerColor1     = RGB(64, 64, 64);
    COLORREF         checkerColor2     = RGB(96, 96, 96);
    bool             smoothResampling  = true;
    bool             showFileName      = true;
    int              language          = 0;   // 0 = 中文, 1 = English
    int              rawMode           = 0;   // 0 auto (preview first), 1 full, 2 preview only
    RECT             windowRect        = { 120, 80, 1180, 780 };
    bool             windowMaximized   = false;

    // iniDir: directory that contains (or will contain) JPEGViewNX.ini
    void Load(const std::wstring& iniDir);
    void Save(const std::wstring& iniDir) const;

    const wchar_t* ModeName() const;
    const wchar_t* RawModeName() const;
    void CycleMode();
};
