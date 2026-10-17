// JPEGViewNX - a from-scratch C++ reimplementation of JPEGView's viewing
// behaviour, with one deliberate fix: images that carry an alpha channel are
// composited at *paint* time over a configurable background, so the
// transparency background can be a checkerboard (or any colour) that can be
// switched at runtime instead of being destructively flattened on load.
//
// Original JPEGView problem (verified against JPEGView.exe, see analysis/):
//   * CPictureView::OnPaint fills the client with BackgroundColor only.
//   * The GDI+ decode path fills a 32bpp DIB with TransparencyColor and then
//     GdipDrawImageRectI()s the image onto it, i.e. alpha is baked away once.
//   -> no checkerboard is possible at all.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "image.h"
#include "assocdlg.h"
#include "i18n.h"
#include "resource.h"
#include "settings.h"
#include "shellassoc.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {

const wchar_t* kAppTitle = L"JPEGViewNX";
const wchar_t* kClassName = L"JPEGViewNX.MainWindow";

enum : UINT {
    IDM_OPEN = 100,
    IDM_NEXT,
    IDM_PREV,
    IDM_FIT,
    IDM_ACTUAL,
    IDM_ZOOM_IN,
    IDM_ZOOM_OUT,
    IDM_ROT_CW,
    IDM_ROT_CCW,
    IDM_MIRROR_H,
    IDM_MIRROR_V,
    IDM_RELOAD,
    IDM_FULLSCREEN,
    IDM_INFO,
    IDM_HELP,
    IDM_ABOUT,
    IDM_SMOOTH,
    IDM_SHOWNAME,
    IDM_ANIM_PAUSE,
    IDM_EXIT,
    IDM_TRANS_SOLID = 200,
    IDM_TRANS_CHECKER,
    IDM_TRANS_NONE,
    IDM_CELL_4,
    IDM_CELL_8,
    IDM_CELL_16,
    IDM_CELL_32,
    IDM_COLORS,

    // file associations, language and overlay toggles
    IDM_SHOWSTATUS,
    IDM_ASSOC_DIALOG = 300,
    IDM_ASSOC_ALL = 360,
    IDM_ASSOC_NONE,
    IDM_ASSOC_SETTINGS,
    IDM_LANG_ZH = 380,
    IDM_LANG_EN
};

struct CheckerTile {
    HDC     dc   = nullptr;
    HBITMAP bmp  = nullptr;
    void*   bits = nullptr;
    int     size = 0;
    int     cell = 0;
    COLORREF c1  = 0;
    COLORREF c2  = 0;
};

struct Viewer {
    HWND      hwnd      = nullptr;
    Settings  cfg;
    ImageDoc  img;
    std::wstring iniDir;

    double zoom  = 1.0;
    bool   fitMode = false;
    double panX  = 0.0;
    double panY  = 0.0;

    bool   fullscreen = false;
    WINDOWPLACEMENT prevPlacement{};

    HDC     memDC  = nullptr;
    HBITMAP memBmp = nullptr;
    void*   memBits = nullptr;
    int     memW = 0, memH = 0;

    CheckerTile checker;

    std::wstring folder;
    std::vector<std::wstring> files;
    int fileIndex = -1;

    bool   dragging = false;
    POINT  dragStart{};
    double dragPanX = 0.0, dragPanY = 0.0;

    bool   showStatus = true;
    bool   animPaused = false;
};

Viewer* g_v = nullptr;

// ---------------------------------------------------------------- utilities

bool IsImageExt(std::wstring ext) {
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
    static const wchar_t* kExts[] = {
        // WIC / built-in
        L".jpg", L".jpeg", L".jpe", L".jfif", L".exif", L".png", L".bmp", L".dib",
        L".rle", L".gif", L".tif", L".tiff", L".ico", L".cur", L".webp", L".wdp",
        L".hdp", L".mdp", L".jxr", L".dds", L".jxl", L".heic", L".heif", L".hif",
        L".avci", L".avcs", L".avif", L".avifs",
        // built-in extra decoders
        L".tga", L".pcx", L".pnm", L".ppm", L".pgm", L".pbm", L".pxm",
        // camera RAW (embedded JPEG preview / WIC RAW codec)
        L".3fr", L".ari", L".arw", L".bay", L".cap", L".cr2", L".cr3", L".crw",
        L".dcs", L".dcr", L".dng", L".drf", L".eip", L".erf", L".fff", L".gpr",
        L".iiq", L".k25", L".kdc", L".mef", L".mos", L".mrw", L".nef", L".nrw",
        L".orf", L".ori", L".pef", L".ptx", L".pxn", L".raf", L".raw", L".rw2",
        L".rwl", L".sr2", L".srf", L".srw", L".x3f"
    };
    for (const wchar_t* e : kExts) if (ext == e) return true;
    return false;
}

int NaturalCompare(const std::wstring& a, const std::wstring& b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (iswdigit(a[i]) && iswdigit(b[j])) {
            size_t i2 = i, j2 = j;
            while (i2 < a.size() && iswdigit(a[i2])) ++i2;
            while (j2 < b.size() && iswdigit(b[j2])) ++j2;
            std::wstring na = a.substr(i, i2 - i);
            std::wstring nb = b.substr(j, j2 - j);
            size_t za = na.find_first_not_of(L'0');
            size_t zb = nb.find_first_not_of(L'0');
            na = (za == std::wstring::npos) ? L"0" : na.substr(za);
            nb = (zb == std::wstring::npos) ? L"0" : nb.substr(zb);
            if (na.size() != nb.size()) return na.size() < nb.size() ? -1 : 1;
            if (na != nb) return na < nb ? -1 : 1;
            i = i2; j = j2;
        } else {
            wchar_t ca = (wchar_t)towlower(a[i]);
            wchar_t cb = (wchar_t)towlower(b[j]);
            if (ca != cb) return ca < cb ? -1 : 1;
            ++i; ++j;
        }
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}


// Comparison key for paths: separators and case differ between the command line
// (forward slashes) and directory_iterator (backslashes).
std::wstring PathKey(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if (c == L'/') c = L'\\';
        else c = (wchar_t)towlower(c);
    }
    return out;
}

std::wstring FileNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

void FreeBackBuffer(Viewer& v) {
    if (v.memDC) { DeleteDC(v.memDC); v.memDC = nullptr; }
    if (v.memBmp) { DeleteObject(v.memBmp); v.memBmp = nullptr; }
    v.memBits = nullptr;
    v.memW = v.memH = 0;
}

void EnsureBackBuffer(Viewer& v, int w, int h) {
    if (w <= 0 || h <= 0) { FreeBackBuffer(v); return; }
    if (v.memDC && v.memW == w && v.memH == h) return;
    FreeBackBuffer(v);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    v.memBmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &v.memBits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!v.memBmp || !v.memBits) return;

    v.memDC = CreateCompatibleDC(nullptr);
    if (!v.memDC) return;
    SelectObject(v.memDC, v.memBmp);

    v.memW = w;
    v.memH = h;
}

void FreeCheckerTile(Viewer& v) {
    if (v.checker.dc) { DeleteDC(v.checker.dc); v.checker.dc = nullptr; }
    if (v.checker.bmp) { DeleteObject(v.checker.bmp); v.checker.bmp = nullptr; }
    v.checker.bits = nullptr;
    v.checker.size = 0;
    v.checker.cell = 0;
}

// A tile several cells wide, anchored at the client origin. The checkerboard
// therefore stays put while the image is panned (like every sane viewer does),
// and it never scales with the zoom factor.
void EnsureCheckerTile(Viewer& v) {
    const int cell = std::max(2, v.cfg.checkerCellSize);
    if (v.checker.dc && v.checker.cell == cell &&
        v.checker.c1 == v.cfg.checkerColor1 && v.checker.c2 == v.cfg.checkerColor2) {
        return;
    }
    FreeCheckerTile(v);

    const int unit = cell * 2;
    const int reps = std::max(1, 256 / unit);
    const int size = unit * reps;
    if (size <= 0) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = -size;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    HBITMAP bmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &v.checker.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bmp) return;

    HDC dc = CreateCompatibleDC(nullptr);
    SelectObject(dc, bmp);   // keep the tile selected: BitBlt reads this DC

    HBRUSH b1 = CreateSolidBrush(v.cfg.checkerColor1);
    HBRUSH b2 = CreateSolidBrush(v.cfg.checkerColor2);
    const int blocks = reps * 2;
    for (int by = 0; by < blocks; ++by) {
        for (int bx = 0; bx < blocks; ++bx) {
            RECT r{ bx * cell, by * cell, (bx + 1) * cell, (by + 1) * cell };
            FillRect(dc, &r, ((bx + by) & 1) ? b1 : b2);
        }
    }
    DeleteObject(b1);
    DeleteObject(b2);
    GdiFlush();

    v.checker.dc   = dc;
    v.checker.bmp  = bmp;
    v.checker.size = size;
    v.checker.cell = cell;
    v.checker.c1   = v.cfg.checkerColor1;
    v.checker.c2   = v.cfg.checkerColor2;
}

void FillCheckerboard(Viewer& v, HDC dc, const RECT& clip) {
    if (!v.checker.dc || v.checker.size <= 0) return;
    const int size = v.checker.size;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
    // Anchored at (0,0) of the client area: the pattern does not move with pan.
    for (int y = 0; y < clip.bottom; y += size) {
        for (int x = 0; x < clip.right; x += size) {
            int w = std::min(size, (int)(clip.right - x));
            int h = std::min(size, (int)(clip.bottom - y));
            if (w <= 0 || h <= 0) continue;
            BitBlt(dc, x, y, w, h, v.checker.dc, 0, 0, SRCCOPY);
        }
    }
    RestoreDC(dc, saved);
}

RECT ClientRect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    return rc;
}

// Destination rectangle of the image in client coordinates.
RECT ComputeDest(const Viewer& v, const RECT& client) {
    RECT r{};
    if (v.img.width <= 0 || v.img.height <= 0) return r;
    const double dw = v.img.width * v.zoom;
    const double dh = v.img.height * v.zoom;
    const double dx = (client.right - dw) * 0.5 + v.panX;
    const double dy = (client.bottom - dh) * 0.5 + v.panY;
    r.left   = (LONG)std::llround(dx);
    r.top    = (LONG)std::llround(dy);
    r.right  = r.left + (LONG)std::llround(dw);
    r.bottom = r.top + (LONG)std::llround(dh);
    return r;
}

double FitZoom(const Viewer& v, const RECT& client) {
    if (v.img.width <= 0 || v.img.height <= 0) return 1.0;
    double fx = (double)client.right / v.img.width;
    double fy = (double)client.bottom / v.img.height;
    double f = std::min(fx, fy);
    if (f <= 0.0) f = 1.0;
    return f;
}

void ClampPan(Viewer& v) {
    RECT client = ClientRect(v.hwnd);
    const double dw = v.img.width * v.zoom;
    const double dh = v.img.height * v.zoom;
    if (dw <= client.right) v.panX = 0.0;
    else {
        const double half = (dw - client.right) * 0.5;
        v.panX = std::max(-half, std::min(half, v.panX));
    }
    if (dh <= client.bottom) v.panY = 0.0;
    else {
        const double half = (dh - client.bottom) * 0.5;
        v.panY = std::max(-half, std::min(half, v.panY));
    }
}

const wchar_t* ModeText(const Settings& cfg) {
    switch (cfg.transparencyMode) {
        case TM_SOLID: return T(S_MODE_SOLID);
        case TM_NONE:  return T(S_MODE_NONE);
        default:       return T(S_MODE_CHECKER);
    }
}

void UpdateTitle(Viewer& v) {
    if (!v.hwnd) return;
    std::wstring t = kAppTitle;
    t += L" - ";
    if (v.img.width > 0) t += FileNameOf(v.img.path);
    else                 t += T(S_NOIMAGE);
    if (v.img.width > 0) {
        wchar_t buf[160];
        swprintf_s(buf, L"  %dx%d  %d%%  %ls", v.img.width, v.img.height,
                   (int)(v.zoom * 100.0 + 0.5), v.img.format.c_str());
        t += buf;
        if (v.img.hasAlpha) {
            wchar_t ab[160];
            swprintf_s(ab, T(S_ALPHA_MODE), ModeText(v.cfg));
            t += L"  [";
            t += ab;
            t += L"]";
        }
        if (!v.img.note.empty()) {
            t += L"  [";
            t += v.img.note;
            t += L"]";
        }
        if (v.img.animated()) {
            wchar_t fr[64];
            wchar_t fb[96];
            swprintf_s(fr, T(S_FRAME), v.img.frameIndex + 1, v.img.frameCount());
            swprintf_s(fb, L"  [%ls%ls]", fr, v.animPaused ? T(S_PAUSED) : L"");
            t += fb;
        }
    }
    SetWindowTextW(v.hwnd, t.c_str());
}

void Invalidate(HWND hwnd) { InvalidateRect(hwnd, nullptr, FALSE); }

void SetZoomKeepAnchor(Viewer& v, double newZoom, POINT anchor) {
    if (v.img.width <= 0) return;
    newZoom = std::max(0.02, std::min(64.0, newZoom));
    RECT client = ClientRect(v.hwnd);
    RECT dest = ComputeDest(v, client);
    // Image coordinate currently under the anchor point.
    const double ix = (anchor.x - dest.left) / v.zoom;
    const double iy = (anchor.y - dest.top) / v.zoom;
    v.zoom = newZoom;
    v.fitMode = false;
    // Put that same image coordinate back under the anchor.
    v.panX = anchor.x - (client.right - v.img.width * newZoom) * 0.5 - ix * newZoom;
    v.panY = anchor.y - (client.bottom - v.img.height * newZoom) * 0.5 - iy * newZoom;
    ClampPan(v);
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

void DoFit(Viewer& v) {
    if (v.img.width <= 0) return;
    RECT client = ClientRect(v.hwnd);
    v.zoom = FitZoom(v, client);
    v.fitMode = true;
    v.panX = v.panY = 0.0;
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

void DoActualSize(Viewer& v) {
    if (v.img.width <= 0) return;
    v.zoom = 1.0;
    v.fitMode = false;
    v.panX = v.panY = 0.0;
    ClampPan(v);
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

void ToggleFitActual(Viewer& v) {
    if (v.fitMode) DoActualSize(v);
    else DoFit(v);
}

// -------------------------------------------------------------- compositing
//
// The image is composited in software: every destination pixel samples the
// source (nearest above 100 %, bilinear between 50 % and 100 %, box average
// below 50 %) and the result is blended over whatever the background pass
// already wrote into the back buffer. The alpha is still intact here, so the
// transparency background can be anything - checkerboard included - and it can
// change at runtime. JPEGView instead bakes TransparencyColor in once, during
// decoding, which makes a checkerboard impossible.

struct AxisSample {
    int i0, i1;   // bilinear taps
    int f;        // weight of i1, in 1/256
    int s0, s1;   // box-filter range
};

void BuildAxis(int first, int count, int destStart, int destLen, int srcLen,
               double ratio, bool area, std::vector<AxisSample>& out) {
    out.resize((size_t)std::max(0, count));
    for (int i = 0; i < count; ++i) {
        const int d = first + i;
        const double s = (double)(d - destStart) * ratio;
        AxisSample& a = out[(size_t)i];

        int b0 = (int)std::floor(s);
        int b1 = (int)std::ceil(s + ratio);
        if (b1 <= b0) b1 = b0 + 1;
        if (b0 < 0) b0 = 0;
        if (b0 > srcLen) b0 = srcLen;
        if (b1 < b0 + 1) b1 = b0 + 1;
        if (b1 > srcLen) b1 = srcLen;
        a.s0 = b0;
        a.s1 = b1;

        const double c = s + ratio * 0.5 - 0.5;
        int i0 = (int)std::floor(c);
        double f = c - i0;
        int i1 = i0 + 1;
        if (i0 < 0) { i0 = 0; i1 = 0; f = 0.0; }
        if (i1 > srcLen - 1) { i1 = srcLen - 1; if (i0 > i1) i0 = i1; }
        a.i0 = std::max(0, std::min(srcLen - 1, i0));
        a.i1 = std::max(0, std::min(srcLen - 1, i1));
        int w = (int)(f * 256.0 + 0.5);
        a.f = std::max(0, std::min(256, w));
    }
}

void RenderImage(Viewer& v, BYTE* dstBuf, int bufW, int bufH, const RECT& clip) {
    const ImageDoc& img = v.img;
    const int iw = img.width, ih = img.height;
    RECT client{ 0, 0, bufW, bufH };
    const RECT dest = ComputeDest(v, client);
    const int dw = dest.right - dest.left;
    const int dh = dest.bottom - dest.top;
    if (iw <= 0 || ih <= 0 || dw <= 0 || dh <= 0) return;

    const double rx = (double)iw / dw;
    const double ry = (double)ih / dh;
    const bool area = v.zoom < 0.5;
    const bool useNearest = (v.zoom >= 1.0) || (v.zoom >= 0.5 && !v.cfg.smoothResampling);

    const int cw = clip.right - clip.left;
    const int ch = clip.bottom - clip.top;
    if (cw <= 0 || ch <= 0) return;

    std::vector<AxisSample> xs, ys;
    BuildAxis(clip.left, cw, dest.left, dw, iw, rx, area, xs);
    BuildAxis(clip.top, ch, dest.top, dh, ih, ry, area, ys);

    const BYTE* src = img.bgra.data();
    for (int yy = clip.top; yy < clip.bottom; ++yy) {
        BYTE* row = dstBuf + (size_t)yy * bufW * 4;
        const AxisSample& ay = ys[(size_t)(yy - clip.top)];
        for (int xx = clip.left; xx < clip.right; ++xx) {
            const AxisSample& ax = xs[(size_t)(xx - clip.left)];
            BYTE* d = row + (size_t)xx * 4;
            uint32_t pa = 0, pr = 0, pg = 0, pb = 0;

            if (area) {
                uint32_t n = 0, sa = 0, sr = 0, sg = 0, sb = 0;
                for (int sy = ay.s0; sy < ay.s1; ++sy) {
                    const BYTE* srow = src + (size_t)sy * iw * 4;
                    for (int sx = ax.s0; sx < ax.s1; ++sx) {
                        const BYTE* p = srow + (size_t)sx * 4;
                        const uint32_t a = p[3];
                        sa += a;
                        sb += (uint32_t)p[0] * a;
                        sg += (uint32_t)p[1] * a;
                        sr += (uint32_t)p[2] * a;
                        ++n;
                    }
                }
                if (n == 0) continue;
                pa = sa / n;
                if (pa) {
                    pb = (sb / n) / 255;
                    pg = (sg / n) / 255;
                    pr = (sr / n) / 255;
                }
            } else if (useNearest) {
                const BYTE* p = src + ((size_t)ay.i0 * iw + ax.i0) * 4;
                pa = p[3];
                if (pa) {
                    pb = (uint32_t)p[0] * pa / 255;
                    pg = (uint32_t)p[1] * pa / 255;
                    pr = (uint32_t)p[2] * pa / 255;
                }
            } else {
                const uint32_t fx = (uint32_t)ax.f, fy = (uint32_t)ay.f;
                const uint32_t w00 = (256 - fx) * (256 - fy);
                const uint32_t w01 = fx * (256 - fy);
                const uint32_t w10 = (256 - fx) * fy;
                const uint32_t w11 = fx * fy;
                const BYTE* p00 = src + ((size_t)ay.i0 * iw + ax.i0) * 4;
                const BYTE* p01 = src + ((size_t)ay.i0 * iw + ax.i1) * 4;
                const BYTE* p10 = src + ((size_t)ay.i1 * iw + ax.i0) * 4;
                const BYTE* p11 = src + ((size_t)ay.i1 * iw + ax.i1) * 4;
                const uint32_t a00 = p00[3], a01 = p01[3], a10 = p10[3], a11 = p11[3];
                const uint32_t sumA = a00 * w00 + a01 * w01 + a10 * w10 + a11 * w11;
                pa = sumA >> 16;
                if (pa) {
                    const uint32_t sumB = (uint32_t)p00[0] * a00 * w00 + (uint32_t)p01[0] * a01 * w01 +
                                          (uint32_t)p10[0] * a10 * w10 + (uint32_t)p11[0] * a11 * w11;
                    const uint32_t sumG = (uint32_t)p00[1] * a00 * w00 + (uint32_t)p01[1] * a01 * w01 +
                                          (uint32_t)p10[1] * a10 * w10 + (uint32_t)p11[1] * a11 * w11;
                    const uint32_t sumR = (uint32_t)p00[2] * a00 * w00 + (uint32_t)p01[2] * a01 * w01 +
                                          (uint32_t)p10[2] * a10 * w10 + (uint32_t)p11[2] * a11 * w11;
                    pb = (sumB >> 16) / 255;
                    pg = (sumG >> 16) / 255;
                    pr = (sumR >> 16) / 255;
                }
            }

            if (pa == 0) continue;
            const uint32_t inv = 255 - pa;
            d[0] = (BYTE)(pb + (uint32_t)d[0] * inv / 255);
            d[1] = (BYTE)(pg + (uint32_t)d[1] * inv / 255);
            d[2] = (BYTE)(pr + (uint32_t)d[2] * inv / 255);
            d[3] = 255;
        }
    }
}

// ---------------------------------------------------------------- painting

void PaintViewer(Viewer& v, HDC target) {
    RECT client = ClientRect(v.hwnd);
    if (client.right <= 0 || client.bottom <= 0) return;

    EnsureBackBuffer(v, client.right, client.bottom);
    if (!v.memDC || !v.memBits) return;

    const int saved = SaveDC(v.memDC);
    SetBkMode(v.memDC, TRANSPARENT);

    // 1. window background (BackgroundColor, exactly like JPEGView)
    HBRUSH bg = CreateSolidBrush(v.cfg.backgroundColor);
    FillRect(v.memDC, &client, bg);
    DeleteObject(bg);

    RECT dest{};
    if (v.img.width > 0) {
        dest = ComputeDest(v, client);
        RECT clip;
        IntersectRect(&clip, &dest, &client);

        // 2a. transparency background, painted *under* the image.
        //     JPEGView does this once at load time with one solid colour; here
        //     it is a real, switchable background.
        if (v.img.hasAlpha && v.cfg.transparencyMode != TM_NONE && !IsRectEmpty(&clip)) {
            if (v.cfg.transparencyMode == TM_SOLID) {
                HBRUSH tb = CreateSolidBrush(v.cfg.transparencyColor);
                FillRect(v.memDC, &clip, tb);
                DeleteObject(tb);
            } else {
                EnsureCheckerTile(v);
                FillCheckerboard(v, v.memDC, clip);
            }
        }

        // 2b. alpha-composite the scaled image on top of it.
        GdiFlush();   // make the GDI background visible to the software renderer
        if (!IsRectEmpty(&clip)) {
            RenderImage(v, (BYTE*)v.memBits, client.right, client.bottom, clip);
        }
    }

    // 3. GDI overlay: file name line and status line, each independently toggleable.
    SetBkMode(v.memDC, TRANSPARENT);
    int lineY = 8;
    auto drawLine = [&](const std::wstring& text) {
        if (text.empty()) return;
        RECT tr{ 10, lineY, client.right - 10, lineY + 22 };
        RECT shadow = tr;
        OffsetRect(&shadow, 1, 1);
        SetTextColor(v.memDC, RGB(0, 0, 0));
        DrawTextW(v.memDC, text.c_str(), (int)text.size(), &shadow,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SetTextColor(v.memDC, RGB(240, 240, 240));
        DrawTextW(v.memDC, text.c_str(), (int)text.size(), &tr,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        lineY += 20;
    };

    if (v.img.width <= 0) {
        drawLine(T(S_OVERLAY_HINT));
    } else {
        if (v.cfg.showFileName) drawLine(FileNameOf(v.img.path));
        if (v.showStatus) {
            std::wstring mode;
            if (v.img.hasAlpha) {
                wchar_t ab[160];
                swprintf_s(ab, T(S_ALPHA_MODE), ModeText(v.cfg));
                mode = ab;
            } else {
                mode = T(S_OPAQUE);
            }
            wchar_t sub[320];
            swprintf_s(sub, L"%dx%d  %ls  %d%%  %ls", v.img.width, v.img.height,
                       v.img.format.c_str(), (int)(v.zoom * 100.0 + 0.5), mode.c_str());
            std::wstring line = sub;
            if (!v.img.note.empty()) {
                line += L"   ";
                line += v.img.note;
            }
            if (v.img.animated()) {
                wchar_t fr[64];
                wchar_t fb[96];
                swprintf_s(fr, T(S_FRAME), v.img.frameIndex + 1, v.img.frameCount());
                swprintf_s(fb, L"   %ls%ls", fr, v.animPaused ? T(S_PAUSED) : L"");
                line += fb;
            }
            drawLine(line);
        }
    }

    RestoreDC(v.memDC, saved);
    BitBlt(target, 0, 0, client.right, client.bottom, v.memDC, 0, 0, SRCCOPY);
}

// -------------------------------------------------------------- file handling

void ScanFolder(Viewer& v, const std::wstring& file) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p(file);
    if (p.is_relative()) {
        std::error_code ae;
        fs::path abs = fs::absolute(p, ae);
        if (!ae) p = abs;
    }
    v.folder = p.parent_path().wstring();
    v.files.clear();

    if (!v.folder.empty()) {
        fs::directory_iterator it(v.folder, fs::directory_options::skip_permission_denied, ec);
        for (; !ec && it != fs::directory_iterator(); it.increment(ec)) {
            const fs::directory_entry& e = *it;
            std::error_code ec2;
            if (!e.is_regular_file(ec2)) continue;
            std::wstring ext = e.path().extension().wstring();
            if (!IsImageExt(ext)) continue;
            v.files.push_back(e.path().wstring());
        }
    }

    const std::wstring targetKey = PathKey(p.wstring());
    const std::wstring targetName = PathKey(p.filename().wstring());

    auto findCurrent = [&]() -> int {
        if (!targetKey.empty()) {
            for (size_t i = 0; i < v.files.size(); ++i)
                if (PathKey(v.files[i]) == targetKey) return (int)i;
        }
        for (size_t i = 0; i < v.files.size(); ++i)
            if (PathKey(fs::path(v.files[i]).filename().wstring()) == targetName) return (int)i;
        return -1;
    };

    if (v.files.empty()) v.files.push_back(p.wstring());
    std::sort(v.files.begin(), v.files.end(),
              [](const std::wstring& a, const std::wstring& b) { return NaturalCompare(a, b) < 0; });

    v.fileIndex = findCurrent();
    if (v.fileIndex < 0) {
        v.files.push_back(p.wstring());
        std::sort(v.files.begin(), v.files.end(),
                  [](const std::wstring& a, const std::wstring& b) { return NaturalCompare(a, b) < 0; });
        v.fileIndex = findCurrent();
    }
    if (v.fileIndex < 0) v.fileIndex = 0;
}

constexpr UINT_PTR kAnimTimer = 1;

void RestartAnimation(Viewer& v) {
    if (!v.hwnd) return;
    KillTimer(v.hwnd, kAnimTimer);
    if (v.img.animated() && !v.animPaused) {
        UINT d = (UINT)std::max(20, v.img.CurrentDelay());
        SetTimer(v.hwnd, kAnimTimer, d, nullptr);
    }
}

void ToggleAnimationPause(Viewer& v) {
    v.animPaused = !v.animPaused;
    RestartAnimation(v);
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

bool LoadFileInternal(Viewer& v, const std::wstring& file, bool rescan) {
    ImageDoc doc;
    std::wstring err;
    if (!doc.Load(file, &err, v.cfg.rawMode)) {
        std::wstring tpl = T(S_ERR_BODY);
        std::vector<wchar_t> buf(tpl.size() + file.size() + err.size() + 64);
        swprintf_s(buf.data(), buf.size(), tpl.c_str(), file.c_str(), err.c_str());
        MessageBoxW(v.hwnd, buf.data(), T(S_ERR_TITLE), MB_ICONWARNING);
        return false;
    }

    v.img = std::move(doc);

    RECT client = ClientRect(v.hwnd);
    double f = FitZoom(v, client);
    if (f < 1.0) { v.zoom = f; v.fitMode = true; }
    else         { v.zoom = 1.0; v.fitMode = false; }
    v.panX = v.panY = 0.0;

    if (rescan) ScanFolder(v, file);

    v.animPaused = false;
    RestartAnimation(v);

    UpdateTitle(v);
    Invalidate(v.hwnd);
    return true;
}

bool LoadFile(Viewer& v, const std::wstring& file) {
    return LoadFileInternal(v, file, true);
}

void OpenDialog(Viewer& v) {
    wchar_t path[MAX_PATH * 2] = {};
    if (!v.img.path.empty()) {
        wcsncpy_s(path, v.img.path.c_str(), _TRUNCATE);
    }
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = v.hwnd;
    static const wchar_t kFilter[] =
        L"Images (*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.tga;*.ico;*.jxr;*.dds;*.jxl;*.heic;*.avif)\0"
        L"*.jpg;*.jpeg;*.jpe;*.jfif;*.png;*.bmp;*.dib;*.gif;*.tif;*.tiff;*.webp;*.tga;*.ico;*.cur;*.wdp;*.jxr;*.dds;*.jxl;*.heic;*.heif;*.avif\0"
        L"Camera RAW (*.cr2;*.cr3;*.nef;*.arw;*.dng;*.orf;*.rw2;*.raf;*.pef;*.srw;*.rwl;*.raw...)\0"
        L"*.cr2;*.cr3;*.crw;*.nef;*.nrw;*.arw;*.srf;*.sr2;*.srw;*.orf;*.raf;*.rw2;*.rwl;*.pef;*.dng;*.raw;*.3fr;*.fff;*.mrw;*.kdc;*.dcr;*.x3f;*.iiq;*.gpr\0"
        L"Netpbm / PCX (*.pcx;*.pnm;*.ppm;*.pgm;*.pbm)\0*.pcx;*.pnm;*.ppm;*.pgm;*.pbm\0"
        L"All files (*.*)\0*.*\0";
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = path;
    ofn.nMaxFile  = MAX_PATH * 2;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) LoadFile(v, path);
}

void StepFile(Viewer& v, int delta) {
    if (v.files.empty()) return;
    int n = (int)v.files.size();
    int idx = v.fileIndex;
    if (idx < 0) idx = 0;
    else idx = (idx + delta % n + n) % n;
    if (idx < 0 || idx >= n) idx = 0;
    v.fileIndex = idx;
    LoadFileInternal(v, v.files[idx], false);
}

void ShowInfo(Viewer& v) {
    const std::wstring folder = v.folder.empty() ? std::wstring(L"-") : v.folder;
    const wchar_t* alpha = v.img.hasAlpha ? T(S_YES) : T(S_NO);
    const std::wstring tpl = T(S_INFO_BODY);
    const size_t cap = tpl.size() + v.img.path.size() + folder.size() +
                       v.img.format.size() + 256;
    std::vector<wchar_t> buf(cap);
    swprintf_s(buf.data(), buf.size(), tpl.c_str(),
               v.img.path.c_str(), v.img.width, v.img.height, v.img.format.c_str(), alpha,
               (int)(v.zoom * 100.0 + 0.5), ModeText(v.cfg), v.cfg.checkerCellSize,
               v.fileIndex >= 0 ? v.fileIndex + 1 : 0, (int)v.files.size(), folder.c_str());
    MessageBoxW(v.hwnd, buf.data(), T(S_INFO_TITLE), MB_ICONINFORMATION);
}

void ShowHelp(Viewer& v) {
    MessageBoxW(v.hwnd, T(S_HELP_BODY), T(S_HELP_TITLE), MB_ICONINFORMATION);
}

void SaveSettings(Viewer& v) {
    if (!v.fullscreen) {
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if (GetWindowPlacement(v.hwnd, &wp)) {
            v.cfg.windowMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
            v.cfg.windowRect = wp.rcNormalPosition;
        }
    }
    v.cfg.Save(v.iniDir);
}

void ToggleFullscreen(Viewer& v) {
    if (!v.fullscreen) {
        v.prevPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(v.hwnd, &v.prevPlacement);
        HMONITOR mon = MonitorFromWindow(v.hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(mon, &mi)) return;
        SetWindowLongPtrW(v.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(v.hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        v.fullscreen = true;
    } else {
        SetWindowLongPtrW(v.hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(v.hwnd, &v.prevPlacement);
        SetWindowPos(v.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        v.fullscreen = false;
    }
    if (v.fitMode) DoFit(v);
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

void CycleTransparency(Viewer& v, int dir) {
    int m = ((int)v.cfg.transparencyMode + dir) % 3;
    if (m < 0) m += 3;
    v.cfg.transparencyMode = (TransparencyMode)m;
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

void AdjustCell(Viewer& v, int dir) {
    static const int kSizes[] = { 2, 4, 6, 8, 12, 16, 24, 32, 64 };
    const int n = (int)(sizeof(kSizes) / sizeof(kSizes[0]));
    int cur = v.cfg.checkerCellSize;
    int best = 0;
    for (int i = 0; i < n; ++i) if (std::abs(kSizes[i] - cur) < std::abs(kSizes[best] - cur)) best = i;
    best += dir;
    if (best < 0) best = 0;
    if (best >= n) best = n - 1;
    v.cfg.checkerCellSize = kSizes[best];
    UpdateTitle(v);
    Invalidate(v.hwnd);
}

// --------------------------------------------------------------- menu/command

void ShowContextMenu(Viewer& v, POINT screenPt) {
    HMENU m = CreatePopupMenu();

    auto add = [&](UINT id, StrId text, const wchar_t* accel, bool checked) {
        std::wstring label = T(text);
        if (accel) {
            label += L"\t";
            label += accel;
        }
        UINT flags = MF_STRING;
        if (checked) flags |= MF_CHECKED;
        AppendMenuW(m, flags, id, label.c_str());
    };

    add(IDM_OPEN, S_MENU_OPEN, L"Ctrl+O", false);
    add(IDM_NEXT, S_MENU_NEXT, L"PgDn", false);
    add(IDM_PREV, S_MENU_PREV, L"PgUp", false);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(IDM_FIT, S_MENU_FIT, L"Return", false);
    add(IDM_ACTUAL, S_MENU_ACTUAL, L"Space", false);
    add(IDM_ZOOM_IN, S_MENU_ZOOM_IN, L"+", false);
    add(IDM_ZOOM_OUT, S_MENU_ZOOM_OUT, L"-", false);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    HMENU trans = CreatePopupMenu();
    AppendMenuW(trans, MF_STRING, IDM_TRANS_CHECKER, T(S_TRANS_CHECKER));
    AppendMenuW(trans, MF_STRING, IDM_TRANS_SOLID, T(S_TRANS_SOLID));
    AppendMenuW(trans, MF_STRING, IDM_TRANS_NONE, T(S_TRANS_NONE));
    UINT checkedId = IDM_TRANS_CHECKER;
    if (v.cfg.transparencyMode == TM_SOLID) checkedId = IDM_TRANS_SOLID;
    else if (v.cfg.transparencyMode == TM_NONE) checkedId = IDM_TRANS_NONE;
    CheckMenuRadioItem(trans, IDM_TRANS_CHECKER, IDM_TRANS_NONE, checkedId, MF_BYCOMMAND);
    AppendMenuW(trans, MF_SEPARATOR, 0, nullptr);

    auto cellLabel = [](int px) {
        wchar_t label[64];
        swprintf_s(label, T(S_CELL_LABEL), px);
        return std::wstring(label);
    };
    AppendMenuW(trans, MF_STRING | MF_DISABLED, 0, cellLabel(v.cfg.checkerCellSize).c_str());
    const int kCells[] = { 4, 8, 16, 32 };
    const UINT kCellIds[] = { IDM_CELL_4, IDM_CELL_8, IDM_CELL_16, IDM_CELL_32 };
    for (int k = 0; k < 4; ++k) {
        std::wstring label = cellLabel(kCells[k]);
        AppendMenuW(trans, MF_STRING | (v.cfg.checkerCellSize == kCells[k] ? MF_CHECKED : 0),
                    kCellIds[k], label.c_str());
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)trans, T(S_TRANS_MENU));

    add(IDM_SMOOTH, S_MENU_SMOOTH, L"F3", v.cfg.smoothResampling);
    add(IDM_SHOWNAME, S_MENU_SHOWNAME, L"I", v.cfg.showFileName);
    add(IDM_SHOWSTATUS, S_MENU_STATUS, L"T", v.showStatus);
    add(IDM_ANIM_PAUSE, S_MENU_PAUSE, L"P", v.animPaused);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(IDM_ROT_CW, S_MENU_ROT_CW, L"R", false);
    add(IDM_ROT_CCW, S_MENU_ROT_CCW, L"Shift+R", false);
    add(IDM_MIRROR_H, S_MENU_MIRROR_H, L"H", false);
    add(IDM_MIRROR_V, S_MENU_MIRROR_V, L"V", false);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(IDM_RELOAD, S_MENU_RELOAD, L"F5", false);
    add(IDM_FULLSCREEN, S_MENU_FULLSCREEN, L"F11", false);
    add(IDM_INFO, S_MENU_INFO, L"F2", false);
    add(IDM_HELP, S_MENU_HELP, L"F1", false);

    HMENU lang = CreatePopupMenu();
    AppendMenuW(lang, MF_STRING | (I18nGet() == LANG_ZH ? MF_CHECKED : 0),
                IDM_LANG_ZH, T(S_LANG_ZH));
    AppendMenuW(lang, MF_STRING | (I18nGet() == LANG_EN ? MF_CHECKED : 0),
                IDM_LANG_EN, T(S_LANG_EN));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)lang, T(S_MENU_LANGUAGE));

    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_ASSOC_DIALOG, T(S_MENU_ASSOC));
    add(IDM_ASSOC_SETTINGS, S_ASSOC_SETTINGS, nullptr, false);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(IDM_EXIT, S_MENU_EXIT, L"Esc", false);

    TrackPopupMenu(m, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, v.hwnd, nullptr);
    DestroyMenu(m);
}

void HandleCommand(Viewer& v, UINT id) {
    switch (id) {
        case IDM_OPEN:       OpenDialog(v); break;
        case IDM_NEXT:       StepFile(v, +1); break;
        case IDM_PREV:       StepFile(v, -1); break;
        case IDM_FIT:        DoFit(v); break;
        case IDM_ACTUAL:     DoActualSize(v); break;
        case IDM_ZOOM_IN:
        case IDM_ZOOM_OUT: {
            RECT client = ClientRect(v.hwnd);
            POINT c{ client.right / 2, client.bottom / 2 };
            SetZoomKeepAnchor(v, v.zoom * (id == IDM_ZOOM_IN ? 1.25 : 1.0 / 1.25), c);
            break;
        }
        case IDM_ROT_CW:     v.img.Rotate90(true);  DoFit(v); break;
        case IDM_ROT_CCW:    v.img.Rotate90(false); DoFit(v); break;
        case IDM_MIRROR_H:   v.img.Mirror(true);   Invalidate(v.hwnd); break;
        case IDM_MIRROR_V:   v.img.Mirror(false);  Invalidate(v.hwnd); break;
        case IDM_RELOAD:
            if (!v.img.path.empty()) LoadFileInternal(v, v.img.path, false);
            break;
        case IDM_FULLSCREEN: ToggleFullscreen(v); break;
        case IDM_INFO:       ShowInfo(v); break;
        case IDM_HELP:       ShowHelp(v); break;
        case IDM_ANIM_PAUSE: ToggleAnimationPause(v); break;
        case IDM_SMOOTH:     v.cfg.smoothResampling = !v.cfg.smoothResampling; Invalidate(v.hwnd); break;
        case IDM_SHOWNAME:   v.cfg.showFileName = !v.cfg.showFileName; Invalidate(v.hwnd); break;
        case IDM_SHOWSTATUS: v.showStatus = !v.showStatus; Invalidate(v.hwnd); break;
        case IDM_ASSOC_DIALOG: {
            AssocDialogResult r = ShowAssocDialog(v.hwnd);
            if (r.changed && r.anyEnabled) {
                const std::wstring tpl = T(S_ASSOC_HINT_BODY);
                std::vector<wchar_t> buf(tpl.size() + 64);
                swprintf_s(buf.data(), buf.size(), tpl.c_str(), r.changedCount);
                if (MessageBoxW(v.hwnd, buf.data(), T(S_ASSOC_TITLE),
                                MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                    AssocOpenDefaultApps();
                }
            }
            break;
        }
        case IDM_ASSOC_ALL:
            AssocSetAll(true);
            AssocNotifyShell();
            break;
        case IDM_ASSOC_NONE:
            AssocSetAll(false);
            AssocNotifyShell();
            break;
        case IDM_ASSOC_SETTINGS:
            AssocOpenDefaultApps();
            break;
        case IDM_LANG_ZH:
        case IDM_LANG_EN: {
            I18nSet(id == IDM_LANG_EN ? LANG_EN : LANG_ZH);
            v.cfg.language = (int)I18nGet();
            v.cfg.Save(v.iniDir);
            UpdateTitle(v);
            Invalidate(v.hwnd);
            break;
        }
        case IDM_EXIT:
            if (v.fullscreen) ToggleFullscreen(v);
            else PostMessageW(v.hwnd, WM_CLOSE, 0, 0);
            break;
        case IDM_TRANS_SOLID:   v.cfg.transparencyMode = TM_SOLID;   UpdateTitle(v); Invalidate(v.hwnd); break;
        case IDM_TRANS_CHECKER: v.cfg.transparencyMode = TM_CHECKER; UpdateTitle(v); Invalidate(v.hwnd); break;
        case IDM_TRANS_NONE:    v.cfg.transparencyMode = TM_NONE;    UpdateTitle(v); Invalidate(v.hwnd); break;
        case IDM_CELL_4:  v.cfg.checkerCellSize = 4;  Invalidate(v.hwnd); break;
        case IDM_CELL_8:  v.cfg.checkerCellSize = 8;  Invalidate(v.hwnd); break;
        case IDM_CELL_16: v.cfg.checkerCellSize = 16; Invalidate(v.hwnd); break;
        case IDM_CELL_32: v.cfg.checkerCellSize = 32; Invalidate(v.hwnd); break;
        default: break;
    }
}

// -------------------------------------------------------------------- window

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Viewer* v = g_v;
    if (!v) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_CREATE:
            v->hwnd = hwnd;
            DragAcceptFiles(hwnd, TRUE);
            return 0;

        case WM_ERASEBKGND:
            return 1;   // fully double buffered

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            // Optional checkerboard tuning from the ini is applied lazily.
            PaintViewer(*v, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            if (v->fitMode) {
                RECT client{ 0, 0, LOWORD(lp), HIWORD(lp) };
                if (v->img.width > 0) {
                    v->zoom = FitZoom(*v, client);
                    v->panX = v->panY = 0.0;
                    UpdateTitle(*v);
                }
            } else {
                ClampPan(*v);
            }
            Invalidate(hwnd);
            return 0;

        case WM_MOUSEWHEEL: {
            if (v->img.width <= 0) return 0;
            int notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            if (notches == 0) notches = (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1);
            const bool slow = (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) != 0;
            double factor = std::pow(slow ? 1.05 : 1.20, (double)notches);
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            SetZoomKeepAnchor(*v, v->zoom * factor, pt);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            v->dragging = true;
            v->dragStart = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            v->dragPanX = v->panX;
            v->dragPanY = v->panY;
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!v->dragging) return 0;
            const int dx = GET_X_LPARAM(lp) - v->dragStart.x;
            const int dy = GET_Y_LPARAM(lp) - v->dragStart.y;
            v->panX = v->dragPanX + dx;
            v->panY = v->dragPanY + dy;
            ClampPan(*v);
            Invalidate(hwnd);
            return 0;
        }

        case WM_LBUTTONUP:
            if (v->dragging) { v->dragging = false; ReleaseCapture(); }
            return 0;

        case WM_LBUTTONDBLCLK:
            ToggleFullscreen(*v);
            return 0;

        case WM_RBUTTONUP: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ClientToScreen(hwnd, &pt);
            ShowContextMenu(*v, pt);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP drop = (HDROP)wp;
            wchar_t path[MAX_PATH * 2] = {};
            if (DragQueryFileW(drop, 0, path, MAX_PATH * 2)) LoadFile(*v, path);
            DragFinish(drop);
            SetForegroundWindow(hwnd);
            return 0;
        }

        case WM_TIMER:
            if (wp == kAnimTimer) {
                if (v->img.AdvanceFrame()) {
                    UpdateTitle(*v);
                    Invalidate(hwnd);
                    RestartAnimation(*v);
                } else {
                    KillTimer(hwnd, kAnimTimer);
                }
                return 0;
            }
            return 0;

        case WM_KEYDOWN: {
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            RECT client = ClientRect(hwnd);
            const int panStep = 32;

            if (ctrl && (wp == '1' || wp == '2' || wp == '3')) {
                v->cfg.transparencyMode = (wp == '1') ? TM_CHECKER : (wp == '2' ? TM_SOLID : TM_NONE);
                UpdateTitle(*v); Invalidate(hwnd);
                return 0;
            }
            if (ctrl && wp == 'O') { OpenDialog(*v); return 0; }

            switch (wp) {
                case VK_ESCAPE:
                    if (v->fullscreen) ToggleFullscreen(*v);
                    else PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    return 0;
                case VK_F11: ToggleFullscreen(*v); return 0;
                case VK_F1:  ShowHelp(*v); return 0;
                case VK_F2:  ShowInfo(*v); return 0;
                case VK_F3:
                    v->cfg.smoothResampling = !v->cfg.smoothResampling;
                    Invalidate(hwnd);
                    return 0;
                case VK_F5:
                    if (!v->img.path.empty()) LoadFileInternal(*v, v->img.path, false);
                    return 0;
                case VK_RETURN: DoFit(*v); return 0;
                case VK_SPACE:  ToggleFitActual(*v); return 0;
                case VK_ADD:
                case VK_OEM_PLUS:
                    SetZoomKeepAnchor(*v, v->zoom * 1.25, { client.right / 2, client.bottom / 2 });
                    return 0;
                case VK_SUBTRACT:
                case VK_OEM_MINUS:
                    SetZoomKeepAnchor(*v, v->zoom / 1.25, { client.right / 2, client.bottom / 2 });
                    return 0;
                case VK_NEXT:  StepFile(*v, +1); return 0;
                case VK_PRIOR: StepFile(*v, -1); return 0;
                case VK_HOME:
                    if (!v->files.empty()) { v->fileIndex = 0; LoadFileInternal(*v, v->files[0], false); }
                    return 0;
                case VK_END:
                    if (!v->files.empty()) {
                        v->fileIndex = (int)v->files.size() - 1;
                        LoadFileInternal(*v, v->files.back(), false);
                    }
                    return 0;
                case VK_RIGHT:
                    if (shift) { v->panX -= panStep; ClampPan(*v); Invalidate(hwnd); }
                    else StepFile(*v, +1);
                    return 0;
                case VK_LEFT:
                    if (shift) { v->panX += panStep; ClampPan(*v); Invalidate(hwnd); }
                    else StepFile(*v, -1);
                    return 0;
                case VK_UP:
                    if (shift) { v->panY += panStep; ClampPan(*v); Invalidate(hwnd); }
                    return 0;
                case VK_DOWN:
                    if (shift) { v->panY -= panStep; ClampPan(*v); Invalidate(hwnd); }
                    return 0;
                case 'C': CycleTransparency(*v, +1); return 0;
                case 'R':
                    v->img.Rotate90(!shift);
                    DoFit(*v);
                    return 0;
                case 'H': v->img.Mirror(true);  Invalidate(hwnd); return 0;
                case 'V': v->img.Mirror(false); Invalidate(hwnd); return 0;
                case 'P': ToggleAnimationPause(*v); return 0;
                case 'T': v->showStatus = !v->showStatus; Invalidate(hwnd); return 0;
                case 'I': v->cfg.showFileName = !v->cfg.showFileName; Invalidate(hwnd); return 0;
                case VK_OEM_4: AdjustCell(*v, -1); return 0;   // [
                case VK_OEM_6: AdjustCell(*v, +1); return 0;   // ]
                default: break;
            }
            return 0;
        }

        case WM_COMMAND:
            HandleCommand(*v, LOWORD(wp));
            return 0;

        case WM_CLOSE:
            SaveSettings(*v);
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kAnimTimer);
            FreeBackBuffer(*v);
            FreeCheckerTile(*v);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    std::wstring s = buf;
    size_t p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    using Fn = BOOL(WINAPI*)(HANDLE);
    Fn fn = (Fn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (fn) fn((HANDLE)(LONG_PTR)-4 /* PER_MONITOR_AWARE_V2 */);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    EnableDpiAwareness();

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        const std::wstring arg1 = argv[1];
        if (_wcsicmp(arg1.c_str(), L"--associate-all") == 0 ||
            _wcsicmp(arg1.c_str(), L"--unassociate-all") == 0) {
            AssocSetAll(_wcsicmp(arg1.c_str(), L"--associate-all") == 0);
            AssocNotifyShell();
            LocalFree(argv);
            if (SUCCEEDED(hrCom)) CoUninitialize();
            return 0;
        }
    }

    Viewer viewer;
    viewer.iniDir = ExeDir();
    viewer.cfg.Load(viewer.iniDir);
    I18nSet((Lang)viewer.cfg.language);
    g_v = &viewer;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    wc.hIcon         = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 0, 0,
                                         LR_DEFAULTSIZE | LR_SHARED);
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm       = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON),
                                         GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (!wc.hIconSm) wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;

    RECT wr = viewer.cfg.windowRect;
    HWND hwnd = CreateWindowExW(0, kClassName, kAppTitle, WS_OVERLAPPEDWINDOW,
                                wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top,
                                nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;

    if (viewer.cfg.windowMaximized) ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    else ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    if (argv) {
        if (argc > 1) LoadFile(viewer, argv[1]);
        LocalFree(argv);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_v = nullptr;
    viewer.img.Clear();
    FreeBackBuffer(viewer);
    FreeCheckerTile(viewer);
    if (SUCCEEDED(hrCom)) CoUninitialize();
    return (int)msg.wParam;
}
