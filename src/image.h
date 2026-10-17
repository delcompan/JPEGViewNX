// JPEGViewNX - image decoding: WIC for everything it knows, plus built-in
// decoders for TGA / PCX / PNM and embedded-JPEG extraction for camera RAW.
// Alpha is preserved as straight BGRA; animated GIFs are composed frame by frame.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

// RawMode values (settings)
enum RawMode {
    RAW_AUTO    = 0,   // embedded JPEG preview first, full RAW as fallback
    RAW_FULL    = 1,   // full RAW through WIC first, preview as fallback
    RAW_PREVIEW = 2    // embedded JPEG preview only
};

struct ImageDoc {
    std::vector<BYTE> bgra;      // current frame, straight BGRA, top-down
    int width  = 0;
    int height = 0;
    bool hasAlpha = false;
    UINT orientation = 1;        // EXIF orientation that was applied on load
    std::wstring path;
    std::wstring format;         // "PNG", "JPEG", "TGA", "RAW", ...
    std::wstring note;           // e.g. "embedded JPEG preview"

    // Animation (animated GIF)
    std::vector<std::vector<BYTE>> frames;   // composed canvases
    std::vector<int> delays;                 // per frame, milliseconds
    int frameIndex = 0;

    bool animated() const { return frames.size() > 1; }
    int  frameCount() const { return frames.size() > 1 ? (int)frames.size() : 1; }
    int  CurrentDelay() const {
        if (delays.empty()) return 100;
        return delays[(size_t)(frameIndex < 0 ? 0 : frameIndex) % delays.size()];
    }
    bool AdvanceFrame();

    void Clear();
    bool Load(const std::wstring& file, std::wstring* err, int rawMode = RAW_AUTO);

    void Rotate90(bool clockwise);
    void Mirror(bool horizontal);
    void ScanAlpha();
};
