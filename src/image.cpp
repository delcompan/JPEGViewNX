#include "image.h"

#include "formats.h"

#include <propvarutil.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace {

std::wstring FormatName(const GUID& g) {
    if (IsEqualGUID(g, GUID_ContainerFormatPng))       return L"PNG";
    if (IsEqualGUID(g, GUID_ContainerFormatJpeg))      return L"JPEG";
    if (IsEqualGUID(g, GUID_ContainerFormatBmp))       return L"BMP";
    if (IsEqualGUID(g, GUID_ContainerFormatGif))       return L"GIF";
    if (IsEqualGUID(g, GUID_ContainerFormatTiff))      return L"TIFF";
    if (IsEqualGUID(g, GUID_ContainerFormatIco))       return L"ICO";
    if (IsEqualGUID(g, GUID_ContainerFormatWmp))       return L"JPEG XR";
    if (IsEqualGUID(g, GUID_ContainerFormatHeif))      return L"HEIF";
    if (IsEqualGUID(g, GUID_ContainerFormatWebp))      return L"WebP";
    if (IsEqualGUID(g, GUID_ContainerFormatDds))       return L"DDS";
    return L"image";
}

bool TransformForOrientation(UINT o, WICBitmapTransformOptions* t) {
    switch (o) {
        case 2: *t = WICBitmapTransformFlipHorizontal; return true;
        case 3: *t = WICBitmapTransformRotate180; return true;
        case 4: *t = WICBitmapTransformFlipVertical; return true;
        case 5: *t = (WICBitmapTransformOptions)(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal); return true;
        case 6: *t = WICBitmapTransformRotate90; return true;
        case 7: *t = (WICBitmapTransformOptions)(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal); return true;
        case 8: *t = WICBitmapTransformRotate270; return true;
        default: return false;
    }
}

UINT ReadOrientation(IWICBitmapFrameDecode* frame) {
    ComPtr<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(&reader)) || !reader) return 1;

    UINT result = 1;
    const wchar_t* queries[] = {
        L"System.Photo.Orientation",
        L"/app1/ifd/{ushort=274}",
        L"/ifd/{ushort=274}",
        L"/eXIf/ifd/{ushort=274}",
    };
    for (const wchar_t* q : queries) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(reader->GetMetadataByName(q, &v))) {
            if (v.vt == VT_UI2 && v.uiVal >= 1 && v.uiVal <= 8)      result = v.uiVal;
            else if (v.vt == VT_UI4 && v.ulVal >= 1 && v.ulVal <= 8) result = (UINT)v.ulVal;
            PropVariantClear(&v);
            if (result != 1) break;
        } else {
            PropVariantClear(&v);
        }
    }
    return result;
}

bool QueryUInt(IWICMetadataQueryReader* reader, const wchar_t* name, UINT* out) {
    if (!reader) return false;
    PROPVARIANT v;
    PropVariantInit(&v);
    bool ok = false;
    if (SUCCEEDED(reader->GetMetadataByName(name, &v))) {
        if (v.vt == VT_UI2) { *out = v.uiVal; ok = true; }
        else if (v.vt == VT_UI4) { *out = v.ulVal; ok = true; }
        else if (v.vt == VT_UI1) { *out = v.bVal; ok = true; }
    }
    PropVariantClear(&v);
    return ok;
}

std::wstring LowerExt(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = path.substr(dot);
    for (wchar_t& c : ext) c = (wchar_t)towlower(c);
    return ext;
}

bool ReadWholeFile(const std::wstring& path, std::vector<BYTE>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > 1500ll * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    out.resize((size_t)size.QuadPart);
    DWORD read = 0;
    const bool ok = ReadFile(h, out.data(), (DWORD)out.size(), &read, nullptr) && read == out.size();
    CloseHandle(h);
    if (!ok) out.clear();
    return ok;
}

// Draw `src` (sw x sh, straight BGRA) over the canvas at (left, top), source-over.
void DrawOver(BYTE* canvas, int cw, int ch, const BYTE* src, int sw, int sh, int left, int top) {
    for (int y = 0; y < sh; ++y) {
        const int cy = top + y;
        if (cy < 0 || cy >= ch) continue;
        for (int x = 0; x < sw; ++x) {
            const int cx = left + x;
            if (cx < 0 || cx >= cw) continue;
            const BYTE* s = src + ((size_t)y * sw + x) * 4;
            BYTE* d = canvas + ((size_t)cy * cw + cx) * 4;
            const uint32_t a = s[3];
            if (a == 0) continue;
            if (a == 255) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; continue; }
            const uint32_t inv = 255 - a;
            d[0] = (BYTE)(s[0] + (uint32_t)d[0] * inv / 255);
            d[1] = (BYTE)(s[1] + (uint32_t)d[1] * inv / 255);
            d[2] = (BYTE)(s[2] + (uint32_t)d[2] * inv / 255);
            d[3] = (BYTE)(a + (uint32_t)d[3] * inv / 255);
        }
    }
}

void ClearRegion(BYTE* canvas, int cw, int ch, int left, int top, int w, int h) {
    for (int y = 0; y < h; ++y) {
        const int cy = top + y;
        if (cy < 0 || cy >= ch) continue;
        for (int x = 0; x < w; ++x) {
            const int cx = left + x;
            if (cx < 0 || cx >= cw) continue;
            memset(canvas + ((size_t)cy * cw + cx) * 4, 0, 4);
        }
    }
}

// Decode every GIF frame, apply disposal rules and store composed canvases.
bool LoadGifAnimation(IWICImagingFactory* factory, IWICBitmapDecoder* decoder, ImageDoc* doc) {
    UINT count = 0;
    if (FAILED(decoder->GetFrameCount(&count)) || count <= 1) return false;

    ComPtr<IWICBitmapFrameDecode> first;
    if (FAILED(decoder->GetFrame(0, &first))) return false;
    UINT cw = 0, ch = 0;
    if (FAILED(first->GetSize(&cw, &ch))) return false;
    {
        ComPtr<IWICMetadataQueryReader> reader;
        UINT v = 0;
        if (SUCCEEDED(first->GetMetadataQueryReader(&reader))) {
            if (QueryUInt(reader.Get(), L"/logscrdesc/Width", &v) && v > 0) cw = v;
            if (QueryUInt(reader.Get(), L"/logscrdesc/Height", &v) && v > 0) ch = v;
        }
    }
    if (cw == 0 || ch == 0 || cw > 16384 || ch > 16384) return false;

    const size_t canvasBytes = (size_t)cw * ch * 4;
    size_t maxFrames = std::min<size_t>(count, 400);
    const size_t budget = 512ull * 1024 * 1024;
    if (canvasBytes * maxFrames > budget) maxFrames = std::max<size_t>(1, budget / canvasBytes);
    if (maxFrames < 2) return false;

    std::vector<BYTE> canvas(canvasBytes, 0);
    std::vector<BYTE> saved;

    for (size_t i = 0; i < maxFrames; ++i) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame((UINT)i, &frame))) break;

        UINT fw = 0, fh = 0;
        if (FAILED(frame->GetSize(&fw, &fh))) break;

        UINT left = 0, top = 0, delay = 10, disposal = 0;
        {
            ComPtr<IWICMetadataQueryReader> reader;
            if (SUCCEEDED(frame->GetMetadataQueryReader(&reader))) {
                if (fw != cw || fh != ch) {
                    QueryUInt(reader.Get(), L"/imgdesc/Left", &left);
                    QueryUInt(reader.Get(), L"/imgdesc/Top", &top);
                }
                if (!QueryUInt(reader.Get(), L"/grctlext/Delay", &delay)) delay = 10;
                QueryUInt(reader.Get(), L"/grctlext/Disposal", &disposal);
            }
        }

        std::vector<BYTE> pixels;
        int pw = 0, ph = 0;
        std::wstring err;
        if (!WicSourceToBgra(factory, frame.Get(), pixels, pw, ph, &err)) break;
        if (pw <= 0 || ph <= 0) break;

        if (disposal == 3) saved = canvas;
        DrawOver(canvas.data(), (int)cw, (int)ch, pixels.data(), pw, ph, (int)left, (int)top);

        doc->frames.push_back(canvas);
        int ms = (int)delay * 10;
        if (ms < 20) ms = 100;   // browsers treat 0-1 hundredths as 100 ms
        doc->delays.push_back(ms);

        if (disposal == 2) {
            const int w = (pw == (int)cw && ph == (int)ch) ? (int)cw : pw;
            const int h = (pw == (int)cw && ph == (int)ch) ? (int)ch : ph;
            ClearRegion(canvas.data(), (int)cw, (int)ch, (int)left, (int)top, w, h);
        } else if (disposal == 3 && !saved.empty()) {
            canvas = saved;
        }
    }

    if (doc->frames.size() < 2) return false;

    doc->width = (int)cw;
    doc->height = (int)ch;
    doc->frameIndex = 0;
    doc->bgra = doc->frames[0];
    doc->format = L"GIF";
    return true;
}

} // namespace

void ImageDoc::Clear() {
    bgra.clear();
    bgra.shrink_to_fit();
    frames.clear();
    frames.shrink_to_fit();
    delays.clear();
    delays.shrink_to_fit();
    width = height = 0;
    hasAlpha = false;
    orientation = 1;
    frameIndex = 0;
    path.clear();
    format.clear();
    note.clear();
}

void ImageDoc::ScanAlpha() {
    auto anyAlpha = [](const std::vector<BYTE>& buf) -> bool {
        if (buf.size() < 4) return false;
        const size_t n = buf.size() / 4;
        for (size_t i = 0; i < n; ++i) {
            if (buf[i * 4 + 3] != 0xFF) return true;
        }
        return false;
    };

    if (frames.size() > 1) {
        for (const auto& f : frames) {
            if (anyAlpha(f)) { hasAlpha = true; return; }
        }
        hasAlpha = false;
        return;
    }
    hasAlpha = anyAlpha(bgra);
}

bool ImageDoc::AdvanceFrame() {
    if (frames.size() <= 1) return false;
    frameIndex = (frameIndex + 1) % (int)frames.size();
    bgra = frames[(size_t)frameIndex];
    return true;
}

bool ImageDoc::Load(const std::wstring& file, std::wstring* err, int rawMode) {
    Clear();

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        if (err) *err = L"No WIC imaging factory.";
        return false;
    }

    const std::wstring ext = LowerExt(file);
    const bool raw = IsRawExtension(ext);

    // Prefer the embedded preview for RAW by default: fast, and it is what the
    // original JPEGView does.
    if (raw && rawMode != RAW_FULL) {
        std::vector<BYTE> data;
        if (ReadWholeFile(file, data)) {
            if (raw && IsExtraExtension(ext)) {
                // not possible for raw extensions, kept for clarity
            }
            std::wstring tmp;
            if (DecodeEmbeddedJpeg(factory.Get(), data, bgra, width, height, &tmp)) {
                format = L"RAW";
                note = L"embedded JPEG preview";
                ScanAlpha();
                path = file;
                return true;
            }
        }
        if (rawMode == RAW_PREVIEW) {
            if (err) *err = L"No embedded JPEG preview in this RAW file.\n"
                            L"Install the Microsoft Raw Image Extension, or set RawMode=full.";
            return false;
        }
        // fall through to the full RAW decode below
    }

    // 1. WIC (JPEG, PNG, BMP, GIF, TIFF, ICO, WebP, HEIF/AVIF, JXL, DDS, RAW ...)
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr)) {
        GUID container{};
        decoder->GetContainerFormat(&container);
        format = FormatName(container);

        // Container GUIDs are shared by several families; name them by extension.
        if (format == L"HEIF") {
            if (ext == L".avif" || ext == L".avifs")      format = L"AVIF";
            else if (ext == L".heic" || ext == L".heics") format = L"HEIC";
        }
        if (format == L"image") {
            if (ext == L".jxl")                 format = L"JXL";
            else if (IsRawExtension(ext))       format = L"RAW";
        }

        if (IsEqualGUID(container, GUID_ContainerFormatGif)) {
            if (LoadGifAnimation(factory.Get(), decoder.Get(), this)) {
                ScanAlpha();
                path = file;
                return true;
            }
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr)) {
            orientation = ReadOrientation(frame.Get());
            ComPtr<IWICBitmapSource> src = frame;
            if (orientation >= 2 && orientation <= 8) {
                WICBitmapTransformOptions opt{};
                if (TransformForOrientation(orientation, &opt)) {
                    ComPtr<IWICBitmapFlipRotator> rot;
                    if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rot)) &&
                        SUCCEEDED(rot->Initialize(frame.Get(), opt))) {
                        src = rot;
                    } else {
                        orientation = 1;
                    }
                } else {
                    orientation = 1;
                }
            }
            std::wstring tmp;
            if (WicSourceToBgra(factory.Get(), src.Get(), bgra, width, height, &tmp)) {
                if (raw) note = L"full RAW decode";
                ScanAlpha();
                path = file;
                return true;
            }
        }
        Clear();
    }

    // 2. Built-in decoders for formats WIC does not handle.
    std::vector<BYTE> data;
    if (!ReadWholeFile(file, data)) {
        if (err) *err = L"Cannot open the file (unsupported or not an image).";
        return false;
    }

    bool ok = false;
    if (ext == L".tga") { ok = DecodeTga(data, bgra, width, height, err); if (ok) format = L"TGA"; }
    else if (ext == L".pcx") { ok = DecodePcx(data, bgra, width, height, err); if (ok) format = L"PCX"; }
    else if (IsExtraExtension(ext)) { ok = DecodePnm(data, bgra, width, height, err); if (ok) format = L"PNM"; }

    if (!ok) {
        ok = DecodeEmbeddedJpeg(factory.Get(), data, bgra, width, height, err);
        if (ok) {
            format = raw ? L"RAW" : L"JPEG";
            note = L"embedded JPEG preview";
        }
    }

    if (!ok) {
        bgra.clear();
        width = height = 0;
        if (err && err->empty()) *err = L"Unsupported or corrupt image file.";
        return false;
    }

    ScanAlpha();
    path = file;
    return true;
}

namespace {

void RotateBuffer(const std::vector<BYTE>& src, int w, int h, bool clockwise, std::vector<BYTE>& dst) {
    const int nw = h;
    const int nh = w;
    dst.resize((size_t)nw * nh * 4);
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            int sx, sy;
            if (clockwise) { sx = y;             sy = h - 1 - x; }
            else           { sx = w - 1 - y;     sy = x; }
            memcpy(dst.data() + ((size_t)y * nw + x) * 4,
                   src.data() + ((size_t)sy * w + sx) * 4, 4);
        }
    }
}

void MirrorBuffer(std::vector<BYTE>& buf, int w, int h, bool horizontal) {
    if (horizontal) {
        for (int y = 0; y < h; ++y) {
            BYTE* row = buf.data() + (size_t)y * w * 4;
            for (int x = 0; x < w / 2; ++x) {
                BYTE* a = row + (size_t)x * 4;
                BYTE* b = row + (size_t)(w - 1 - x) * 4;
                for (int k = 0; k < 4; ++k) std::swap(a[k], b[k]);
            }
        }
    } else {
        std::vector<BYTE> row((size_t)w * 4);
        for (int y = 0; y < h / 2; ++y) {
            BYTE* a = buf.data() + (size_t)y * w * 4;
            BYTE* b = buf.data() + (size_t)(h - 1 - y) * w * 4;
            memcpy(row.data(), a, row.size());
            memcpy(a, b, row.size());
            memcpy(b, row.data(), row.size());
        }
    }
}

} // namespace

void ImageDoc::Rotate90(bool clockwise) {
    if (width <= 0 || height <= 0) return;
    std::vector<BYTE> rotated;
    RotateBuffer(bgra, width, height, clockwise, rotated);
    bgra.swap(rotated);

    for (auto& f : frames) {
        std::vector<BYTE> tmp;
        RotateBuffer(f, width, height, clockwise, tmp);
        f.swap(tmp);
    }
    const int nw = height;
    const int nh = width;
    width = nw;
    height = nh;
}

void ImageDoc::Mirror(bool horizontal) {
    if (width <= 0 || height <= 0) return;
    MirrorBuffer(bgra, width, height, horizontal);
    for (auto& f : frames) MirrorBuffer(f, width, height, horizontal);
}
