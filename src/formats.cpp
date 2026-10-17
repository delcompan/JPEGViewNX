#include "formats.h"

#include <wrl/client.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwctype>

using Microsoft::WRL::ComPtr;

namespace {

bool Fail(std::wstring* err, const wchar_t* text) {
    if (err) *err = text;
    return false;
}

inline void PutBgra(std::vector<BYTE>& out, int width, int x, int y, BYTE b, BYTE g, BYTE r, BYTE a) {
    BYTE* p = out.data() + ((size_t)y * width + x) * 4;
    p[0] = b; p[1] = g; p[2] = r; p[3] = a;
}

} // namespace

// ---------------------------------------------------------------- WIC helper

bool WicSourceToBgra(IWICImagingFactory* factory, IWICBitmapSource* src,
                     std::vector<BYTE>& out, int& w, int& h, std::wstring* err) {
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) return Fail(err, L"Cannot create a WIC format converter.");
    if (FAILED(conv->Initialize(src, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                                nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return Fail(err, L"Cannot convert the image to 32bpp BGRA.");
    }
    UINT uw = 0, uh = 0;
    conv->GetSize(&uw, &uh);
    if (uw == 0 || uh == 0 || uw > 32768 || uh > 32768) return Fail(err, L"Unsupported image dimensions.");
    out.resize((size_t)uw * uh * 4);
    if (FAILED(conv->CopyPixels(nullptr, uw * 4, (UINT)out.size(), out.data()))) {
        out.clear();
        return Fail(err, L"Cannot read the image pixels.");
    }
    w = (int)uw;
    h = (int)uh;
    return true;
}

// ---------------------------------------------------------------------- TGA

bool DecodeTga(const std::vector<BYTE>& d, std::vector<BYTE>& out, int& w, int& h, std::wstring* err) {
    if (d.size() < 18) return Fail(err, L"TGA file is truncated.");

    const BYTE idLength    = d[0];
    const BYTE colorMap    = d[1];
    const BYTE imageType   = d[2];
    const WORD cmapLength  = (WORD)(d[5] | (d[6] << 8));
    const BYTE cmapDepth   = d[7];
    const WORD width       = (WORD)(d[12] | (d[13] << 8));
    const WORD height      = (WORD)(d[14] | (d[15] << 8));
    const BYTE depth       = d[16];
    const BYTE descriptor  = d[17];

    const bool rle  = (imageType == 10 || imageType == 11);
    const bool gray = (imageType == 3 || imageType == 11);
    if (imageType != 2 && imageType != 3 && imageType != 10 && imageType != 11) {
        return Fail(err, L"Unsupported TGA type (only true-color and grayscale are handled).");
    }
    if (gray && depth != 8) return Fail(err, L"Unsupported grayscale TGA depth.");
    if (!gray && depth != 16 && depth != 24 && depth != 32) return Fail(err, L"Unsupported TGA depth.");
    if (width == 0 || height == 0) return Fail(err, L"Invalid TGA dimensions.");

    const int bpp = (depth + 7) / 8;
    size_t pos = 18 + (size_t)idLength;
    if (colorMap == 1) pos += (size_t)cmapLength * ((cmapDepth + 7) / 8);
    if (pos >= d.size()) return Fail(err, L"TGA file is truncated.");

    const int iw = width, ih = height;
    const bool topDown = (descriptor & 0x20) != 0;
    out.assign((size_t)iw * ih * 4, 0);

    auto store = [&](int x, int y, const BYTE* p) {
        const int oy = topDown ? y : (ih - 1 - y);
        if (oy < 0 || oy >= ih) return;
        if (gray) {
            PutBgra(out, iw, x, oy, p[0], p[0], p[0], 255);
        } else if (depth == 24) {
            PutBgra(out, iw, x, oy, p[0], p[1], p[2], 255);
        } else if (depth == 32) {
            PutBgra(out, iw, x, oy, p[0], p[1], p[2], p[3]);
        } else {
            const WORD v = (WORD)(p[0] | (p[1] << 8));
            PutBgra(out, iw, x, oy,
                    (BYTE)((v & 0x1F) << 3),
                    (BYTE)(((v >> 5) & 0x1F) << 3),
                    (BYTE)(((v >> 10) & 0x1F) << 3), 255);
        }
    };

    int x = 0, y = 0;
    auto advance = [&]() {
        if (++x >= iw) { x = 0; ++y; }
    };

    while (y < ih) {
        if (!rle) {
            if (pos + bpp > d.size()) return Fail(err, L"TGA pixel data is truncated.");
            store(x, y, d.data() + pos);
            pos += bpp;
            advance();
        } else {
            if (pos >= d.size()) return Fail(err, L"TGA RLE data is truncated.");
            const BYTE packet = d[pos++];
            const int count = (packet & 0x7F) + 1;
            if (packet & 0x80) {
                if (pos + bpp > d.size()) return Fail(err, L"TGA RLE data is truncated.");
                const BYTE* px = d.data() + pos;
                pos += bpp;
                for (int i = 0; i < count && y < ih; ++i) { store(x, y, px); advance(); }
            } else {
                for (int i = 0; i < count && y < ih; ++i) {
                    if (pos + bpp > d.size()) return Fail(err, L"TGA RLE data is truncated.");
                    store(x, y, d.data() + pos);
                    pos += bpp;
                    advance();
                }
            }
        }
    }
    w = iw;
    h = ih;
    return true;
}

// ---------------------------------------------------------------------- PCX

bool DecodePcx(const std::vector<BYTE>& d, std::vector<BYTE>& out, int& w, int& h, std::wstring* err) {
    if (d.size() < 128 || d[0] != 0x0A) return Fail(err, L"Not a PCX file.");
    if (d[2] != 1) return Fail(err, L"Unsupported PCX encoding.");
    const BYTE bits = d[3];
    if (bits != 8) return Fail(err, L"Unsupported PCX bit depth (only 8-bit is handled).");

    const int xmin = d[4] | (d[5] << 8);
    const int ymin = d[6] | (d[7] << 8);
    const int xmax = d[8] | (d[9] << 8);
    const int ymax = d[10] | (d[11] << 8);
    const int planes = d[65];
    const int bytesPerLine = d[66] | (d[67] << 8);

    const int iw = xmax - xmin + 1;
    const int ih = ymax - ymin + 1;
    if (iw <= 0 || ih <= 0 || iw > 32768 || ih > 32768) return Fail(err, L"Invalid PCX dimensions.");
    if (bytesPerLine <= 0 || planes < 1 || planes > 4) return Fail(err, L"Unsupported PCX layout.");
    if (planes != 1 && planes < 3) return Fail(err, L"Unsupported PCX plane count.");

    const size_t total = (size_t)bytesPerLine * planes * ih;
    std::vector<BYTE> raw;
    raw.reserve(total);
    size_t pos = 128;
    while (raw.size() < total && pos < d.size()) {
        const BYTE b = d[pos++];
        if (b >= 0xC0) {
            const int run = b & 0x3F;
            if (pos >= d.size()) break;
            const BYTE v = d[pos++];
            for (int i = 0; i < run && raw.size() < total; ++i) raw.push_back(v);
        } else {
            raw.push_back(b);
        }
    }
    if (raw.size() < total) return Fail(err, L"PCX pixel data is truncated.");

    out.assign((size_t)iw * ih * 4, 255);

    if (planes == 1) {
        if (d.size() < 769 || d[d.size() - 769] != 0x0C) return Fail(err, L"PCX palette is missing.");
        const BYTE* pal = d.data() + d.size() - 768;
        for (int y = 0; y < ih; ++y) {
            const BYTE* row = raw.data() + (size_t)y * bytesPerLine;
            for (int x = 0; x < iw; ++x) {
                const BYTE idx = row[x];
                PutBgra(out, iw, x, y, pal[idx * 3 + 2], pal[idx * 3 + 1], pal[idx * 3], 255);
            }
        }
    } else {
        for (int y = 0; y < ih; ++y) {
            const BYTE* row = raw.data() + (size_t)y * bytesPerLine * planes;
            for (int x = 0; x < iw; ++x) {
                const BYTE r = row[x];
                const BYTE g = row[bytesPerLine + x];
                const BYTE b = row[2 * bytesPerLine + x];
                const BYTE a = (planes >= 4) ? row[3 * bytesPerLine + x] : (BYTE)255;
                PutBgra(out, iw, x, y, b, g, r, a);
            }
        }
    }
    w = iw;
    h = ih;
    return true;
}

// ---------------------------------------------------------------------- PNM

namespace {

bool NextPnmToken(const std::vector<BYTE>& d, size_t& pos, std::string& token) {
    token.clear();
    while (pos < d.size()) {
        const BYTE c = d[pos];
        if (c == '#') {
            while (pos < d.size() && d[pos] != '\n' && d[pos] != '\r') ++pos;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
            ++pos;
        } else {
            break;
        }
    }
    if (pos >= d.size()) return false;
    while (pos < d.size()) {
        const BYTE c = d[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' || c == '#') break;
        token.push_back((char)c);
        ++pos;
    }
    // one separator after the token
    if (pos < d.size() && d[pos] == '\r' && pos + 1 < d.size() && d[pos + 1] == '\n') pos += 2;
    else if (pos < d.size()) ++pos;
    return !token.empty();
}

} // namespace

bool DecodePnm(const std::vector<BYTE>& d, std::vector<BYTE>& out, int& w, int& h, std::wstring* err) {
    if (d.size() < 4) return Fail(err, L"PNM file is truncated.");
    const std::string magic((const char*)d.data(), 2);
    if (magic != "P5" && magic != "P6") return Fail(err, L"Only P5/P6 PNM files are supported.");

    size_t pos = 2;
    std::string tok;
    if (!NextPnmToken(d, pos, tok)) return Fail(err, L"PNM header is truncated.");
    const int iw = std::atoi(tok.c_str());
    if (!NextPnmToken(d, pos, tok)) return Fail(err, L"PNM header is truncated.");
    const int ih = std::atoi(tok.c_str());
    if (!NextPnmToken(d, pos, tok)) return Fail(err, L"PNM header is truncated.");
    const int maxval = std::atoi(tok.c_str());
    if (iw <= 0 || ih <= 0 || maxval <= 0 || maxval > 65535) return Fail(err, L"Invalid PNM header.");

    const int channels = (magic == "P6") ? 3 : 1;
    const int sampleBytes = (maxval > 255) ? 2 : 1;
    const size_t need = (size_t)iw * ih * channels * sampleBytes;
    if (pos + need > d.size()) return Fail(err, L"PNM pixel data is truncated.");

    out.resize((size_t)iw * ih * 4);
    auto sample = [&](size_t off) -> int {
        if (sampleBytes == 1) return d[pos + off];
        return (d[pos + off] << 8) | d[pos + off + 1];
    };
    for (int y = 0; y < ih; ++y) {
        for (int x = 0; x < iw; ++x) {
            const size_t base = ((size_t)y * iw + x) * channels * sampleBytes;
            if (channels == 1) {
                const int v = sample(base) * 255 / maxval;
                PutBgra(out, iw, x, y, (BYTE)v, (BYTE)v, (BYTE)v, 255);
            } else {
                const int r = sample(base) * 255 / maxval;
                const int g = sample(base + sampleBytes) * 255 / maxval;
                const int b = sample(base + 2 * sampleBytes) * 255 / maxval;
                PutBgra(out, iw, x, y, (BYTE)b, (BYTE)g, (BYTE)r, 255);
            }
        }
    }
    w = iw;
    h = ih;
    return true;
}

// ---------------------------------------------------------- embedded JPEG

namespace {

bool OpenJpegFrame(IWICImagingFactory* factory, const std::vector<BYTE>& d, size_t off,
                   ComPtr<IWICBitmapFrameDecode>* frame) {
    if (off + 4 > d.size()) return false;
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) return false;
    if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(d.data() + off),
                                            (DWORD)std::min<size_t>(d.size() - off, 0xFFFFFFFFu)))) {
        return false;
    }
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
                                                WICDecodeMetadataCacheOnDemand, &dec))) {
        return false;
    }
    if (FAILED(dec->GetFrame(0, frame->ReleaseAndGetAddressOf()))) return false;
    return true;
}

} // namespace

bool DecodeEmbeddedJpeg(IWICImagingFactory* factory, const std::vector<BYTE>& d,
                        std::vector<BYTE>& out, int& w, int& h, std::wstring* err) {
    std::vector<size_t> candidates;
    for (size_t i = 0; i + 3 < d.size(); ++i) {
        if (d[i] == 0xFF && d[i + 1] == 0xD8 && d[i + 2] == 0xFF) {
            candidates.push_back(i);
            if (candidates.size() >= 64) break;
            i += 2;
        }
    }
    if (candidates.empty()) {
        return Fail(err, L"No embedded JPEG preview found in this file "
                         L"(install the Microsoft Raw Image Extension for full RAW decoding).");
    }

    size_t bestOff = 0;
    UINT bestArea = 0;
    for (size_t off : candidates) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (!OpenJpegFrame(factory, d, off, &frame)) continue;
        UINT fw = 0, fh = 0;
        if (FAILED(frame->GetSize(&fw, &fh))) continue;
        const UINT area = fw * fh;
        if (area > bestArea) {
            bestArea = area;
            bestOff = off;
        }
    }
    if (bestArea == 0) return Fail(err, L"No decodable embedded JPEG preview found.");

    ComPtr<IWICBitmapFrameDecode> frame;
    if (!OpenJpegFrame(factory, d, bestOff, &frame)) return Fail(err, L"Cannot open the embedded JPEG preview.");
    return WicSourceToBgra(factory, frame.Get(), out, w, h, err);
}

// ------------------------------------------------------------ extension sets

bool IsRawExtension(const std::wstring& ext) {
    static const wchar_t* kRaw[] = {
        L".3fr", L".ari", L".arw", L".bay", L".cap", L".cr2", L".cr3", L".crw",
        L".dcs", L".dcr", L".dng", L".drf", L".eip", L".erf", L".fff", L".gpr",
        L".iiq", L".k25", L".kdc", L".mef", L".mos", L".mrw", L".nef", L".nrw",
        L".orf", L".ori", L".pef", L".ptx", L".pxn", L".raf", L".raw", L".rw2",
        L".rwl", L".sr2", L".srf", L".srw", L".x3f"
    };
    for (const wchar_t* e : kRaw) if (ext == e) return true;
    return false;
}

bool IsExtraExtension(const std::wstring& ext) {
    return ext == L".tga" || ext == L".pcx" || ext == L".pnm" || ext == L".ppm" ||
           ext == L".pgm" || ext == L".pbm" || ext == L".pxm";
}
