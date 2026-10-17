// Extra decoders that WIC does not provide on every Windows install:
//   * TGA  - supported by the original JPEGView, not by WIC
//   * PCX / PNM - not supported by the original JPEGView (extra formats)
//   * camera RAW - largest embedded JPEG preview (works with no RAW codec)
#pragma once

#include <windows.h>
#include <wincodec.h>

#include <string>
#include <vector>

// Shared helper: convert any WIC source (frame or bitmap) to straight BGRA.
bool WicSourceToBgra(IWICImagingFactory* factory, IWICBitmapSource* src,
                     std::vector<BYTE>& out, int& w, int& h, std::wstring* err);

bool DecodeTga(const std::vector<BYTE>& data, std::vector<BYTE>& out, int& w, int& h, std::wstring* err);
bool DecodePcx(const std::vector<BYTE>& data, std::vector<BYTE>& out, int& w, int& h, std::wstring* err);
bool DecodePnm(const std::vector<BYTE>& data, std::vector<BYTE>& out, int& w, int& h, std::wstring* err);

// Finds every JPEG stream embedded in a container and decodes the largest one.
// Used for camera RAW (CR2/CR3/NEF/ARW/DNG/ORF/RW2/RAF/...) without relying on
// a RAW WIC codec being installed.
bool DecodeEmbeddedJpeg(IWICImagingFactory* factory, const std::vector<BYTE>& data,
                        std::vector<BYTE>& out, int& w, int& h, std::wstring* err);

// Extension classification (lower case, with dot).
bool IsRawExtension(const std::wstring& ext);
bool IsExtraExtension(const std::wstring& ext);
