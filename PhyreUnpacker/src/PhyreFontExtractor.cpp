#include "PhyreFontExtractor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <set>
#include <cmath>

// Phyre SDK - keep heavy includes in .cpp only
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Phyre.h"
// Ensure texel format definitions appear before texture headers
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTexelFormat.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTextureFormat.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTexture.h"
// Limit includes to only what we need to avoid pulling full render target surface API
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTextureBoxFilter.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTexture2D.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/Generic/PhyreTexture2DGeneric.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/D3D11/PhyreTexture2DD3D11.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Text/PhyreBitmapFont.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/PhyreAssetReference.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/ObjectModel/PhyreUtilityObjectModel.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreUtilityRendering.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreUtilitySerialization.h"
// Avoid direct render interface includes; not needed for extraction
// Avoid including Text utility header due to rendering scene types; we only need Text types bound by PBitmapFont itself.
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/ObjectModel/PhyreCluster.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreStreamReaderFile.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreBinarySerialization.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Text/PhyreBitmapFont.inl"

// No explicit Text utility forward-declare; types are bound via includes and Rendering utility

static HWND g_hiddenWindow = NULL;

static HWND createHiddenWindow()
{
    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "PhyreUnpackerHiddenWindow";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "PhyreUnpackerHidden", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 64, 64, NULL, NULL, wc.hInstance, NULL);
    return hwnd;
}

namespace PhyreUnpacker {
static void writeValidationReport(const PhyreUnpacker::FontInfo &fontInfo, const std::string &outputDirectory)
{
    try {
        const std::filesystem::path reportPath = std::filesystem::path(outputDirectory) /
            (std::filesystem::path(fontInfo.originalFileName).stem().string() + "_validate.txt");
        std::ofstream rpt(reportPath, std::ios::out | std::ios::trunc);
        if (!rpt.is_open()) return;

        const PUInt32 texW = fontInfo.textureWidth;
        const PUInt32 texH = fontInfo.textureHeight;
        const size_t totalPixels = static_cast<size_t>(texW) * static_cast<size_t>(texH);
        const auto &px = fontInfo.pixelData;
        size_t globalNonZero = 0; for (size_t i = 0; i < px.size() && i < totalPixels; ++i) if (px[i]) ++globalNonZero;

        rpt << "Texture " << texW << "x" << texH << ", nonZero=" << globalNonZero << "/" << totalPixels << "\n";
        rpt << "code x y w h rotated nonZero area ratio boundsOK ignored\n";

        auto clampU32 = [](PUInt32 v, PUInt32 lo, PUInt32 hi) -> PUInt32 { return v < lo ? lo : (v > hi ? hi : v); };

        size_t emptyRects = 0, oobRects = 0, ignoredRects = 0;
        for (const auto &ch : fontInfo.characters) {
            PUInt32 gx = (PUInt32)std::max(0.0f, std::floor(ch.uvX));
            PUInt32 gy = (PUInt32)std::max(0.0f, std::floor(ch.uvY));
            PUInt32 gw = (PUInt32)std::max(0.0f, std::ceil(ch.width));
            PUInt32 gh = (PUInt32)std::max(0.0f, std::ceil(ch.height));

            // Rectangle in texture space
            PUInt32 x0 = gx;
            PUInt32 y0 = gy;
            PUInt32 x1 = gx + gw; // exclusive
            PUInt32 y1 = gy + gh; // exclusive
            bool boundsOK = (x0 < texW) && (y0 < texH) && (x1 > 0) && (y1 > 0);
            if (x1 > texW) x1 = texW;
            if (y1 > texH) y1 = texH;
            if (x0 > texW) x0 = texW;
            if (y0 > texH) y0 = texH;
            const PUInt32 rw = (x1 > x0) ? (x1 - x0) : 0;
            const PUInt32 rh = (y1 > y0) ? (y1 - y0) : 0;
            size_t area = (size_t)rw * (size_t)rh;
            bool ignored = (gw == 0 || gh == 0 || area == 0); // нулевой глиф (space/nbsp) — игнорируем

            size_t nonZero = 0;
            if (area > 0 && !px.empty()) {
                for (PUInt32 yy = y0; yy < y1; ++yy) {
                    const size_t rowOff = (size_t)yy * texW;
                    for (PUInt32 xx = x0; xx < x1; ++xx) {
                        if (px[rowOff + xx]) ++nonZero;
                    }
                }
            }

            if (!ignored) {
                if (!boundsOK) ++oobRects;
                if (nonZero == 0) ++emptyRects;
            } else {
                ++ignoredRects;
            }

            double ratio = (area > 0) ? (double)nonZero / (double)area : 0.0;
            std::ostringstream os; os << std::hex << std::nouppercase << ch.characterCode; std::string h = os.str();
            rpt << h << " " << gx << " " << gy << " " << gw << " " << gh << " "
                << (ch.rotated ? 1 : 0) << " " << nonZero << " " << area << " " << std::fixed << std::setprecision(4) << ratio
                << " " << (boundsOK ? 1 : 0) << " " << (ignored ? 1 : 0) << "\n";
        }
        rpt << "summary: emptyRects=" << emptyRects << ", oobRects=" << oobRects << ", ignoredRects=" << ignoredRects << " of " << fontInfo.characters.size() << "\n";
        rpt.close();
    } catch (...) {
    }
}

static bool saveOverlayPPM(const PhyreUnpacker::FontInfo &fontInfo, const std::string &outputDirectory)
{
    try {
        if (fontInfo.textureWidth == 0 || fontInfo.textureHeight == 0 || fontInfo.pixelData.empty()) return false;
        const PUInt32 texW = fontInfo.textureWidth;
        const PUInt32 texH = fontInfo.textureHeight;

        // Build RGB base from grayscale
        std::vector<uint8_t> rgb;
        rgb.resize(static_cast<size_t>(texW) * static_cast<size_t>(texH) * 3u);
        for (size_t i = 0, j = 0; i < static_cast<size_t>(texW) * static_cast<size_t>(texH); ++i) {
            uint8_t g = fontInfo.pixelData[i];
            rgb[j++] = g; rgb[j++] = g; rgb[j++] = g;
        }

        auto setPx = [&](PUInt32 x, PUInt32 y, uint8_t r, uint8_t g, uint8_t b){
            if (x >= texW || y >= texH) return;
            const size_t idx = (static_cast<size_t>(y) * texW + x) * 3u;
            rgb[idx+0] = r; rgb[idx+1] = g; rgb[idx+2] = b;
        };
        auto drawRect = [&](PUInt32 x0, PUInt32 y0, PUInt32 x1, PUInt32 y1, uint8_t r, uint8_t g, uint8_t b){
            if (x0 >= x1 || y0 >= y1) return;
            for (PUInt32 x = x0; x < x1; ++x) { setPx(x, y0, r,g,b); if (y1>0) setPx(x, y1-1, r,g,b); }
            for (PUInt32 y = y0; y < y1; ++y) { setPx(x0, y, r,g,b); if (x1>0) setPx(x1-1, y, r,g,b); }
        };

        auto scanNonZero = [&](PUInt32 x0, PUInt32 y0, PUInt32 x1, PUInt32 y1) -> size_t {
            size_t nz = 0;
            for (PUInt32 y = y0; y < y1; ++y) {
                const size_t rowOff = static_cast<size_t>(y) * texW;
                for (PUInt32 x = x0; x < x1; ++x) if (fontInfo.pixelData[rowOff + x]) ++nz;
            }
            return nz;
        };

        size_t marked = 0;
        for (const auto &ch : fontInfo.characters) {
            PUInt32 gx = (PUInt32)std::max(0.0f, std::floor(ch.uvX));
            PUInt32 gy = (PUInt32)std::max(0.0f, std::floor(ch.uvY));
            PUInt32 gw = (PUInt32)std::max(0.0f, std::ceil(ch.width));
            PUInt32 gh = (PUInt32)std::max(0.0f, std::ceil(ch.height));

            // Bounds
            bool oob = false;
            uint64_t ex = (uint64_t)gx + (uint64_t)gw;
            uint64_t ey = (uint64_t)gy + (uint64_t)gh;
            if (gx >= texW || gy >= texH || ex == 0 || ey == 0 || ex > texW || ey > texH) oob = true;
            PUInt32 x0 = std::min<PUInt32>(gx, texW);
            PUInt32 y0 = std::min<PUInt32>(gy, texH);
            PUInt32 x1 = std::min<PUInt32>((PUInt32)ex, texW);
            PUInt32 y1 = std::min<PUInt32>((PUInt32)ey, texH);
            if (x1 <= x0 || y1 <= y0) { /* нулевой глиф (space/nbsp) — пропускаем */ continue; }

            const size_t area = (size_t)(x1 - x0) * (size_t)(y1 - y0);
            const size_t nonZero = scanNonZero(x0, y0, x1, y1);
            const double ratio = area ? (double)nonZero / (double)area : 0.0;

            // Color: magenta if OOB, red if empty, yellow if sparse
            uint8_t r=0,g=0,b=0; bool paint=false;
            if (oob) { r=255; g=0; b=255; paint=true; }
            else if (nonZero == 0) { r=255; g=0; b=0; paint=true; }
            else if (ratio < 0.05) { r=255; g=255; b=0; paint=true; }
            if (paint) { drawRect(x0, y0, x1, y1, r,g,b); ++marked; }
        }

        const std::filesystem::path outPath = std::filesystem::path(outputDirectory) /
            (std::filesystem::path(fontInfo.originalFileName).stem().string() + "_overlay.ppm");
        std::ofstream file(outPath, std::ios::binary);
        if (!file.is_open()) return false;
        file << "P6\n" << texW << " " << texH << "\n255\n";
        file.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
        file.close();
        return true;
    } catch (...) {
        return false;
    }
}


PhyreFontExtractor::PhyreFontExtractor()
    : m_sdkInitialized(false)
{
}

PhyreFontExtractor::~PhyreFontExtractor() {
    cleanupPhyreEngine();
}

bool PhyreFontExtractor::initializeSDK() {
    try {
        // Register minimal utilities required to load fonts/textures and binary clusters
        Phyre::PResult result = Phyre::PE_RESULT_NO_ERROR;
        result = Phyre::PUtility::RegisterUtility(Phyre::s_utilityObjectModel); if (result != Phyre::PE_RESULT_NO_ERROR) { m_lastError = "Register ObjectModel failed: " + std::to_string(result); return false; }
        // Rendering utility must be registered so PTexture2D and formats are bound
        result = Phyre::PUtility::RegisterUtility(Phyre::PRendering::s_utilityRendering); if (result != Phyre::PE_RESULT_NO_ERROR) { m_lastError = "Register Rendering failed: " + std::to_string(result); return false; }
        result = Phyre::PUtility::RegisterUtility(Phyre::PSerialization::s_utilitySerialization); if (result != Phyre::PE_RESULT_NO_ERROR) { m_lastError = "Register Serialization failed: " + std::to_string(result); return false; }
        // Ensure text classes are bound once
        {
            static bool textBound = false;
            if (!textBound) {
                Phyre::PText::PBitmapFont::Bind();
                Phyre::PText::PBitmapFontCharInfo::Bind();
                textBound = true;
            }
        }

        // Initialize Phyre runtime
        result = Phyre::PhyreInit();
        if (result != Phyre::PE_RESULT_NO_ERROR) { m_lastError = "Failed to initialize PhyreEngine SDK: " + std::to_string(result); return false; }

        // Initialize D3D11 render interface on a hidden window to enable staging readbacks
        HWND hwnd = createHiddenWindow();
        if (hwnd) {
#if !defined(PHYRE_TOOL_BUILD)
            Phyre::PRendering::PRenderInterface::Initialize();
            Phyre::PRendering::PD3D11Win32InitParams initParams; initParams.m_windowHandle = hwnd; initParams.m_multisampleCount = 1; initParams.m_fullscreen = false; initParams.m_vsync = false;
            Phyre::PRendering::PRenderInterface &ri = Phyre::PRendering::PRenderInterface::GetInstance();
            ri.initialize(&initParams, 1u<<20, 1u<<20);
#endif
        }

        m_sdkInitialized = true;
        return true;
    } catch (const std::exception &e) {
        m_lastError = "Failed to initialize PhyreEngine SDK: " + std::string(e.what());
        return false;
    }
}

bool PhyreFontExtractor::loadPhyreFile(const std::string &phyreFilePath) {
    if (!m_sdkInitialized) {
        m_lastError = "SDK not initialized";
        return false;
    }

    try {
        if (!std::filesystem::exists(phyreFilePath)) {
            m_lastError = "File does not exist: " + phyreFilePath;
            return false;
        }
        m_loadedFilePath = phyreFilePath;
        return true;
    } catch (const std::exception &e) {
        m_lastError = "Failed to validate file: " + std::string(e.what());
            return false;
    }
}

static bool copyTextureToL8(const Phyre::PRendering::PTexture2D &texture,
                            std::vector<uint8_t> &outPixels,
                            Phyre::PUInt32 &outWidth,
                            Phyre::PUInt32 &outHeight,
                            std::string &err) {
    auto decodeDXT5AlphaToL8 = [&](const uint8_t *srcBase, Phyre::PUInt32 rowPitch) -> bool {
        const Phyre::PUInt32 blocksWide = (outWidth + 3u) / 4u;
        const Phyre::PUInt32 blocksHigh = (outHeight + 3u) / 4u;
        for (Phyre::PUInt32 by = 0; by < blocksHigh; ++by) {
            const uint8_t *row = srcBase + static_cast<size_t>(by) * rowPitch;
            for (Phyre::PUInt32 bx = 0; bx < blocksWide; ++bx) {
                const uint8_t *block = row + bx * 16u; // 16 bytes per DXT5 block
                const uint8_t a0 = block[0];
                const uint8_t a1 = block[1];
                uint64_t alphaBits = 0;
                for (int i = 0; i < 6; ++i) alphaBits |= (uint64_t)block[2 + i] << (i * 8);
                uint8_t palette[8];
                palette[0] = a0; palette[1] = a1;
                if (a0 > a1) {
                    palette[2] = (uint8_t)((6 * a0 + 1 * a1) / 7);
                    palette[3] = (uint8_t)((5 * a0 + 2 * a1) / 7);
                    palette[4] = (uint8_t)((4 * a0 + 3 * a1) / 7);
                    palette[5] = (uint8_t)((3 * a0 + 4 * a1) / 7);
                    palette[6] = (uint8_t)((2 * a0 + 5 * a1) / 7);
                    palette[7] = (uint8_t)((1 * a0 + 6 * a1) / 7);
                } else {
                    palette[2] = (uint8_t)((4 * a0 + 1 * a1) / 5);
                    palette[3] = (uint8_t)((3 * a0 + 2 * a1) / 5);
                    palette[4] = (uint8_t)((2 * a0 + 3 * a1) / 5);
                    palette[5] = (uint8_t)((1 * a0 + 4 * a1) / 5);
                    palette[6] = 0;
                    palette[7] = 255;
                }
                for (int py = 0; py < 4; ++py) {
                    for (int px = 0; px < 4; ++px) {
                        const int i = py * 4 + px;
                        const uint32_t idx = (uint32_t)((alphaBits >> (i * 3)) & 0x7ull);
                        const uint32_t dstX = bx * 4u + (uint32_t)px;
                        const uint32_t dstY = by * 4u + (uint32_t)py;
                        if (dstX < outWidth && dstY < outHeight) {
                            outPixels[(size_t)dstY * outWidth + dstX] = palette[idx];
                        }
                    }
                }
            }
        }
        return true;
    };

    auto decodeDXT3AlphaToL8 = [&](const uint8_t *srcBase, Phyre::PUInt32 rowPitch) -> bool {
        const Phyre::PUInt32 blocksWide = (outWidth + 3u) / 4u;
        const Phyre::PUInt32 blocksHigh = (outHeight + 3u) / 4u;
        for (Phyre::PUInt32 by = 0; by < blocksHigh; ++by) {
            const uint8_t *row = srcBase + static_cast<size_t>(by) * rowPitch;
            for (Phyre::PUInt32 bx = 0; bx < blocksWide; ++bx) {
                const uint8_t *block = row + bx * 16u; // 16 bytes per DXT3 block
                const uint8_t *alphaBlock = block;     // first 8 bytes are alpha
                for (int py = 0; py < 4; ++py) {
                    for (int px = 0; px < 4; ++px) {
                        const int i = py * 4 + px;
                        const uint8_t packed = alphaBlock[i / 2];
                        const uint8_t nibble = (i & 1) ? (packed >> 4) : (packed & 0xF);
                        const uint8_t alpha = (uint8_t)(nibble * 17); // scale 4-bit to 8-bit
                        const uint32_t dstX = bx * 4u + (uint32_t)px;
                        const uint32_t dstY = by * 4u + (uint32_t)py;
                        if (dstX < outWidth && dstY < outHeight) {
                            outPixels[(size_t)dstY * outWidth + dstX] = alpha;
                        }
                    }
                }
            }
        }
        return true;
    };

    auto decodeBC4AlphaToL8 = [&](const uint8_t *srcBase, Phyre::PUInt32 rowPitch) -> bool {
        // BC4 block layout matches DXT5 alpha block (8 bytes per 4x4)
        const Phyre::PUInt32 blocksWide = (outWidth + 3u) / 4u;
        const Phyre::PUInt32 blocksHigh = (outHeight + 3u) / 4u;
        for (Phyre::PUInt32 by = 0; by < blocksHigh; ++by) {
            const uint8_t *row = srcBase + static_cast<size_t>(by) * rowPitch;
            for (Phyre::PUInt32 bx = 0; bx < blocksWide; ++bx) {
                const uint8_t *block = row + bx * 8u; // 8 bytes per BC4 block
                const uint8_t a0 = block[0];
                const uint8_t a1 = block[1];
                uint64_t bits = 0;
                for (int i = 0; i < 6; ++i) bits |= (uint64_t)block[2 + i] << (i * 8);
                uint8_t palette[8];
                palette[0] = a0; palette[1] = a1;
                if (a0 > a1) {
                    palette[2] = (uint8_t)((6 * a0 + 1 * a1) / 7);
                    palette[3] = (uint8_t)((5 * a0 + 2 * a1) / 7);
                    palette[4] = (uint8_t)((4 * a0 + 3 * a1) / 7);
                    palette[5] = (uint8_t)((3 * a0 + 4 * a1) / 7);
                    palette[6] = (uint8_t)((2 * a0 + 5 * a1) / 7);
                    palette[7] = (uint8_t)((1 * a0 + 6 * a1) / 7);
                } else {
                    palette[2] = (uint8_t)((4 * a0 + 1 * a1) / 5);
                    palette[3] = (uint8_t)((3 * a0 + 2 * a1) / 5);
                    palette[4] = (uint8_t)((2 * a0 + 3 * a1) / 5);
                    palette[5] = (uint8_t)((1 * a0 + 4 * a1) / 5);
                    palette[6] = 0;
                    palette[7] = 255;
                }
                for (int py = 0; py < 4; ++py) {
                    for (int px = 0; px < 4; ++px) {
                        const int i = py * 4 + px;
                        const uint32_t idx = (uint32_t)((bits >> (i * 3)) & 0x7ull);
                        const uint32_t dstX = bx * 4u + (uint32_t)px;
                        const uint32_t dstY = by * 4u + (uint32_t)py;
                        if (dstX < outWidth && dstY < outHeight) {
                            outPixels[(size_t)dstY * outWidth + dstX] = palette[idx];
                        }
                    }
                }
            }
        }
        return true;
    };

    try {
        outWidth = texture.getWidth();
        outHeight = texture.getHeight();
        outPixels.resize(static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight));

        // Map for CPU read (engine will create staging buffer internally if required)
        auto &mutableTex = const_cast<Phyre::PRendering::PTexture2D&>(texture);
        Phyre::PRendering::PTexture2D::PReadMapResult readMap;
        Phyre::PResult mapResult = mutableTex.map(readMap);
        {
            std::ostringstream dbg; dbg << "[map] rs=" << mapResult
                << " fmt=" << texture.getFormat().getName()
                << " ft=" << (int)texture.getFormat().getFormat()
                << " w=" << outWidth << " h=" << outHeight
                << " stride0=" << readMap.m_mips[0].m_stride
                << " buf0=" << (void*)readMap.m_mips[0].m_buffer;
            OutputDebugStringA((dbg.str() + "\n").c_str());
        }
        if (mapResult != Phyre::PE_RESULT_NO_ERROR) {
            err = "Texture map() failed: " + std::to_string(mapResult);
            return false;
        }

        const void *base = readMap.m_mips[0].m_buffer;
        const Phyre::PUInt32 stride = readMap.m_mips[0].m_stride;
        if (!base || stride == 0) {
            mutableTex.unmap(readMap);
            err = "Mapped texture returned null buffer or zero stride";
            return false;
        }

        const Phyre::PRendering::PTexelFormatType formatType = texture.getFormat().getFormat();
        const uint8_t *src = static_cast<const uint8_t*>(base);

        switch (formatType) {
            case Phyre::PRendering::PE_TEXTURE_FORMAT_L8: {
                for (Phyre::PUInt32 y = 0; y < outHeight; ++y) {
                    const uint8_t *row = src + static_cast<size_t>(y) * stride;
                    std::copy(row, row + outWidth, outPixels.begin() + static_cast<size_t>(y) * outWidth);
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_A8: {
                // Treat alpha-byte as luminance
                for (Phyre::PUInt32 y = 0; y < outHeight; ++y) {
                    const uint8_t *row = src + static_cast<size_t>(y) * stride;
                    std::copy(row, row + outWidth, outPixels.begin() + static_cast<size_t>(y) * outWidth);
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_LA8: {
                // Two bytes per pixel: L then A; use the strongest channel (covers cases where mask is in L)
                for (Phyre::PUInt32 y = 0; y < outHeight; ++y) {
                    const uint8_t *row = src + static_cast<size_t>(y) * stride;
                    for (Phyre::PUInt32 x = 0; x < outWidth; ++x) {
                        const uint8_t l = row[x * 2 + 0];
                        const uint8_t a = row[x * 2 + 1];
                        outPixels[static_cast<size_t>(y) * outWidth + x] = (l > a ? l : a);
                    }
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_RGBA8:
            case Phyre::PRendering::PE_TEXTURE_FORMAT_RGBA8_SRGB: {
                // Use max of channels (covers cases where glyph data is in color instead of alpha)
                for (Phyre::PUInt32 y = 0; y < outHeight; ++y) {
                    const uint8_t *row = src + static_cast<size_t>(y) * stride;
                    for (Phyre::PUInt32 x = 0; x < outWidth; ++x) {
                        const uint8_t r = row[x * 4 + 0];
                        const uint8_t g = row[x * 4 + 1];
                        const uint8_t b = row[x * 4 + 2];
                        const uint8_t a = row[x * 4 + 3];
                        uint8_t m = r; if (g > m) m = g; if (b > m) m = b; if (a > m) m = a;
                        outPixels[static_cast<size_t>(y) * outWidth + x] = m;
                    }
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_ARGB8:
            case Phyre::PRendering::PE_TEXTURE_FORMAT_ARGB8_SRGB: {
                // Use max of channels
                for (Phyre::PUInt32 y = 0; y < outHeight; ++y) {
                    const uint8_t *row = src + static_cast<size_t>(y) * stride;
                    for (Phyre::PUInt32 x = 0; x < outWidth; ++x) {
                        const uint8_t a = row[x * 4 + 0];
                        const uint8_t r = row[x * 4 + 1];
                        const uint8_t g = row[x * 4 + 2];
                        const uint8_t b = row[x * 4 + 3];
                        uint8_t m = r; if (g > m) m = g; if (b > m) m = b; if (a > m) m = a;
                        outPixels[static_cast<size_t>(y) * outWidth + x] = m;
                    }
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_DXT1: {
                // Decode color (no alpha) and take luma
                const Phyre::PUInt32 blocksWide = (outWidth + 3u) / 4u;
                const Phyre::PUInt32 blocksHigh = (outHeight + 3u) / 4u;
                for (Phyre::PUInt32 by = 0; by < blocksHigh; ++by) {
                    const uint8_t *row = src + static_cast<size_t>(by) * stride;
                    for (Phyre::PUInt32 bx = 0; bx < blocksWide; ++bx) {
                        const uint8_t *block = row + bx * 8u;
                        uint16_t c0 = block[0] | (block[1] << 8);
                        uint16_t c1 = block[2] | (block[3] << 8);
                        auto expand565 = [](uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b){
                            r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
                            g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
                            b = (uint8_t)((c & 0x1F) * 255 / 31);
                        };
                        uint8_t r0,g0,b0,r1,g1,b1; expand565(c0,r0,g0,b0); expand565(c1,r1,g1,b1);
                        uint8_t r2,g2,b2,r3,g3,b3;
                        if (c0 > c1) {
                            r2 = (uint8_t)((2*r0 + r1)/3); g2 = (uint8_t)((2*g0 + g1)/3); b2 = (uint8_t)((2*b0 + b1)/3);
                            r3 = (uint8_t)((r0 + 2*r1)/3); g3 = (uint8_t)((g0 + 2*g1)/3); b3 = (uint8_t)((b0 + 2*b1)/3);
                        } else {
                            r2 = (uint8_t)((r0 + r1)/2); g2 = (uint8_t)((g0 + g1)/2); b2 = (uint8_t)((b0 + b1)/2);
                            r3 = 0; g3 = 0; b3 = 0; // transparent -> black
                        }
                        const uint32_t idxBits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
                        for (int py = 0; py < 4; ++py) {
                            for (int px = 0; px < 4; ++px) {
                                const int i = py * 4 + px;
                                const uint8_t idx = (uint8_t)((idxBits >> (i * 2)) & 0x3u);
                                uint8_t r,g,b;
                                switch (idx) {
                                    case 0: r=r0; g=g0; b=b0; break;
                                    case 1: r=r1; g=g1; b=b1; break;
                                    case 2: r=r2; g=g2; b=b2; break;
                                    default: r=r3; g=g3; b=b3; break;
                                }
                                const uint32_t dstX = bx * 4u + (uint32_t)px;
                                const uint32_t dstY = by * 4u + (uint32_t)py;
                                if (dstX < outWidth && dstY < outHeight) {
                                    const uint8_t luma = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
                                    outPixels[(size_t)dstY * outWidth + dstX] = luma;
                                }
                            }
                        }
                    }
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_DXT3:
            case Phyre::PRendering::PE_TEXTURE_FORMAT_DXT3_SRGB: {
                // Decode both alpha and color and take the strongest
                std::vector<uint8_t> alpha(outPixels.size());
                if (!decodeDXT3AlphaToL8(src, stride)) { mutableTex.unmap(readMap); err = "DXT3 alpha decode failed"; return false; }
                // Copy alpha result
                alpha.assign(outPixels.begin(), outPixels.end());
                // Decode color from block+8
                const Phyre::PUInt32 blocksWide = (outWidth + 3u) / 4u;
                const Phyre::PUInt32 blocksHigh = (outHeight + 3u) / 4u;
                for (Phyre::PUInt32 by = 0; by < blocksHigh; ++by) {
                    const uint8_t *row = src + static_cast<size_t>(by) * stride;
                    for (Phyre::PUInt32 bx = 0; bx < blocksWide; ++bx) {
                        const uint8_t *block = row + bx * 16u + 8u; // skip alpha
                        uint16_t c0 = block[0] | (block[1] << 8);
                        uint16_t c1 = block[2] | (block[3] << 8);
                        auto expand565 = [](uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b){
                            r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
                            g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
                            b = (uint8_t)((c & 0x1F) * 255 / 31);
                        };
                        uint8_t r0,g0,b0,r1,g1,b1; expand565(c0,r0,g0,b0); expand565(c1,r1,g1,b1);
                        uint8_t r2,g2,b2,r3,g3,b3;
                        if (c0 > c1) {
                            r2 = (uint8_t)((2*r0 + r1)/3); g2 = (uint8_t)((2*g0 + g1)/3); b2 = (uint8_t)((2*b0 + b1)/3);
                            r3 = (uint8_t)((r0 + 2*r1)/3); g3 = (uint8_t)((g0 + 2*g1)/3); b3 = (uint8_t)((b0 + 2*b1)/3);
                        } else {
                            r2 = (uint8_t)((r0 + r1)/2); g2 = (uint8_t)((g0 + g1)/2); b2 = (uint8_t)((b0 + b1)/2);
                            r3 = 0; g3 = 0; b3 = 0;
                        }
                        const uint32_t idxBits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
                        for (int py = 0; py < 4; ++py) {
                            for (int px = 0; px < 4; ++px) {
                                const int i = py * 4 + px;
                                const uint8_t idx = (uint8_t)((idxBits >> (i * 2)) & 0x3u);
                                uint8_t r,g,b;
                                switch (idx) { case 0: r=r0; g=g0; b=b0; break; case 1: r=r1; g=g1; b=b1; break; case 2: r=r2; g=g2; b=b2; break; default: r=r3; g=g3; b=b3; break; }
                                const uint8_t luma = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
                                const uint32_t dstX = bx * 4u + (uint32_t)px;
                                const uint32_t dstY = by * 4u + (uint32_t)py;
                                if (dstX < outWidth && dstY < outHeight) {
                                    const size_t di = (size_t)dstY * outWidth + dstX;
                                    outPixels[di] = (alpha[di] > luma ? alpha[di] : luma);
                                }
                            }
                        }
                    }
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_DXT5:
            case Phyre::PRendering::PE_TEXTURE_FORMAT_DXT5_SRGB: {
                // Decode both alpha and color and take the strongest
                std::vector<uint8_t> alpha(outPixels.size());
                if (!decodeDXT5AlphaToL8(src, stride)) { mutableTex.unmap(readMap); err = "DXT5 alpha decode failed"; return false; }
                alpha.assign(outPixels.begin(), outPixels.end());
                const Phyre::PUInt32 blocksWide = (outWidth + 3u) / 4u;
                const Phyre::PUInt32 blocksHigh = (outHeight + 3u) / 4u;
                for (Phyre::PUInt32 by = 0; by < blocksHigh; ++by) {
                    const uint8_t *row = src + static_cast<size_t>(by) * stride;
                    for (Phyre::PUInt32 bx = 0; bx < blocksWide; ++bx) {
                        const uint8_t *block = row + bx * 16u + 8u; // skip alpha
                        uint16_t c0 = block[0] | (block[1] << 8);
                        uint16_t c1 = block[2] | (block[3] << 8);
                        auto expand565 = [](uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b){
                            r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
                            g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
                            b = (uint8_t)((c & 0x1F) * 255 / 31);
                        };
                        uint8_t r0,g0,b0,r1,g1,b1; expand565(c0,r0,g0,b0); expand565(c1,r1,g1,b1);
                        uint8_t r2,g2,b2,r3,g3,b3;
                        if (c0 > c1) {
                            r2 = (uint8_t)((2*r0 + r1)/3); g2 = (uint8_t)((2*g0 + g1)/3); b2 = (uint8_t)((2*b0 + b1)/3);
                            r3 = (uint8_t)((r0 + 2*r1)/3); g3 = (uint8_t)((g0 + 2*g1)/3); b3 = (uint8_t)((b0 + 2*b1)/3);
                        } else {
                            r2 = (uint8_t)((r0 + r1)/2); g2 = (uint8_t)((g0 + g1)/2); b2 = (uint8_t)((b0 + b1)/2);
                            r3 = 0; g3 = 0; b3 = 0;
                        }
                        const uint32_t idxBits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
                        for (int py = 0; py < 4; ++py) {
                            for (int px = 0; px < 4; ++px) {
                                const int i = py * 4 + px;
                                const uint8_t idx = (uint8_t)((idxBits >> (i * 2)) & 0x3u);
                                uint8_t r,g,b;
                                switch (idx) { case 0: r=r0; g=g0; b=b0; break; case 1: r=r1; g=g1; b=b1; break; case 2: r=r2; g=g2; b=b2; break; default: r=r3; g=g3; b=b3; break; }
                                const uint8_t luma = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
                                const uint32_t dstX = bx * 4u + (uint32_t)px;
                                const uint32_t dstY = by * 4u + (uint32_t)py;
                                if (dstX < outWidth && dstY < outHeight) {
                                    const size_t di = (size_t)dstY * outWidth + dstX;
                                    outPixels[di] = (alpha[di] > luma ? alpha[di] : luma);
                                }
                            }
                        }
                    }
                }
                break;
            }
            case Phyre::PRendering::PE_TEXTURE_FORMAT_BC4: {
                if (!decodeBC4AlphaToL8(src, stride)) {
                    mutableTex.unmap(readMap);
                    err = "BC4 alpha decode failed";
                    return false;
                }
                break;
            }
            default: {
                // Unsupported format for direct L8 conversion
                mutableTex.unmap(readMap);
                err = "Unsupported texture format for L8 conversion: " + std::string(texture.getFormat().getName());
            return false;
            }
        }

        mutableTex.unmap(readMap);
        if (!outPixels.empty()) {
            uint8_t mn = 255, mx = 0; size_t nonZero = 0;
            for (size_t i = 0; i < outPixels.size(); ++i) { uint8_t v = outPixels[i]; if (v < mn) mn = v; if (v > mx) mx = v; if (v) ++nonZero; }
            std::ostringstream dbg; dbg << "[pixels] min=" << (int)mn << " max=" << (int)mx << " nonZero=" << nonZero;
            OutputDebugStringA((dbg.str() + "\n").c_str());
        }
        return true;
    } catch (const std::exception &e) {
        err = e.what();
        return false;
    }
}

bool PhyreFontExtractor::extractFonts(const std::string &outputDirectory) {
    if (!m_sdkInitialized) {
        m_lastError = "SDK not initialized";
            return false;
        }
    if (m_loadedFilePath.empty()) {
        m_lastError = "No .phyre file loaded";
            return false;
        }

    try {
        // Ensure CWD points to SCE_PHYRE so relative asset references resolve
        {
            char phyreRoot[1024] = {0};
            DWORD got = GetEnvironmentVariableA("SCE_PHYRE", phyreRoot, static_cast<DWORD>(sizeof(phyreRoot)));
            if (got > 0 && got < sizeof(phyreRoot)) {
                SetCurrentDirectoryA(phyreRoot);
            }
        }

        std::filesystem::create_directories(outputDirectory);

        // Load asset file via PCluster API (handles namespaces and platform specifics)
        Phyre::PCluster cluster;
               // Analyze first to build namespace mapping without strict platform enforcement in tool build
               {
                   Phyre::PSerialization::PStreamReaderFile reader(m_loadedFilePath.c_str());
                   if (!reader.isOpen()) { m_lastError = std::string("Cannot open: ") + m_loadedFilePath; return false; }
                   Phyre::PResult ar = Phyre::PSerialization::PBinary::analyzeCluster(reader);
                   if (ar != Phyre::PE_RESULT_NO_ERROR) {
                       m_lastError = std::string("analyzeCluster failed: ") + std::to_string(ar) + ": " + Phyre::PError::GetLastError();
        return false;
    }
}
               // analyze first for namespace mapping (tool build friendly), then load directly
               {
                   Phyre::PSerialization::PStreamReaderFile reader(m_loadedFilePath.c_str());
                   if (!reader.isOpen()) { m_lastError = std::string("Cannot open: ") + m_loadedFilePath; return false; }
                   Phyre::PResult ar = Phyre::PSerialization::PBinary::analyzeCluster(reader);
                   if (ar != Phyre::PE_RESULT_NO_ERROR) {
                       m_lastError = std::string("analyzeCluster failed: ") + std::to_string(ar) + ": " + Phyre::PError::GetLastError();
                       return false;
                   }
               }
               {
                   Phyre::PSerialization::PStreamReaderFile reader(m_loadedFilePath.c_str());
                   if (!reader.isOpen()) { m_lastError = std::string("Cannot open: ") + m_loadedFilePath; return false; }
                   Phyre::PResult lr = Phyre::PSerialization::PBinary::loadCluster(reader, cluster);
                   if (lr != Phyre::PE_RESULT_NO_ERROR) {
                       m_lastError = "loadCluster failed: " + std::to_string(lr) + ": " + Phyre::PError::GetLastError();
            return false;
        }
               }

        // Attempt to preload referenced clusters for PAssetReferences (e.g., *.fgen asset refs)
        {
            const auto loadedDir = std::filesystem::path(m_loadedFilePath).parent_path();
            char phyreRootBuf[1024] = {0};
            DWORD phyreGot = GetEnvironmentVariableA("SCE_PHYRE", phyreRootBuf, static_cast<DWORD>(sizeof(phyreRootBuf)));
            const std::string phyreRootStr = (phyreGot > 0 && phyreGot < sizeof(phyreRootBuf)) ? std::string(phyreRootBuf) : std::string();

            Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PAssetReference> arIt(cluster);
            std::string extractedFgenPath;
            for (; arIt; ++arIt) {
                const Phyre::PAssetReference &ar = *arIt;

                const Phyre::PString &id = ar.getID();
                const char *idC = id.c_str();
                std::string idStr = idC ? idC : "";
                if (idStr.empty())
                    continue;

                // Strip '#object' part, keep left part as base path/id
                const size_t sharp = idStr.find('#');
                const std::string base = (sharp == std::string::npos) ? idStr : idStr.substr(0, sharp);
                if (base.empty())
                    continue;

                // Generate candidate paths
                std::vector<std::string> candidates;
                auto push_unique = [&candidates](const std::string &p) {
                    if (p.empty()) return;
                    if (std::find(candidates.begin(), candidates.end(), p) == candidates.end()) {
                        candidates.push_back(p);
                    }
                };

                const bool hasPhyreExt = base.size() >= 6 && base.rfind(".phyre") == base.size() - 6;
                push_unique(base);
                if (!hasPhyreExt) push_unique(base + ".phyre");
                push_unique(std::string("Media/") + base);
                if (!hasPhyreExt) push_unique(std::string("Media/") + base + ".phyre");
                if (!loadedDir.empty()) {
                    push_unique((loadedDir / base).string());
                    if (!hasPhyreExt) push_unique((loadedDir / (base + ".phyre")).string());
                }
                if (!phyreRootStr.empty()) {
                    push_unique((std::filesystem::path(phyreRootStr) / base).string());
                    if (!hasPhyreExt) push_unique((std::filesystem::path(phyreRootStr) / (base + ".phyre")).string());
                    push_unique((std::filesystem::path(phyreRootStr) / "Media" / base).string());
                    if (!hasPhyreExt) push_unique((std::filesystem::path(phyreRootStr) / "Media" / (base + ".phyre")).string());
                }

                const bool isFgenId = (base.rfind(".fgen") != std::string::npos);
                // Try to load first existing candidate (only .phyre files)
                for (const auto &cand : candidates) {
                    std::error_code ec;
                    if (!std::filesystem::exists(cand, ec)) continue;
                    const std::string ext = std::filesystem::path(cand).extension().string();
                    const bool isPhyre = (ext == ".phyre");
                    Phyre::PCluster dep;
                    Phyre::PResult lr2 = isPhyre ? Phyre::PCluster::LoadAssetFile(dep, cand.c_str()) : Phyre::PE_RESULT_ARGUMENT_OUT_OF_RANGE;
                    if (lr2 == Phyre::PE_RESULT_NO_ERROR) {
                        cluster.absorb(dep);
                        if (isFgenId && extractedFgenPath.empty()) {
                            // Copy the original .fgen to output folder for convenience
                            try {
                                const auto dest = std::filesystem::path(outputDirectory) / std::filesystem::path(base).filename();
                                std::filesystem::create_directories(std::filesystem::path(outputDirectory));
                                std::filesystem::copy_file(cand, dest, std::filesystem::copy_options::overwrite_existing, ec);
                                if (!ec) extractedFgenPath = dest.string();
                                // If copied, try to read line 2 (TTF path) and save as helper text
                                try {
                                    std::ifstream fsrc(dest);
                                    if (fsrc.good()) {
                                        std::string line1, line2;
                                        std::getline(fsrc, line1);
                                        std::getline(fsrc, line2); // line 2 should be TTF path
                                        fsrc.close();
                                        if (!line2.empty()) {
                                            const auto ttfOut = dest.string() + ".ttf.name.txt";
                                            std::ofstream fout(ttfOut, std::ios::out | std::ios::trunc);
                                            if (fout.is_open()) { fout << line2 << "\n"; fout.close(); }
                                            std::ostringstream dbg; dbg << "[fgen] TTF from fgen: " << line2;
                                            OutputDebugStringA((dbg.str() + "\n").c_str());
                                        }
                                    }
                                } catch (...) {}
                            } catch (...) {
                                // ignore copy errors; continue
                            }
                        }
                        break; // one is enough
                    }
                    // If it isn't a .phyre and is an .fgen, just copy it beside outputs
                    if (!isPhyre && isFgenId && extractedFgenPath.empty()) {
                        try {
                            const auto dest = std::filesystem::path(outputDirectory) / std::filesystem::path(base).filename();
                            std::filesystem::create_directories(std::filesystem::path(outputDirectory));
                            std::filesystem::copy_file(cand, dest, std::filesystem::copy_options::overwrite_existing, ec);
                            if (!ec) extractedFgenPath = dest.string();
                        } catch (...) {}
                    }
                }
            }
            // Also write a hint file with ID if .fgen wasn't found on disk
            if (extractedFgenPath.empty()) {
                Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PAssetReference> arIt2(cluster);
                for (; arIt2; ++arIt2) {
                    const Phyre::PAssetReference &ar = *arIt2;
                    const char *idC2 = ar.getID().c_str();
                    std::string idStr2 = idC2 ? idC2 : "";
                    if (idStr2.find(".fgen") != std::string::npos) {
                        try {
                            const auto hintPath = std::filesystem::path(outputDirectory) / "referenced_fgen_id.txt";
                            std::ofstream f(hintPath, std::ios::out | std::ios::trunc);
                            if (f.is_open()) {
                                f << idStr2;
                                f.close();
                            }
                        } catch (...) {}
                        break;
                    }
                }
            }
        }

        // Resolve asset references and perform fixups to materialize referenced objects
        {
            Phyre::PResult rr = cluster.resolveAssetReferences();
            if (rr != Phyre::PE_RESULT_NO_ERROR) {
                m_lastError = std::string("resolveAssetReferences failed: ") + std::to_string(rr) + ": " + Phyre::PError::GetLastError();
                return false;
            }
            Phyre::PResult fx = cluster.fixupInstances();
            if (fx != Phyre::PE_RESULT_NO_ERROR) {
                m_lastError = std::string("fixupInstances failed: ") + std::to_string(fx) + ": " + Phyre::PError::GetLastError();
        return false;
    }
}

        // Note: concatenated clusters are appended internally by LoadAssetFile; iteration over objects via iterators covers siblings

        // Iterate bitmap fonts
        Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PText::PBitmapFont> it(cluster);
        bool any = false;
        for (; it; ++it) {
            any = true;
            const Phyre::PText::PBitmapFont &bf = *it;

    FontInfo fontInfo;
            fontInfo.fontName = std::filesystem::path(m_loadedFilePath).stem().string();
            fontInfo.fontSize = bf.getFontSize();
            fontInfo.lineSpacing = bf.getLineSpacing();
            fontInfo.baselineOffset = bf.getBaselineOffset();
            fontInfo.isSDF = bf.isSDF();
            fontInfo.originalFileName = m_loadedFilePath;

            // Texture: always set dimensions; if readback fails, emit black image of correct size
            const Phyre::PRendering::PTexture2D &tex = bf.getBitmapFontTexture();
            Phyre::PUInt32 w = tex.getWidth();
            Phyre::PUInt32 h = tex.getHeight();
            fontInfo.textureWidth = w;
            fontInfo.textureHeight = h;
            std::string texErr;
            if (!copyTextureToL8(tex, fontInfo.pixelData, w, h, texErr)) {
                // ensure valid buffer so PPM is openable
                const size_t needed = static_cast<size_t>(fontInfo.textureWidth) * static_cast<size_t>(fontInfo.textureHeight);
                fontInfo.pixelData.assign(needed, 0);
                std::ostringstream dbg; dbg << "[warn] copyTextureToL8 failed: " << texErr;
                OutputDebugStringA((dbg.str() + "\n").c_str());

                // Fallback: try to read raw platform tail (works for simple L8 DX11 textures)
                try {
                    const std::string &filePath = m_loadedFilePath;
                    std::ifstream fs(filePath, std::ios::binary);
                    if (fs.good()) {
                        fs.seekg(0, std::ios::end);
                        const std::streamoff fsize = fs.tellg();
                        const std::streamoff need = static_cast<std::streamoff>(fontInfo.textureWidth) * static_cast<std::streamoff>(fontInfo.textureHeight);
                        if (fsize > need) {
                            fs.seekg(fsize - need, std::ios::beg);
                            std::vector<uint8_t> tail(static_cast<size_t>(need));
                            fs.read(reinterpret_cast<char*>(tail.data()), need);
                            if (fs.gcount() == need) {
                                // Heuristic: if not all zeros, accept
                                uint8_t mn = 255, mx = 0; size_t nonZero = 0;
                                for (size_t i = 0; i < tail.size(); ++i) { uint8_t v = tail[i]; if (v < mn) mn = v; if (v > mx) mx = v; if (v) ++nonZero; }
                                std::ostringstream dbg2; dbg2 << "[fallback-tail] read=" << tail.size() << " bytes mn=" << (int)mn << " mx=" << (int)mx << " nz=" << nonZero;
                                OutputDebugStringA((dbg2.str() + "\n").c_str());
                                if (nonZero > 0) {
                                    fontInfo.pixelData.swap(tail);
                                }
                            }
                        }
                    }
                } catch (...) {}
            }

            // Debug hook left out to avoid pulling heavy DDS dependencies in this tool build

            // Characters
            {
                const Phyre::PArray<Phyre::PText::PBitmapFontCharInfo> &chars = bf.getCharacterInfoArray();
                const Phyre::PUInt32 count = chars.getCount();
                fontInfo.characters.reserve(count);
                for (Phyre::PUInt32 i = 0; i < count; ++i) {
                    const Phyre::PText::PBitmapFontCharInfo &ci = chars[i];
                    CharacterInfo co{};
                    co.characterCode = ci.m_characterCode;
                    co.width = ci.m_width;
                    co.height = ci.m_height;
                    co.uvX = ci.m_uv[0];
                    co.uvY = ci.m_uv[1];
                    co.offsetX = ci.m_offset[0];
                    co.offsetY = ci.m_offset[1];
                    co.advanceX = ci.m_advance[0];
                    co.advanceY = ci.m_advance[1];
                    co.rotated = ci.m_rotated;
                    co.kernPairs = ci.m_kernPairs;
                    co.kernOffset = ci.m_kernOffset;
                    fontInfo.characters.push_back(co);
                }
            }

            // Kerning
            {
                const Phyre::PArray<Phyre::PInt32> &kerning = bf.getKerningInfoArray();
                const Phyre::PUInt32 count = kerning.getCount();
                fontInfo.kerningData.resize(count);
                for (Phyre::PUInt32 i = 0; i < count; ++i) {
                    fontInfo.kerningData[i] = kerning[i];
                }
            }

            // Save files
            if (!saveFontAsPPM(fontInfo, outputDirectory)) return false;
            if (!saveFontAsXML(fontInfo, outputDirectory)) return false;
            if (!saveFontAsJSON(fontInfo, outputDirectory)) return false;
            writeValidationReport(fontInfo, outputDirectory);
            saveOverlayPPM(fontInfo, outputDirectory);

            // Save reconstructed .fgen (text) from runtime font object
            try {
                const auto fgenPath = (std::filesystem::path(outputDirectory) / (std::filesystem::path(fontInfo.originalFileName).stem().string() + ".fgen")).string();
                std::ofstream f(fgenPath, std::ios::out | std::ios::trunc);
                if (f.is_open()) {
                    // Determine a TTF path usable by PFGenParser (line 2).
                    // Strategy: try to locate a .ttf in likely Media directories and copy to output; then write base filename.
                    std::string ttfLine;
                    try {
                        std::vector<std::filesystem::path> searchDirs;
                        const std::filesystem::path outDir(outputDirectory);
                        searchDirs.push_back(outDir);
                        const std::filesystem::path loadedDir = std::filesystem::path(m_loadedFilePath).parent_path();
                        searchDirs.push_back(loadedDir.parent_path()); // .../Media/D3D11
                        searchDirs.push_back(loadedDir.parent_path().parent_path() / "Media");
                        searchDirs.push_back(loadedDir.parent_path().parent_path() / "Media" / "SpaceStation");
                        char phyreRoot[1024] = {0};
                        DWORD got = GetEnvironmentVariableA("SCE_PHYRE", phyreRoot, static_cast<DWORD>(sizeof(phyreRoot)));
                        if (got > 0 && got < sizeof(phyreRoot)) {
                            searchDirs.push_back(std::filesystem::path(phyreRoot) / "Media");
                            searchDirs.push_back(std::filesystem::path(phyreRoot) / "Media" / "SpaceStation");
                        }
                        std::filesystem::path foundTtf;
                        auto nameLower = [](std::string s){ for (auto &c : s) c = (char)tolower((unsigned char)c); return s; };
                        // Try to guess base name from input (strip double extension .fgen.phyre -> name)
                        std::string baseLower = nameLower(std::filesystem::path(std::filesystem::path(m_loadedFilePath).stem().string()).stem().string());
                        for (const auto &dir : searchDirs) {
                            std::error_code ec; if (!std::filesystem::exists(dir, ec)) continue;
                            for (auto it = std::filesystem::directory_iterator(dir, ec); !ec && it != std::filesystem::end(it); ++it) {
                                if (!it->is_regular_file(ec)) continue;
                                if (nameLower(it->path().extension().string()) == ".ttf") {
                                    const std::string fileLower = nameLower(it->path().filename().string());
                                    if (fileLower.find(baseLower) != std::string::npos || baseLower.empty()) { foundTtf = it->path(); break; }
                                }
                            }
                            if (!foundTtf.empty()) break;
                        }
                        if (!foundTtf.empty()) {
                            const std::filesystem::path copied = std::filesystem::path(outputDirectory) / foundTtf.filename();
                            std::error_code ec; std::filesystem::copy_file(foundTtf, copied, std::filesystem::copy_options::overwrite_existing, ec);
                            ttfLine = copied.filename().string();
                        }
                    } catch (...) {}
                    if (ttfLine.empty()) ttfLine = "NEUROPOL.ttf"; // Fallback; user can adjust manually

                    // PFGenParser strict layout
                    f << "fgen\n";                                 // 1: id
                    f << ttfLine << "\n";                           // 2: ttf path (relative to file dir)
                    f << "DUMMY\n";                                 // 3: unused
                    f << "Fixed\n";                                 // 4: packing strategy
                    f << (fontInfo.textureWidth ? fontInfo.textureWidth : 512) << "\n";   // 5: width
                    f << (fontInfo.textureHeight ? fontInfo.textureHeight : 512) << "\n"; // 6: height
                    f << fontInfo.fontSize << "\n";                 // 7: size
                    f << (fontInfo.isSDF ? 1 : 0) << "\n";          // 8: sdf
                    f << 1 << "\n";                                  // 9: charScale (default 1)
                    f << 0 << "\n";                                  // 10: charPad
                    f << 0 << "\n";                                  // 11: mipPad

                    // 12+: hex character codes, unique and sorted
                    std::set<Phyre::PInt32> codes;
                    for (const auto &ch : fontInfo.characters) codes.insert(ch.characterCode);
                    for (const auto code : codes) {
                        std::ostringstream os; os << std::hex << std::nouppercase << code;
                        f << os.str() << "\n";
                    }
                    f.close();
                }
            } catch (...) {}
        }

        if (!any) {
            m_lastError = "No PBitmapFont found in cluster";
            return false;
        }

        return true;
    } catch (const std::exception &e) {
        m_lastError = "extractFonts failed: " + std::string(e.what());
        return false;
    }
}

std::string PhyreFontExtractor::getLastError() const {
    return m_lastError;
}

void PhyreFontExtractor::cleanupPhyreEngine() {
    if (m_sdkInitialized) {
        Phyre::PhyreExit();
        m_sdkInitialized = false;
    }
}

// Save helpers remain unchanged
bool PhyreFontExtractor::saveFontAsPPM(const FontInfo& fontInfo, const std::string& outputDirectory) {
    try {
        std::string fileName = std::filesystem::path(fontInfo.originalFileName).stem().string() + "_texture.ppm";
        std::string filePath = (std::filesystem::path(outputDirectory) / fileName).string();

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Cannot create PPM file: " + filePath;
            return false;
        }

        file << "P6\n";
        file << fontInfo.textureWidth << " " << fontInfo.textureHeight << "\n";
        file << "255\n";

        const size_t expected = static_cast<size_t>(fontInfo.textureWidth) * static_cast<size_t>(fontInfo.textureHeight);
        for (size_t i = 0; i < expected; ++i) {
            uint8_t gray = i < fontInfo.pixelData.size() ? fontInfo.pixelData[i] : 0u;
            file.write(reinterpret_cast<const char*>(&gray), 1);
            file.write(reinterpret_cast<const char*>(&gray), 1);
            file.write(reinterpret_cast<const char*>(&gray), 1);
        }

        file.close();
        return true;
    } catch (const std::exception &e) {
        m_lastError = "Failed to save PPM: " + std::string(e.what());
        return false;
    }
}

bool PhyreFontExtractor::saveFontAsXML(const FontInfo& fontInfo, const std::string& outputDirectory) {
    try {
        std::string fileName = std::filesystem::path(fontInfo.originalFileName).stem().string() + "_metadata.xml";
        std::string filePath = (std::filesystem::path(outputDirectory) / fileName).string();

        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_lastError = "Cannot create XML file: " + filePath;
            return false;
        }

        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<font>\n";
        file << "  <name>" << fontInfo.fontName << "</name>\n";
        file << "  <size>" << fontInfo.fontSize << "</size>\n";
        file << "  <lineSpacing>" << fontInfo.lineSpacing << "</lineSpacing>\n";
        file << "  <baselineOffset>" << fontInfo.baselineOffset << "</baselineOffset>\n";
        file << "  <isSDF>" << (fontInfo.isSDF ? "true" : "false") << "</isSDF>\n";
        file << "  <textureWidth>" << fontInfo.textureWidth << "</textureWidth>\n";
        file << "  <textureHeight>" << fontInfo.textureHeight << "</textureHeight>\n";
        file << "  <characters>\n";

        for (const auto &ch : fontInfo.characters) {
            file << "    <character>\n";
            file << "      <code>" << ch.characterCode << "</code>\n";
            file << "      <width>" << ch.width << "</width>\n";
            file << "      <height>" << ch.height << "</height>\n";
            file << "      <uvX>" << ch.uvX << "</uvX>\n";
            file << "      <uvY>" << ch.uvY << "</uvY>\n";
            file << "      <offsetX>" << ch.offsetX << "</offsetX>\n";
            file << "      <offsetY>" << ch.offsetY << "</offsetY>\n";
            file << "      <advanceX>" << ch.advanceX << "</advanceX>\n";
            file << "      <advanceY>" << ch.advanceY << "</advanceY>\n";
            file << "      <rotated>" << (ch.rotated ? "true" : "false") << "</rotated>\n";
            file << "      <kernPairs>" << ch.kernPairs << "</kernPairs>\n";
            file << "      <kernOffset>" << ch.kernOffset << "</kernOffset>\n";
            file << "    </character>\n";
        }

        file << "  </characters>\n";
        file << "</font>\n";

        file.close();
        return true;
    } catch (const std::exception &e) {
        m_lastError = "Failed to save XML: " + std::string(e.what());
        return false;
    }
}

bool PhyreFontExtractor::saveFontAsJSON(const FontInfo& fontInfo, const std::string& outputDirectory) {
    try {
        std::string fileName = std::filesystem::path(fontInfo.originalFileName).stem().string() + "_characters.json";
        std::string filePath = (std::filesystem::path(outputDirectory) / fileName).string();

        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_lastError = "Cannot create JSON file: " + filePath;
            return false;
        }

        file << "{\n";
        file << "  \"fontName\": \"" << fontInfo.fontName << "\",\n";
        file << "  \"fontSize\": " << fontInfo.fontSize << ",\n";
        file << "  \"lineSpacing\": " << fontInfo.lineSpacing << ",\n";
        file << "  \"baselineOffset\": " << fontInfo.baselineOffset << ",\n";
        file << "  \"isSDF\": " << (fontInfo.isSDF ? "true" : "false") << ",\n";
        file << "  \"textureWidth\": " << fontInfo.textureWidth << ",\n";
        file << "  \"textureHeight\": " << fontInfo.textureHeight << ",\n";
        file << "  \"characters\": [\n";
        
        for (size_t i = 0; i < fontInfo.characters.size(); ++i) {
            const auto &ch = fontInfo.characters[i];
            file << "    {\n";
            file << "      \"code\": " << ch.characterCode << ",\n";
            file << "      \"width\": " << ch.width << ",\n";
            file << "      \"height\": " << ch.height << ",\n";
            file << "      \"uvX\": " << ch.uvX << ",\n";
            file << "      \"uvY\": " << ch.uvY << ",\n";
            file << "      \"offsetX\": " << ch.offsetX << ",\n";
            file << "      \"offsetY\": " << ch.offsetY << ",\n";
            file << "      \"advanceX\": " << ch.advanceX << ",\n";
            file << "      \"advanceY\": " << ch.advanceY << ",\n";
            file << "      \"rotated\": " << (ch.rotated ? "true" : "false") << ",\n";
            file << "      \"kernPairs\": " << ch.kernPairs << ",\n";
            file << "      \"kernOffset\": " << ch.kernOffset << "\n";
            file << "    }";
            if (i + 1 < fontInfo.characters.size()) file << ",";
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";

        file.close();
        return true;
    } catch (const std::exception &e) {
        m_lastError = "Failed to save JSON: " + std::string(e.what());
        return false;
    }
}

} // namespace PhyreUnpacker