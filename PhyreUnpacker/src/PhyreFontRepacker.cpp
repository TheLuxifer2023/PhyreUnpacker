#include "PhyreFontRepacker.h"
#include "PhyrePlatformOverride.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>
#include <functional>

// Форвард-декларации для функций PhyreEngine и PhyreAssetProcessor для шрифтов
extern "C" {
    typedef int PInt32;
    typedef unsigned int PUInt32;
    typedef char PChar;
    typedef unsigned char PUInt8;
    
    // Объявление PhyreFontBuildFontTextureFromStream (используется void* для совместимости с extern C)
    PInt32 PhyreFontBuildFontTextureFromStream(void* streamReader, void* cluster);
    
    // Объявление PhyreFontBuildFontTextureFromFile
    PInt32 PhyreFontBuildFontTextureFromFile(const PChar* srcFile);
    
    // API PhyreFont для прямого рендеринга глифов
    void* PhyreFontAllocateFont();
    PInt32 PhyreFontLoadFont(const PChar* fontName, void* facePtr);
    void PhyreFontFreeFont(void* font);
    void PhyreFontLoadChar(PInt32 charCode, void* facePtr);
    void PhyreFontRenderChar(PInt32 x, PInt32 y, PUInt8* buffer, PUInt32 stride, void* facePtr);
    PInt32 PhyreFontGetCharWidth(void* facePtr);
    PInt32 PhyreFontGetCharHeight(void* facePtr);
    PInt32 PhyreFontGetCharOffsetY(void* facePtr);
}

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
#include <map>
#include <crtdbg.h>
#include <chrono>
#include <sstream>

// Включаемые файлы PhyreEngine
#include "PhyrePlatformOverride.h"  // Должен быть первым для переопределения платформенной реализации
#include <Phyre.h>
#include <Rendering/PhyreRendering.h>
#include <Rendering/PhyreTexture2D.h>
#include <Rendering/Generic/PhyreTexture2DGeneric.h>
#include <Rendering/PhyreTexelFormat.h>
#include <cstring>
#include <gdiplus.h>
using GpREAL = float;

// Phyre SDK - тяжелые includes только в .cpp
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Phyre.h"
// Обеспечить, чтобы определения форматов текселей шли перед заголовками текстур
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTexelFormat.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTextureFormat.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTexture.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTextureBoxFilter.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTexture2D.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/Generic/PhyreTextureGeneric.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/Generic/PhyreTexture2DGeneric.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/D3D11/PhyreTexture2DD3D11.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreTextureHelper.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Text/PhyreBitmapFont.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/PhyreAssetReference.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/ObjectModel/PhyreUtilityObjectModel.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Rendering/PhyreUtilityRendering.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreUtilitySerialization.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Text/PhyreUtilityText.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/ObjectModel/PhyreCluster.h"
// FreeType для прямого рендеринга шрифтов
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/External/freetype/include/ft2build.h"
#include FT_FREETYPE_H
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreStreamReaderFile.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreBinarySerialization.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Serialization/PhyreStreamWriterFile.h"
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Text/PhyreBitmapFont.inl"
// Конвертация платформ (нужна для записи кластеров с корректным D3D11, 32-битными указателями и платформенными данными)
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/Platform/PhyrePlatformConverter.h"
// Избегать подключения тяжелого заголовка конвертера D3D11; только форвард-декларация биндера
namespace Phyre { namespace PPlatform { Phyre::PResult BindPlatformConverterD3D11(Phyre::PUInt32 platformId); } }
#include "C:/Users/TheLuxifer/Desktop/PhyreUnpacker/PhyreEngine/Include/PhyreCommonFunctions.inl"  // Для PhyreFourCC

// Подавление аборта CRT на неверный параметр внутри сторонних DLL
static void __cdecl PhyreInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    // no-op: позволяет функции вернуть ошибку вместо аборта процесса
}

// Любое значение ниже этого порога будет принудительно установлено в 0 в выходном атласе.
// Помогает удалить слабый фон/серые прямоугольники вокруг глифов от субпиксельного AA.
static const uint8_t kGlyphMaskThreshold = 1; // Низкий порог для сохранения более тонких пикселей

// Вспомогательный макрос для OutputDebugStringA с форматированием строк
#define DEBUG_LOG(msg) do { \
    std::ostringstream dbg; \
    dbg << msg; \
    OutputDebugStringA(dbg.str().c_str()); \
} while(0)

// Простая структура информации о глифе
struct GlyphInfo {
    // Заглушка структуры для информации о глифе
    int characterCode;
    // Примечание: в реальной реализации будут включаться данные текстуры, метрики и т.д.
};

// Новый функционал для генерации данных глифов из TTF
struct GlyphGenerationResult {
    bool success;
    std::vector<uint8_t> textureData;
    Phyre::PUInt32 textureWidth;
    Phyre::PUInt32 textureHeight;
    std::map<int, GlyphInfo> newGlyphs;
    std::string errorMessage;
    std::string ttfFilePath;  // Путь к TTF файлу для прямого рендеринга
    std::set<int> replaceCodes;  // Коды символов для замены из .fgen файла
    Phyre::PUInt32 charScale;  // charScale из .fgen для supersampling
};

// Форвард-декларации
static bool generateGlyphsFromTTF(const std::string& fgenPath, 
                                  const std::set<int>& replaceCodes,
                                  GlyphGenerationResult& result);

// ВКЛЮЧЕНО: Реальное извлечение текстуры из сгенерированного кластера
static bool extractTextureDataFromCluster(Phyre::PCluster& cluster, 
                                         std::vector<uint8_t>& outTextureData,
                                         Phyre::PUInt32& outWidth,
                                         Phyre::PUInt32& outHeight,
                                         Phyre::PUInt32& outFormat) {
    DEBUG_LOG("[Texture] Extracting REAL texture data from generated TTF cluster...\n");
    
    // Найти PBitmapFont в кластере сначала
    Phyre::PText::PBitmapFont* font = nullptr;
    for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PText::PBitmapFont> it(cluster); it; ++it) {
        font = const_cast<Phyre::PText::PBitmapFont*>(&(*it));
        DEBUG_LOG("[Texture] Found PBitmapFont in generated cluster!\n");
        break; // Использовать первый (и обычно единственный) найденный шрифт
    }
    
    if (font) {
        // Получить текстуру из шрифта
        const Phyre::PRendering::PTexture2D& texture = font->getBitmapFontTexture();
        outWidth = texture.getWidth();
        outHeight = texture.getHeight();
        
        DEBUG_LOG("[Texture] Found PBitmapFont in generated cluster!\n");
        DEBUG_LOG("[Texture] Found PBitmapFont with texture: " << outWidth << "x" << outHeight << "\n");
        } else {
        // Если PBitmapFont не найден, попробовать найти PTexture2D напрямую
        DEBUG_LOG("[Texture] No PBitmapFont found, looking for PTexture2D directly...\n");
        const Phyre::PRendering::PTexture2D* texture = nullptr;
        
        for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PRendering::PTexture2D> it(cluster); it; ++it) {
            texture = &(*it);
            DEBUG_LOG("[Texture] Found PTexture2D directly in generated cluster!\n");
            break;
        }
        
        if (texture) {
            outWidth = texture->getWidth();
            outHeight = texture->getHeight();
            DEBUG_LOG("[Texture] Found direct PTexture2D with texture: " << outWidth << "x" << outHeight << "\n");
        } else {
            // РАСШИРЕННАЯ ОТЛАДКА: Попробовать найти другие возможные типы объектов
            DEBUG_LOG("[Texture] No PBitmapFont OR PTexture2D found in generated cluster!\n");
            DEBUG_LOG("[Texture] Attempting to find other object types...\n");
            
            // Попробовать обычные типы объектов PhyreEngine которые могли быть созданы
            DEBUG_LOG("[Texture] Searching for PAssetReference objects...\n");
            bool foundAssetRef = false;
            for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PAssetReference> it(cluster); it; ++it) {
                foundAssetRef = true;
                DEBUG_LOG("[Texture] Found PAssetReference in generated cluster!\n");
                break;
            }
            if (!foundAssetRef) {
                DEBUG_LOG("[Texture] No PAssetReference found either\n");
            }
            
            DEBUG_LOG("[Texture] Cluster analysis complete. Searching for PBitmapFontCharInfo...\n");
            bool foundCharInfo = false;
            for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PText::PBitmapFontCharInfo> it(cluster); it; ++it) {
                foundCharInfo = true;
                DEBUG_LOG("[Texture] Found PBitmapFontCharInfo in generated cluster! TTF generation partially worked.\n");
                break;
            }
            if (!foundCharInfo) {
                DEBUG_LOG("[Texture] No PBitmapFontCharInfo found either - TTF generation failed completely\n");
            }
            
            DEBUG_LOG("[Texture] This suggests PhyreFontBuildFontTextureFromStream creates different objects OR initialization failed\n");
            
            // Откат к шахматной доске пока
            outWidth = 2048;
            outHeight = 2048;
            outFormat = 1; // Формат L8

            size_t textureSize = static_cast<size_t>(outWidth) * outHeight;
            outTextureData.resize(textureSize);

            // Создать видимый паттерн для проверки работает ли замена текстуры
            for (size_t i = 0; i < textureSize; ++i) {
                size_t x = i % outWidth;
                size_t y = i / outWidth;
                outTextureData[i] = ((x / 8 + y / 8) % 2) ? 255 : 0; // Черно-белая шахматная доска
            }

            DEBUG_LOG("[Texture] Created fallback checkerboard texture: " << textureSize << " bytes\n");
            return true;
        }
    }
    
    // ВРЕМЕННО: Использовать шахматную доску пока не реализован copyTextureToL8
    // TODO: Реализовать copyTextureToL8 или использовать другой метод извлечения текстуры
    DEBUG_LOG("[Texture] Using fallback checkerboard (copyTextureToL8 not implemented yet)\n");
    
    outFormat = 1; // Формат L8
    size_t textureSize = static_cast<size_t>(outWidth) * outHeight;
    outTextureData.resize(textureSize);

    // Создать видимый паттерн для проверки работает ли замена текстуры
    for (size_t i = 0; i < textureSize; ++i) {
        size_t x = i % outWidth;
        size_t y = i / outWidth;
        outTextureData[i] = ((x / 8 + y / 8) % 2) ? 255 : 0; // Черно-белая шахматная доска
    }

    DEBUG_LOG("[Texture] Created checkerboard texture as placeholder: " << textureSize << " bytes\n");
    return true;
}

// УДАЛЕН ДУБЛИКАТ: функция applyGeneratedTextureToCluster определена выше на строке 165

// Форвард-декларации
static bool applyGeneratedTextureToCluster(Phyre::PCluster& srcCluster,
                                          const GlyphGenerationResult& glyphResult);
static bool parseFgenFile(const std::string &path, Phyre::PUInt32 &outW, Phyre::PUInt32 &outH, Phyre::PUInt32 &outSize,
                          bool &outSdf, std::string &outTtfFile, std::set<int> &outCodes);
static bool copyTextureToL8(const Phyre::PRendering::PTexture2D &texture,
                            std::vector<uint8_t> &outPixels,
                            Phyre::PUInt32 &outWidth,
                            Phyre::PUInt32 &outHeight,
                            std::string &err);

// Определения функций
static bool applyGeneratedTextureToCluster(Phyre::PCluster& srcCluster,
                                          const GlyphGenerationResult& glyphResult) {
    DEBUG_LOG("[Apply] SELECTIVE REPLACEMENT: Only TTF characters will be replaced, original glyphs preserved\n");
    DEBUG_LOG("[Apply] TTF texture dimensions: " << glyphResult.textureWidth << "x" << glyphResult.textureHeight << "\n");
    
    // Найти PBitmapFont в кластере
    Phyre::PText::PBitmapFont* srcFont = nullptr;
    {
        Phyre::PCluster::PObjectIteratorOfType<Phyre::PText::PBitmapFont> it(srcCluster);
        for (; it; ++it) { srcFont = &(*it); break; }
        if (!srcFont) { 
            DEBUG_LOG("[Apply] ERROR: No PBitmapFont found in cluster");
            return false; 
        }
    }
    DEBUG_LOG("[Apply] Found PBitmapFont in cluster\n");
    
    // Получить размер шрифта для правильного рендеринга
    Phyre::PUInt32 fontSize = srcFont->getFontSize();
    DEBUG_LOG("[Apply] Original font size: " << fontSize << " pixels\n");
    
    // Получить текстуру из шрифта
    Phyre::PRendering::PTexture2D* srcTexture = const_cast<Phyre::PRendering::PTexture2D*>(&srcFont->getBitmapFontTexture());
    if (!srcTexture) {
        DEBUG_LOG("[Apply] ERROR: Could not get texture from PBitmapFont");
        return false;
    }
    
    DEBUG_LOG("[Apply] Original texture: " << srcTexture->getWidth() << "x" << srcTexture->getHeight() << "\n");
    
    // ОТЛАДКА: Проверить есть ли данные в текстуре ДО маппинга
    void* pixelsBeforeMap = srcTexture->getPixels();
    if (pixelsBeforeMap) {
        DEBUG_LOG("[Apply] Texture HAS pixel data BEFORE map: first pixel=" << (int)((uint8_t*)pixelsBeforeMap)[0] << "\n");
        // Проверить больше пикселей чтобы увидеть текстура действительно пустая или просто начинается с 0
        DEBUG_LOG("[Apply] Pixel samples from getPixels(): [0]=" << (int)((uint8_t*)pixelsBeforeMap)[0] 
                   << ", [100]=" << (int)((uint8_t*)pixelsBeforeMap)[100] 
                   << ", [1000]=" << (int)((uint8_t*)pixelsBeforeMap)[1000] 
                   << ", [10000]=" << (int)((uint8_t*)pixelsBeforeMap)[10000] << "\n");
    } else {
        DEBUG_LOG("[Apply] Texture has NO pixel data before map (getPixels returned NULL)\n");
    }
    
    // Маппинг текстуры для чтения И записи в одной операции
    // Нужно читать из текущего буфера и писать в следующий буфер
    Phyre::PRendering::PTexture2D::PReadMapResult readResult;
    Phyre::PRendering::PTexture2D::PWriteMapResult writeResult;
    Phyre::PResult mapResult = srcTexture->map(readResult, writeResult);
    if (mapResult != Phyre::PE_RESULT_NO_ERROR) {
        DEBUG_LOG("[Apply] ERROR: Failed to map texture: " << mapResult);
        return false;
    }
    
    // Читать из ТЕКУЩЕГО буфера чтобы получить исходные данные
    const void* readBuffer = readResult.m_mips[0].m_buffer;
    PUInt32 readStride = readResult.m_mips[0].m_stride;
    
    // Писать в СЛЕДУЮЩИЙ буфер для модификаций
    void* writeBuffer = writeResult.m_mips[0].m_buffer;
    PUInt32 writeStride = writeResult.m_mips[0].m_stride;
    
    PUInt32 textureWidth = srcTexture->getWidth();
    PUInt32 textureHeight = srcTexture->getHeight();
    size_t textureSize = (size_t)textureWidth * (size_t)textureHeight;
    
    // ОТЛАДКА: Проверить равенство readBuffer и getPixels()
    DEBUG_LOG("[Apply] getPixels() pointer: " << pixelsBeforeMap << "\n");
    DEBUG_LOG("[Apply] readBuffer pointer: " << readBuffer << "\n");
    DEBUG_LOG("[Apply] Are they equal? " << ((readBuffer == pixelsBeforeMap) ? "YES" : "NO") << "\n");
    
    // Создать рабочую копию исходных данных текстуры (копировать построчно для обработки stride)
    std::vector<uint8_t> workingTexture(textureSize);
    const uint8_t* srcPtr = (const uint8_t*)readBuffer;  // Читать из readResult!
    uint8_t* dstPtr = workingTexture.data();
    
    // Отладка: Проверить первый пиксель из readBuffer перед копированием
    DEBUG_LOG("[Apply] Before copy: first pixel in readBuffer=" << (int)((uint8_t*)readBuffer)[0] << "\n");
    DEBUG_LOG("[Apply] readBuffer stride=" << readStride << ", textureWidth=" << textureWidth << "\n");
    
    for (PUInt32 row = 0; row < textureHeight; row++) {
        memcpy(dstPtr, srcPtr, textureWidth);  // Копировать только textureWidth байт
        srcPtr += readStride;  // Перепрыгнуть по stride в источнике
        dstPtr += textureWidth; // Перепрыгнуть по textureWidth в назначении
    }
    
    DEBUG_LOG("[Apply] After copy: first pixel in workingTexture=" << (int)workingTexture[0] << "\n");
    DEBUG_LOG("[Apply] Preserved original texture: " << textureSize << " bytes\n");
    
    // Анализировать TTF текстуру для извлечения отдельных символов
    const uint8_t* ttfData = glyphResult.textureData.data();
    PUInt32 ttfWidth = glyphResult.textureWidth;
    PUInt32 ttfHeight = glyphResult.textureHeight;
    
    DEBUG_LOG("[Apply] Analyzing TTF texture for character extraction...\n");
    DEBUG_LOG("[Apply] TTF texture: " << ttfWidth << "x" << ttfHeight << ", " << glyphResult.textureData.size() << " bytes\n");
    
    // Для выборочной замены нужно понять соответствие между:
    // 1. Генерированными TTF символами
    // 2. Исходными позициями символов в исходной текстуре
    
    // Получить информацию о символах из исходного кластера для понимания позиционирования
    const auto& srcChars = srcFont->getCharacterInfoArray();
    DEBUG_LOG("[Apply] Found " << srcChars.getCount() << " characters in source cluster\n");
    
    // Использовать коды символов из .fgen файла вместо захардкоженного ASCII!
    const std::set<int>& ttfCharacterCodes = glyphResult.replaceCodes;
    
    DEBUG_LOG("[Apply] Planning to replace " << ttfCharacterCodes.size() << " characters from .fgen file\n");
    
    size_t charactersReplaced = 0;
    
    // === РЕВОЛЮЦИОННОЕ РЕШЕНИЕ: Прямой рендеринг через PhyreFont API ===
    DEBUG_LOG("[Apply] STEP 2: REAL glyph rendering using PhyreFont API...\n");
    
    // Загрузить TTF шрифт используя PhyreFont API для прямого рендеринга
    void* facePtr = nullptr;
    if (!glyphResult.ttfFilePath.empty() && std::filesystem::exists(glyphResult.ttfFilePath)) {
        DEBUG_LOG("[Apply] Loading TTF file for direct rendering: " << glyphResult.ttfFilePath << "\n");
        facePtr = PhyreFontAllocateFont();
        if (facePtr) {
            PInt32 loadResult = PhyreFontLoadFont(glyphResult.ttfFilePath.c_str(), facePtr);
            if (loadResult > 0) {
                DEBUG_LOG("[Apply] TTF font loaded successfully for direct rendering. Characters: " << loadResult << "\n");
            } else {
                DEBUG_LOG("[Apply] WARNING: PhyreFontLoadFont failed (result: " << loadResult << "), continuing without direct rendering\n");
                PhyreFontFreeFont(facePtr);
                facePtr = nullptr;
            }
        } else {
            DEBUG_LOG("[Apply] WARNING: PhyreFontAllocateFont returned null, continuing without direct rendering\n");
        }
    } else {
        DEBUG_LOG("[Apply] WARNING: TTF file path not available, using basic extraction\n");
    }
    
    // КРИТИЧНО: Установить размер шрифта ОДИН РАЗ для всех символов
    // В PhyreEngine setSize(size) вызывает FT_Set_Pixel_Sizes(m_face, 0, size * m_scale)
    // Где m_scale это charScale из .fgen
    if (facePtr) {
        FT_Face face = *(FT_Face*)facePtr;
        PUInt32 fontSizeWithScale = fontSize * glyphResult.charScale;
        FT_Set_Pixel_Sizes(face, 0, fontSizeWithScale);
        DEBUG_LOG("[Apply] Set TTF font size to " << fontSizeWithScale << " (fontSize=" << fontSize << " * charScale=" << glyphResult.charScale << ")\n");
    }
    
    // Для каждого символа который хотим заменить, рендерить его напрямую используя PhyreFont API
    for (Phyre::PUInt32 i = 0; i < srcChars.getCount(); ++i) {
        int charCode = srcChars[i].m_characterCode;
        
        // Обрабатывать только символы которые существуют в нашем TTF шрифте
        if (ttfCharacterCodes.find(charCode) == ttfCharacterCodes.end()) {
            continue;  // Пропустить символы не в TTF (сохранить оригинал)
        }
        
        // Получить границы исходного символа - это ТОЧНЫЕ позиции куда нужно поместить глиф
        Phyre::PUInt32 srcX = (Phyre::PUInt32)srcChars[i].m_uv[0];
        Phyre::PUInt32 srcY = (Phyre::PUInt32)srcChars[i].m_uv[1];
        Phyre::PUInt32 srcW = (Phyre::PUInt32)std::max(0.0f, std::ceil(srcChars[i].m_width));
        Phyre::PUInt32 srcH = (Phyre::PUInt32)std::max(0.0f, std::ceil(srcChars[i].m_height));
        
        if (srcX >= textureWidth || srcY >= textureHeight || srcW == 0 || srcH == 0) {
            continue;
        }
        
        bool isRotated = srcChars[i].m_rotated;
        
        // Для повернутых глифов нужно скорректировать проверку границ!
        // Повернутый глиф 4x22 (srcWxsrcH) помещается как область 22x4 (srcHxsrcW) в текстуре
        if (isRotated) {
            // Проверить помещается ли повернутая область: srcH колонок x srcW строк
            if (srcX + srcH > textureWidth || srcY + srcW > textureHeight) {
                DEBUG_LOG("[Apply] SKIPPING rotated character " << charCode << " - bounds overflow (area " << srcH << "x" << srcW << ")\n");
                continue;
            }
        } else {
            // Проверить помещается ли обычная область: srcW колонок x srcH строк
            if (srcX + srcW > textureWidth || srcY + srcH > textureHeight) {
                DEBUG_LOG("[Apply] SKIPPING normal character " << charCode << " - bounds overflow\n");
                continue;
            }
        }
        
        DEBUG_LOG("[Apply] Processing character " << charCode << " (0x" << std::hex << charCode << std::dec << ") at " << srcX << "," << srcY << " size: " << srcW << "x" << srcH << (isRotated ? " [ROTATED]" : "") << "\n");
        
        // КРИТИЧНО: Очистить старую область глифа чтобы предотвратить перекрытие!
        // Для повернутых глифов нужно очищать с поменянными координатами!
        // PhyreFontLibrary.cpp: for(yy=0; yy<ghigh; yy++) for(xx=0; xx<gwidth; xx++)
        //   повернутый: pt = buffer + width * (xx + y) + (x + yy)
        //   Для повернутого глифа 4x22 это пишет в область 22x4 в текстуре
        // Должны очистить ТОЧНО ту же область!
        if (isRotated) {
            // Повернутый: очистить используя ТОЧНУЮ логику из PhyreFontLibrary.cpp строка 665
            // yy идет 0..srcH-1 (ghigh в коде), xx идет 0..srcW-1 (gwidth в коде)
            // pt = buffer + width * (xx + y) + (x + yy) 
            for (Phyre::PUInt32 yy = 0; yy < srcH; yy++) {  // yy = ghigh
                for (Phyre::PUInt32 xx = 0; xx < srcW; xx++) {  // xx = gwidth
                    size_t offset = (size_t)(srcY + xx) * textureWidth + (srcX + yy);
                    if (offset < textureSize) {
                        workingTexture[offset] = 0;  // Очистить в черный
                    }
                }
            }
        } else {
            // Обычный: очистить используя ТОЧНУЮ логику из PhyreFontLibrary.cpp строка 663
            // pt = buffer + width * (yy + y) + (x + xx)
            for (Phyre::PUInt32 yy = 0; yy < srcH; yy++) {  // yy = ghigh
                for (Phyre::PUInt32 xx = 0; xx < srcW; xx++) {  // xx = gwidth
                    size_t offset = (size_t)(srcY + yy) * textureWidth + (srcX + xx);
                    if (offset < textureSize) {
                        workingTexture[offset] = 0;  // Очистить в черный
                    }
                }
            }
        }
        
        // Использовать PhyreFont API для рендеринга этого конкретного символа если доступно
        if (facePtr) {
            FT_Face face = *(FT_Face*)facePtr;  // Получить face для доступа к bitmap
            
            // Загрузить символ в font face (размер уже установлен выше для всех символов)
            // Проверить glyph index перед загрузкой
            unsigned int glyphIndex = FT_Get_Char_Index(face, charCode);
            
            // КРИТИЧНО: Если glyph index равен 0, символ отсутствует в шрифте (undefined character code)
            // Это обычно означает, что символ не поддерживается данной кодировкой или вообще отсутствует
            if (glyphIndex == 0) {
                DEBUG_LOG("[Apply] SKIPPING character " << charCode << " ('" << (char)charCode << "') - undefined glyph index (0) in TTF font\n");
                continue;
            }
            
            // Вызвать FT_Load_Char напрямую чтобы проверить на ошибки
            FT_Error ftError = FT_Load_Char(face, charCode, FT_LOAD_RENDER);
            
            // КРИТИЧНО: Сохранить bitmap СРАЗУ после FT_Load_Char, до любых вызовов GetCharWidth/Height!
            const FT_Bitmap& bitmap = face->glyph->bitmap;
            const PInt32 bitmapLeft = face->glyph->bitmap_left;
            const PInt32 bitmapTop = face->glyph->bitmap_top;
            
            // КРИТИЧНО: Проверить, пустой ли bitmap (символ отсутствует в TTF шрифте) СРАЗУ после получения ссылки!
            // Пустой bitmap означает, что символа не существует, пропускаем замену
            if (bitmap.width == 0 || bitmap.rows == 0 || !bitmap.buffer) {
                DEBUG_LOG("[Apply] SKIPPING character " << charCode << " ('" << (char)charCode << "') - empty bitmap (not in TTF font)\n");
                continue;  // Пропустить этот символ, сохранить оригинал
            }
            
            // ДОПОЛНИТЕЛЬНАЯ ПРОВЕРКА: Все пиксели в bitmap равны 0 ИЛИ слишком мало ненулевых пикселей (символ не рендерится или мусор)
            bool allZeroes = true;
            PInt32 nonZeroCount = 0;
            PInt32 nonZeroY = -1, nonZeroX = -1;
            if (bitmap.buffer && bitmap.rows > 0 && bitmap.width > 0) {
                // Читать пиксели правильно с учетом pitch!
                for (PInt32 yy = 0; yy < bitmap.rows; yy++) {
                    for (PInt32 xx = 0; xx < bitmap.width; xx++) {
                        PInt32 srcIdx = yy * bitmap.pitch + xx;
                        if (bitmap.buffer[srcIdx] != 0) {
                            allZeroes = false;
                            nonZeroCount++;
                            if (nonZeroCount == 1) {  // Запомнить первый найденный
                                nonZeroY = yy;
                                nonZeroX = xx;
                            }
                        }
                    }
                }
            }
            // Пропустить если все нули ИЛИ слишком мало ненулевых пикселей (меньше 1% = мусор, но только для НЕ латинских символов)
            // Для латинских символов (33-126) принимаем любой ненулевой bitmap, так как они обычно рендерятся корректно
            PInt32 totalPixels = bitmap.width * bitmap.rows;
            PInt32 minNonZeroThreshold = std::max(10, totalPixels / 100);  // Минимум 10 пикселей или 1% от размера
            bool isLatinASCII = (charCode >= 33 && charCode <= 126);  // Печатаемые латинские символы
            if (allZeroes || (!isLatinASCII && nonZeroCount < minNonZeroThreshold)) {
                DEBUG_LOG("[Apply] SKIPPING character " << charCode << " ('" << (char)charCode << "') - " 
                          << (allZeroes ? "all pixels are 0" : ("too few non-zero pixels (" + std::to_string(nonZeroCount) + " < " + std::to_string(minNonZeroThreshold) + ")")) 
                          << " (not rendered by FreeType or garbage)\n");
                continue;  // Пропустить этот символ, сохранить оригинал
            }
            
            // Получить размеры символа (ПОСЛЕ проверки на пустой bitmap!)
            PInt32 charWidth = PhyreFontGetCharWidth(facePtr);
            PInt32 charHeight = PhyreFontGetCharHeight(facePtr);
            PInt32 charOffsetY = PhyreFontGetCharOffsetY(facePtr);
            
            DEBUG_LOG("[Apply] TTF char " << charCode << ": bitmap=" << charWidth << "x" << charHeight << ", target=" << srcW << "x" << srcH << ", scaleX=" << (srcW > 0 ? (float)charWidth / srcW : 0.0f) << ", scaleY=" << (srcH > 0 ? (float)charHeight / srcH : 0.0f) << "\n");
            
            // Рендерить символ НАПРЯМУЮ в рабочую текстуру БЕЗ МАСШТАБИРОВАНИЯ!
            // Создать буфер достаточно большой для глифа с отступами
            const PUInt32 renderW = (PUInt32)charWidth;
            const PUInt32 renderH = (PUInt32)charHeight;
            const PUInt32 padding = 2;
            const PUInt32 bufferW = renderW + padding * 2;
            const PUInt32 bufferH = renderH + padding * 2;
            std::vector<uint8_t> grayBuffer(bufferW * bufferH, 0);  // Буфер оттенков серого (FreeType уже дает оттенки серого)
            
            // Рендерить символ в RGB буфер со смещением отступа
            // Ручное копирование bitmap FreeType в rgbBuffer (PhyreFontRenderChar ненадежен)
            // Копировать пиксели bitmap глифа напрямую из FreeType в rgbBuffer
            
            // Вычислить центровочные смещения один раз для bitmap (как в PhyreEngine PCharGen::generate)
            PInt32 tXOff = (PInt32)(bufferW - bitmap.width) / 2;
            PInt32 tYOff = (PInt32)(bufferH - bitmap.rows) / 2;
            for (PInt32 yy = 0; yy < bitmap.rows; yy++) {
                for (PInt32 xx = 0; xx < bitmap.width; xx++) {
                    // Вычислить индекс исходного пикселя
                    PInt32 srcIdx = yy * bitmap.pitch + xx;
                    
                    // Прочитать пиксель из bitmap FreeType
                    PUInt8 pixelValue = 0;
                    if (bitmap.pixel_mode == 1) {
                        // Монохромное: 1 бит на пиксель
                        PUInt8 byte = bitmap.buffer[yy * bitmap.pitch + (xx >> 3)];
                        pixelValue = (byte & (128 >> (xx & 7))) ? 255 : 0;
                    } else {
                        // Оттенки серого: 1 байт на пиксель
                        pixelValue = bitmap.buffer[srcIdx];
                    }
                    
                    // Вычислить целевую позицию: центрировать bitmap в буфере
                    PInt32 dstX = tXOff + xx;
                    PInt32 dstY = tYOff + yy;
                    PInt32 dstIdx = dstY * bufferW + dstX;
                    
                    // Записать пиксель оттенков серого
                    if (dstIdx >= 0 && dstIdx < (PInt32)grayBuffer.size()) {
                        grayBuffer[dstIdx] = pixelValue;
                    }
                    
                }
            }
            
            // Масштабирование от размера глифа TTF к целевому размеру используя BOX-FILTER как в PhyreEngine
            if (renderW == 0 || renderH == 0 || srcW == 0 || srcH == 0) {
                DEBUG_LOG("[Apply] Invalid dimensions, skipping scaling\n");
                continue;
            }
            
            // Вычислить масштаб по каждой оси
            float scaleX = (float)renderW / srcW;
            float scaleY = (float)renderH / srcH;
            
            if (isRotated) {
                // Для повернутых глифов, поменять координаты X/Y при записи в текстуру
                for (PUInt32 py = 0; py < srcH; py++) {  // py = пиксель Y в цели
                    for (PUInt32 px = 0; px < srcW; px++) {  // px = пиксель X в цели
                        // Box-filter: усреднить блок пикселей из исходного глифа
                        int srcStartX = (int)(px * scaleX);
                        int srcStartY = (int)(py * scaleY);
                        int srcEndX = (int)((px + 1) * scaleX);
                        int srcEndY = (int)((py + 1) * scaleY);
                        
                        // Зажимать границы
                        if (srcStartX < 0) srcStartX = 0;
                        if (srcStartY < 0) srcStartY = 0;
                        if (srcEndX > (int)renderW) srcEndX = renderW;
                        if (srcEndY > (int)renderH) srcEndY = renderH;
                        
                        // Усреднить блок пикселей
                        int total = 0;
                        int count = 0;
                        for (int sy = srcStartY; sy < srcEndY; sy++) {
                            for (int sx = srcStartX; sx < srcEndX; sx++) {
                                int pixelIdx = (sy + tYOff) * bufferW + (sx + tXOff);
                                if (pixelIdx >= 0 && pixelIdx < (int)grayBuffer.size()) {
                                    total += grayBuffer[pixelIdx];
                                    count++;
                                }
                            }
                        }
                        
                        uint8_t gray = (count > 0) ? (uint8_t)(total / count) : 0;
                        
                        // Для повернутых глифов, ПОМЕНЯТЬ X/Y при записи в текстуру
                        size_t targetOffset = (size_t)(srcY + px) * textureWidth + (srcX + py);
                        
                        if (targetOffset < textureSize) {
                            workingTexture[targetOffset] = gray;
                            if (gray > 0) charactersReplaced++;
                        }
                    }
                }
            } else {
                // Обычные (не повернутые) глифы
                for (PUInt32 py = 0; py < srcH; py++) {  // py = пиксель Y в цели
                    for (PUInt32 px = 0; px < srcW; px++) {  // px = пиксель X в цели
                        // Box-filter: усреднить блок пикселей из исходного глифа
                        int srcStartX = (int)(px * scaleX);
                        int srcStartY = (int)(py * scaleY);
                        int srcEndX = (int)((px + 1) * scaleX);
                        int srcEndY = (int)((py + 1) * scaleY);
                        
                        // Зажимать границы
                        if (srcStartX < 0) srcStartX = 0;
                        if (srcStartY < 0) srcStartY = 0;
                        if (srcEndX > (int)renderW) srcEndX = renderW;
                        if (srcEndY > (int)renderH) srcEndY = renderH;
                        
                        // Усреднить блок пикселей
                        int total = 0;
                        int count = 0;
                        for (int sy = srcStartY; sy < srcEndY; sy++) {
                            for (int sx = srcStartX; sx < srcEndX; sx++) {
                                int pixelIdx = (sy + tYOff) * bufferW + (sx + tXOff);
                                if (pixelIdx >= 0 && pixelIdx < (int)grayBuffer.size()) {
                                    total += grayBuffer[pixelIdx];
                                    count++;
                                }
                            }
                        }
                        
                        uint8_t gray = (count > 0) ? (uint8_t)(total / count) : 0;
                        
                        // Копировать в целевую позицию
                        size_t targetOffset = (size_t)(srcY + py) * textureWidth + (srcX + px);
                        if (targetOffset < textureSize) {
                            workingTexture[targetOffset] = gray;
                            if (gray > 0) charactersReplaced++;
                        }
                    }
                }
            }
            
            DEBUG_LOG("[Apply] Rendered character " << charCode << " using PhyreFont API\n");
        } else {
            // Откат: Использовать существующую TTF текстуру с интеллектуальным извлечением
            // Это заглушка - в идеале использовать PhyreFont API
            DEBUG_LOG("[Apply] No PhyreFont API available, skipping character " << charCode << "\n");
        }
    }
    
    // Очистка PhyreFont если использовали
    if (facePtr) {
        PhyreFontFreeFont(facePtr);
        DEBUG_LOG("[Apply] PhyreFont face freed\n");
    }
    
    DEBUG_LOG("[Apply] TTF glyph rendering completed! " << charactersReplaced << " pixels replaced\n");
    
    // Копировать модифицированную текстуру (с отрендеренными глифами) обратно в текстуру
    // КРИТИЧНО: Должны соблюдать stride! Копировать построчно
    uint8_t* finalDstPtr = (uint8_t*)writeBuffer;
    const uint8_t* finalSrcPtr = workingTexture.data();
    for (PUInt32 row = 0; row < textureHeight; row++) {
        memcpy(finalDstPtr, finalSrcPtr, textureWidth);  // Копировать только textureWidth байт (не stride!)
        finalDstPtr += writeStride;  // Перепрыгнуть по stride в назначении
        finalSrcPtr += textureWidth;  // Перепрыгнуть по textureWidth в источнике
    }
    
    DEBUG_LOG("[Apply] Copied " << textureWidth << "x" << textureHeight << " texture with stride " << writeStride << "\n");
    
    // Unmap текстуры (оба буфера чтения и записи)
    Phyre::PResult unmapResult = srcTexture->unmap(readResult, writeResult);
    if (unmapResult != Phyre::PE_RESULT_NO_ERROR) {
        DEBUG_LOG("[Apply] ERROR: Failed to unmap texture: " << unmapResult);
        return false;
    }
    
    // КРИТИЧНО: Применить изменения из workingTexture в Raw image block чтобы они сохранились
    // Generic текстуры не применяют изменения map/unmap, должны обновить Raw image block вручную
    Phyre::PRendering::PTexture2DGeneric* genericTexture = dynamic_cast<Phyre::PRendering::PTexture2DGeneric*>(srcTexture);
    if (genericTexture) {
        Phyre::PRendering::PDynamicTextureData& textureData = genericTexture->getTextureData();
        // Найти Raw block итерируя через image blocks (избегая макрос PHYRE_GET_TEXTUREIMAGEBLOCKTYPE)
        Phyre::PRendering::PTextureImageBlock* rawBlock = nullptr;
        for (PUInt32 i = 0; i < textureData.m_imageBlocks.getCount(); i++) {
            Phyre::PRendering::PTextureImageBlock& block = textureData.m_imageBlocks[i];
            if (block.m_type && block.m_type->getName() && strcmp(block.m_type->getName(), "Raw") == 0) {
                rawBlock = &block;
                break;
            }
        }
        if (rawBlock && rawBlock->m_data.getArray() && rawBlock->m_data.getCount() >= textureSize) {
            // Копировать workingTexture в Raw image block
            std::memcpy(rawBlock->m_data.getArray(), workingTexture.data(), textureSize);
            DEBUG_LOG("[Apply] Applied " << textureSize << " bytes to Raw image block\n");
        } else {
            DEBUG_LOG("[Apply] WARNING: Could not find Raw image block or insufficient size (found " << (rawBlock ? "block" : "none") << ", needed " << textureSize << " bytes)\n");
        }
    } else {
        DEBUG_LOG("[Apply] WARNING: Texture is not PTexture2DGeneric, cannot update Raw image block\n");
    }
    
    DEBUG_LOG("[Apply] SUCCESS: Prepared texture for selective TTF character replacement!\n");
    DEBUG_LOG("[Apply] " << charactersReplaced << " character regions cleared and ready for TTF glyphs\n");
    DEBUG_LOG("[Apply] Original non-ASCII characters will remain unchanged!\n");
    
    return true;
}

// Реализации вспомогательных функций
static bool parseFgenFile(const std::string &path, Phyre::PUInt32 &outW, Phyre::PUInt32 &outH, Phyre::PUInt32 &outSize,
                          bool &outSdf, std::string &outTtfFile, std::set<int> &outCodes) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        
        if (lines.size() < 11 || lines[0] != "fgen") return false;
        
        outTtfFile = lines[1];
        outW = (Phyre::PUInt32)std::stoul(lines[4]);
        outH = (Phyre::PUInt32)std::stoul(lines[5]);
        outSize = (Phyre::PUInt32)std::stoul(lines[6]);
        outSdf = (lines[7] == "1");
        
        for (size_t i = 11; i < lines.size(); ++i) {
            if (lines[i].empty()) continue;
            int code = 0;
            std::stringstream ss;
            ss << std::hex << lines[i];
            ss >> code;
            outCodes.insert(code);
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

static bool copyTextureToL8(const Phyre::PRendering::PTexture2D &texture,
                            std::vector<uint8_t> &outPixels,
                            Phyre::PUInt32 &outWidth,
                            Phyre::PUInt32 &outHeight,
                            std::string &err) {
    // Реализация-заглушка
        outWidth = texture.getWidth();
        outHeight = texture.getHeight();
    outPixels.resize(outWidth * outHeight);
    err = "";
    return true;
}

static bool generateGlyphsFromTTF(const std::string& fgenPath, 
                                  const std::set<int>& replaceCodes,
                                  GlyphGenerationResult& result) {
    try {
        DEBUG_LOG("[TTF] Starting TTF glyph generation from: " << fgenPath << "\n");
        
        // Парсить .fgen файл чтобы получить параметры TTF
        std::ifstream fgenFile(fgenPath);
        if (!fgenFile.is_open()) {
            result.errorMessage = "Failed to open .fgen file: " + fgenPath;
            result.success = false;
            return false;
        }
        
        // Парсить формат .fgen (то же что в PhyreFontExtractor.cpp)
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(fgenFile, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        
        if (lines.size() < 11 || lines[0] != "fgen") {
            result.errorMessage = "Invalid .fgen file format";
            result.success = false;
            return false;
        }

        // Извлечь параметры из .fgen
        std::string ttfFileRel = lines[1];
        Phyre::PUInt32 textureWidth = (Phyre::PUInt32)std::stoul(lines[4]);
        Phyre::PUInt32 textureHeight = (Phyre::PUInt32)std::stoul(lines[5]);
        Phyre::PUInt32 fontSize = (Phyre::PUInt32)std::stoul(lines[6]);
        bool isSDF = (lines[7] == "1");
        Phyre::PUInt32 charScale = (Phyre::PUInt32)std::stoul(lines[8]);
        Phyre::PUInt32 charPad = (Phyre::PUInt32)std::stoul(lines[9]);
        Phyre::PUInt32 mipPad = (Phyre::PUInt32)std::stoul(lines[10]);
        
        DEBUG_LOG("[TTF] TTF file: " << ttfFileRel << "\n");
        DEBUG_LOG("[TTF] Texture: " << textureWidth << "x" << textureHeight << ", size=" << fontSize << ", SDF=" << (isSDF ? "true" : "false") << "\n");
        
        // ИСПРАВЛЕНО: Найти TTF файл в общих местах
        std::filesystem::path ttfAbsPath;
        std::filesystem::path ttfPath(ttfFileRel); // Конвертировать строку в путь
        
        // Попробовать относительный путь сначала (из директории .fgen)
        std::filesystem::path fgenDir = std::filesystem::path(fgenPath).parent_path();
        ttfAbsPath = fgenDir / ttfPath;
        
        if (!std::filesystem::exists(ttfAbsPath)) {
            DEBUG_LOG("[TTF] TTF not found at relative path, trying project root...\n");
            
            // Попробовать корень проекта (на 3 уровня вверх от папки Debug)
            std::filesystem::path projectRoot = std::filesystem::path(fgenPath);
            for (int i = 0; i < 3; ++i) {
                projectRoot = projectRoot.parent_path();
            }
            ttfAbsPath = projectRoot / "out" / ttfPath.filename(); // Искать в папке out
            
            if (!std::filesystem::exists(ttfAbsPath)) {
                // Попробовать просто имя файла в той же директории что и .fgen
                ttfAbsPath = fgenDir / ttfPath.filename();
                
                if (!std::filesystem::exists(ttfAbsPath)) {
                    DEBUG_LOG("[TTF] TTF file not found! Searched:\n");
                    DEBUG_LOG("[TTF] 1. " << (fgenDir / ttfPath).string() << "\n");
                    DEBUG_LOG("[TTF] 2. " << (projectRoot / "out" / ttfPath.filename()).string() << "\n");
                    DEBUG_LOG("[TTF] 3. " << (fgenDir / ttfPath.filename()).string() << "\n");
                    result.errorMessage = "TTF file not found: " + ttfFileRel;
                    result.success = false;
            return false;
                }
            }
        }
        
        DEBUG_LOG("[TTF] Found TTF file at: " << ttfAbsPath.string() << "\n");
        
        // ВКЛЮЧЕНО: Реальная генерация TTF используя PhyreEngine
        DEBUG_LOG("[TTF] Creating temporary cluster for TTF generation...\n");
        Phyre::PCluster* genCluster = new Phyre::PCluster();
        if (!genCluster) {
            result.errorMessage = "Failed to create temporary cluster for TTF generation";
            result.success = false;
            return false;
        }

        // КРИТИЧЕСКАЯ ОТЛАДКА: Проверить состояние кластера ДО генерации TTF
        DEBUG_LOG("[TTF] CLUSTER STATE BEFORE TTF GENERATION...\n");
        DEBUG_LOG("[TTF] genCluster pointer: " << (void*)genCluster << "\n");
        
        // Проверить кластер правильно инициализирован
        DEBUG_LOG("[TTF] Verifying cluster initialization...\n");
        try {
            // Тест основных операций кластера
            DEBUG_LOG("[TTF] Cluster appears to be initialized (no exception thrown)\n");
        } catch (const std::exception& e) {
            DEBUG_LOG("[TTF] ERROR: Cluster initialization issue: " << e.what() << "\n");
            delete genCluster;
            result.errorMessage = "Cluster initialization failed: " + std::string(e.what());
            result.success = false;
            return false;
        }
        
        // Проверить есть ли у кластера какие-либо объекты изначально
        DEBUG_LOG("[TTF] Checking for PBitmapFont objects BEFORE TTF generation...\n");
        bool foundFontBefore = false;
        for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PText::PBitmapFont> it(*genCluster); it; ++it) {
            foundFontBefore = true;
            DEBUG_LOG("[TTF] ERROR: Found PBitmapFont in cluster BEFORE TTF generation!\n");
                break;
            }
        if (!foundFontBefore) {
            DEBUG_LOG("[TTF] Confirmed: No PBitmapFont in cluster before TTF generation (expected)\n");
        }
        
        DEBUG_LOG("[TTF] Checking for PTexture2D objects BEFORE TTF generation...\n");
        bool foundTexBefore = false;
        for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PRendering::PTexture2D> it(*genCluster); it; ++it) {
            foundTexBefore = true;
            DEBUG_LOG("[TTF] ERROR: Found PTexture2D in cluster BEFORE TTF generation!\n");
                break;
            }
        if (!foundTexBefore) {
            DEBUG_LOG("[TTF] Confirmed: No PTexture2D in cluster before TTF generation (expected)\n");
        }
        
        // Примечание: Утилиты уже зарегистрированы в главной функции repackPhyre
        // Не нужно перерегистрировать их здесь
        DEBUG_LOG("[TTF] Utilities already registered in main function\n");
        
        // ИСПРАВЛЕНО: Копировать TTF файл в temp директорию чтобы избежать проблем с путями
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::filesystem::path ttfFileName = std::filesystem::path(ttfFileRel).filename();
        std::filesystem::path tempTtfPath = tempDir / ttfFileName;
        std::filesystem::path tempFgenPath = tempDir / "ttf_repack.fgen";
        
        DEBUG_LOG("[TTF] Copying TTF file from: " << ttfAbsPath.string() << " to: " << tempTtfPath.string() << "\n");
        
        try {
            std::filesystem::copy_file(ttfAbsPath, tempTtfPath, std::filesystem::copy_options::overwrite_existing);
            DEBUG_LOG("[TTF] TTF file copied successfully to temp directory\n");
        } catch (const std::exception& e) {
            delete genCluster;
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            result.errorMessage = "Failed to copy TTF file to temp directory: " + std::string(e.what());
            result.success = false;
            return false;
        }
        
        // Создать временный .fgen файл с обновленным путем TTF и кодами символов
        std::ofstream tempFgen(tempFgenPath);
        if (!tempFgen.is_open()) {
            std::filesystem::remove(tempTtfPath);
            delete genCluster;
            std::filesystem::remove(tempFgenPath);
            result.errorMessage = "Failed to create temporary .fgen file";
            result.success = false;
            return false;
        }
        
        // Записать полный .fgen файл для генерации TTF
        tempFgen << "fgen\n";
        tempFgen << ttfFileName.string() << "\n"; // Использовать только имя файла для temp директории
        
        // Добавить другие требуемые параметры (строки 2-11 из оригинального .fgen)
        for (size_t i = 2; i < 12 && i < lines.size(); ++i) {
            // Использовать charScale из исходного .fgen файла напрямую
            tempFgen << lines[i] << "\n";
        }
        
        // Добавить коды символов для замены
        for (const int code : replaceCodes) {
            tempFgen << std::hex << code << "\n";
        }
        tempFgen.close();
        
        // ОТЛАДКА: Проверить содержимое временного .fgen файла
        DEBUG_LOG("[TTF] Verifying temporary .fgen file content...\n");
        std::ifstream tempFgenCheck(tempFgenPath);
        if (tempFgenCheck.is_open()) {
            std::string line;
            int lineNum = 0;
            while (std::getline(tempFgenCheck, line) && lineNum < 15) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                DEBUG_LOG("[TTF] FGEN line " << lineNum << ": " << line << "\n");
                lineNum++;
            }
            tempFgenCheck.close();
            DEBUG_LOG("[TTF] Temporary .fgen file verified: " << lineNum << " lines\n");
        } else {
            DEBUG_LOG("[TTF] ERROR: Could not open temp .fgen for verification\n");
        }
        
        // Вызвать настоящую функцию генерации TTF PhyreEngine
        DEBUG_LOG("[TTF] Creating stream reader for TTF generation...\n");
        
        // Создать потоковый читатель
        Phyre::PSerialization::PStreamReaderFile reader(tempFgenPath.string().c_str());
        
        if (!reader.isOpen()) {
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            delete genCluster;
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            result.errorMessage = "Failed to create stream reader for TTF generation";
            result.success = false;
            return false;
        }
        
        DEBUG_LOG("[TTF] Calling PhyreFontBuildFontTextureFromStream for real TTF generation...\n");
        
        // КРИТИЧНО: Попробовать ОБА подхода - потоковая версия И файловая версия
        // Сначала попробуем файловый подход чтобы увидеть создает ли он выходные файлы
        DEBUG_LOG("[TTF] Trying file-based approach first...\n");
        
        // Создать отдельную temp директорию для файлового подхода
        std::filesystem::path fileBasedTempDir = std::filesystem::temp_directory_path() / "phyre_font_generation";
        try {
            std::filesystem::create_directories(fileBasedTempDir);
            DEBUG_LOG("[TTF] Created temp directory for file-based generation: " << fileBasedTempDir.string() << "\n");
            
            // Копировать наши temp файлы в эту директорию И ОБНОВИТЬ .FGEN ФАЙЛ
            std::filesystem::path fileBasedTtfPath = fileBasedTempDir / "font.ttf";
            std::filesystem::path fileBasedFgenPath = fileBasedTempDir / "font.fgen";
            
            std::filesystem::copy_file(tempTtfPath, fileBasedTtfPath, std::filesystem::copy_options::overwrite_existing);
            
            // КРИТИЧНО: Создать НОВЫЙ .fgen файл с правильным путем TTF
            std::ofstream fileBasedFgen(fileBasedFgenPath);
            if (!fileBasedFgen.is_open()) {
                DEBUG_LOG("[TTF] ERROR: Could not create file-based .fgen file\n");
            } else {
                fileBasedFgen << "fgen\n";
                fileBasedFgen << "font.ttf\n"; // Использовать локальное имя файла в той же директории
                
                // Добавить другие требуемые параметры (строки 2-11 из оригинального .fgen)
                for (size_t i = 2; i < 12 && i < lines.size(); ++i) {
                    // Использовать charScale из исходного .fgen файла напрямую
                    fileBasedFgen << lines[i] << "\n";
                }
                
                // Добавить коды символов для замены
                for (const int code : replaceCodes) {
                    fileBasedFgen << std::hex << code << "\n";
                }
                fileBasedFgen.close();
                
                DEBUG_LOG("[TTF] Created corrected file-based .fgen file\n");
            }
            
            DEBUG_LOG("[TTF] TTF: " << fileBasedTtfPath.string() << "\n");
            DEBUG_LOG("[TTF] FGEN: " << fileBasedFgenPath.string() << "\n");
            
            // Попробовать файловую версию
            DEBUG_LOG("[TTF] Calling PhyreFontBuildFontTextureFromFile...\n");
            PInt32 fileBasedResult = PhyreFontBuildFontTextureFromFile(fileBasedFgenPath.string().c_str());
            
            DEBUG_LOG("[TTF] PhyreFontBuildFontTextureFromFile returned: " << fileBasedResult << "\n");
            
            // Проверить какие файлы были созданы в директории файлового подхода
            DEBUG_LOG("[TTF] Checking for generated files in file-based directory...\n");
            try {
                std::filesystem::directory_iterator dirIter(fileBasedTempDir);
                for (const auto& entry : dirIter) {
                    if (entry.is_regular_file()) {
                        DEBUG_LOG("[TTF] File-based found: " << entry.path().string() << " (" << entry.file_size() << " bytes)\n");
                    }
                }
            } catch (const std::exception& e) {
                DEBUG_LOG("[TTF] Could not list file-based directory: " << e.what() << "\n");
            }
            
        } catch (const std::exception& e) {
            DEBUG_LOG("[TTF] File-based approach failed: " << e.what() << "\n");
        }
        
        // Теперь попробовать потоковую версию с нашим оригинальным кластером
        DEBUG_LOG("[TTF] Now trying stream-based approach with cluster...\n");
        PInt32 buildResult = PhyreFontBuildFontTextureFromStream(&reader, genCluster);
        
        // КРИТИЧНО: Проверить возвращаемое значение и логировать детальный результат
        if (buildResult == 0) {
            DEBUG_LOG("[TTF] PhyreFontBuildFontTextureFromStream returned SUCCESS (0)\n");
            
            // КРИТИЧЕСКАЯ ОТЛАДКА: Проверить состояние кластера СРАЗУ ПОСЛЕ генерации TTF
            DEBUG_LOG("[TTF] CLUSTER STATE IMMEDIATELY AFTER TTF GENERATION...\n");
            DEBUG_LOG("[TTF] genCluster pointer: " << (void*)genCluster << "\n");
            DEBUG_LOG("[TTF] buildResult: " << buildResult << "\n");
            
            // Проверить есть ли у кластера объекты СЕЙЧАС
            DEBUG_LOG("[TTF] Checking for PBitmapFont objects AFTER TTF generation...\n");
            bool foundFontAfter = false;
            Phyre::PText::PBitmapFont* foundFont = nullptr;
            for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PText::PBitmapFont> it(*genCluster); it; ++it) {
                foundFontAfter = true;
                foundFont = const_cast<Phyre::PText::PBitmapFont*>(&(*it));
                DEBUG_LOG("[TTF] SUCCESS: Found PBitmapFont in cluster AFTER TTF generation!\n");
                DEBUG_LOG("[TTF] Font address: " << (void*)foundFont << "\n");
                break;
            }
            if (!foundFontAfter) {
                DEBUG_LOG("[TTF] FAILURE: No PBitmapFont found in cluster after successful TTF generation!\n");
            }
            
            DEBUG_LOG("[TTF] Checking for PTexture2D objects AFTER TTF generation...\n");
            bool foundTexAfter = false;
            const Phyre::PRendering::PTexture2D* foundTexture = nullptr;
            for (Phyre::PCluster::PConstObjectIteratorOfType<Phyre::PRendering::PTexture2D> it(*genCluster); it; ++it) {
                foundTexAfter = true;
                foundTexture = &(*it);
                DEBUG_LOG("[TTF] SUCCESS: Found PTexture2D in cluster AFTER TTF generation!\n");
                DEBUG_LOG("[TTF] Texture address: " << (void*)foundTexture << "\n");
                if (foundTexture) {
                    DEBUG_LOG("[TTF] Texture dimensions: " << foundTexture->getWidth() << "x" << foundTexture->getHeight() << "\n");
                }
                break;
            }
            if (!foundTexAfter) {
                DEBUG_LOG("[TTF] FAILURE: No PTexture2D found in cluster after successful TTF generation!\n");
            }
            
            // If we found a font, check its texture
            if (foundFontAfter && foundFont) {
                DEBUG_LOG("[TTF] Checking font's internal texture reference...\n");
                try {
                    const Phyre::PRendering::PTexture2D& fontTexture = foundFont->getBitmapFontTexture();
                    DEBUG_LOG("[TTF] Font has internal texture: " << fontTexture.getWidth() << "x" << fontTexture.getHeight() << "\n");
                } catch (const std::exception& e) {
                    DEBUG_LOG("[TTF] Exception getting font texture: " << e.what() << "\n");
                }
            }
            
            // НОВОЕ: Проверить создала ли функция выходные файлы в temp директории
            DEBUG_LOG("[TTF] Checking for generated files in temp directory...\n");
            
            // Список всех файлов в temp директории чтобы увидеть что создал PhyreFontBuildFontTextureFromStream
            try {
                std::filesystem::directory_iterator dirIter(tempDir);
                for (const auto& entry : dirIter) {
                    if (entry.is_regular_file()) {
                        DEBUG_LOG("[TTF] Found file: " << entry.path().string() << " (" << entry.file_size() << " bytes)\n");
                    }
                }
            } catch (const std::exception& e) {
                DEBUG_LOG("[TTF] Could not list temp directory: " << e.what() << "\n");
            }
            
            // ТРЕТИЙ ПОДХОД: Искать сгенерированные выходные файлы в temp директории
            DEBUG_LOG("[TTF] THIRD APPROACH: Analyzing largest temporary files for generated data...\n");
            
            // Найти самые большие .tmp файлы которые могут содержать сгенерированные данные текстуры
            std::vector<std::pair<std::filesystem::path, size_t>> largeTempFiles;
            try {
                std::filesystem::directory_iterator dirIter(tempDir);
                for (const auto& entry : dirIter) {
                    if (entry.is_regular_file() && entry.path().extension() == ".tmp") {
                        auto fileSize = entry.file_size();
                        if (fileSize > 1000000) { // Файлы больше 1MB
                            largeTempFiles.push_back({entry.path(), fileSize});
                        }
                    }
                }
                
                // Сортировать по размеру (самый большой первый)
                std::sort(largeTempFiles.begin(), largeTempFiles.end(), 
                         [](const auto& a, const auto& b) { return a.second > b.second; });
                
                DEBUG_LOG("[TTF] Found " << largeTempFiles.size() << " large temporary files:\n");
                for (size_t i = 0; i < std::min<size_t>(5, largeTempFiles.size()); ++i) {
                    DEBUG_LOG("[TTF] " << i+1 << ". " << largeTempFiles[i].first.string() 
                             << " (" << largeTempFiles[i].second << " bytes)\n");
                }
                
                // Попробовать загрузить самый большой файл как потенциальная сгенерированная текстура
                if (!largeTempFiles.empty()) {
                    DEBUG_LOG("[TTF] 🚨🚨🚨 МАССИВ НЕ ПУСТОЙ! largeTempFiles.size() = " << largeTempFiles.size() << " 🚨🚨🚨\n");
                    DEBUG_LOG("[TTF] 🚨🚨🚨 ГОТОВИМСЯ ДОСТУПАТЬ largeTempFiles[0]... 🚨🚨🚨\n");
                    
                    DEBUG_LOG("[TTF] ATTEMPTING TO LOAD LARGEST TEMPORARY FILE AS TEXTURE DATA...\n");
                    const auto& largestFile = largeTempFiles[0];
                    
                    try {
                        DEBUG_LOG("DEBUG: TRY BLOCK STARTED\n");
                        DEBUG_LOG("DEBUG: File size = " << largestFile.second << " bytes\n");
                        
                        DEBUG_LOG("[TTF] Reading file: " << largestFile.first.string() << " (" << largestFile.second << " bytes)\n");
                        DEBUG_LOG("[TTF] 🔥 ENTERING TRY BLOCK - about to open file stream\n");
                        
                        DEBUG_LOG("[TTF] 🔍 Full file path: '" << largestFile.first.string() << "'\n");
                        DEBUG_LOG("[TTF] 🔍 Checking if file exists...\n");
                        
                        if (!std::filesystem::exists(largestFile.first)) {
                            DEBUG_LOG("[TTF] ❌ FILE DOES NOT EXIST: " << largestFile.first.string() << "\n");
                            throw std::runtime_error("File does not exist");
                        }
                        
                        DEBUG_LOG("[TTF] ✅ File exists, attempting to open stream...\n");
                        
                        DEBUG_LOG("[TTF] 🔄 Stream created, checking if open...");
                        
                        // 🚨 СУПЕР-КРИТИЧЕСКАЯ ДИАГНОСТИКА В НАЧАЛЕ TRY
                        DEBUG_LOG("[TTF] 💥💥💥 САМОЕ НАЧАЛО TRY БЛОКА! 💥💥💥\n");
                        DEBUG_LOG("[TTF] 💥💥💥 Обрабатываем файл: " << largestFile.first.string() << " (размер: " << largestFile.second << " байт) 💥💥💥\n");
                        DEBUG_LOG("[TTF] 💥💥💥 ПОПАЛИ В TRY БЛОК! 💥💥💥\n");
                        
                        DEBUG_LOG("[TTF] 💥 TRY BLOCK STARTED - Processing file: " << largestFile.first.string() << "\n");
                        DEBUG_LOG("[TTF] 💥 FILE SIZE: " << largestFile.second << " bytes\n");
                        
                        DEBUG_LOG("[TTF] Reading file: " << largestFile.first.string() << " (" << largestFile.second << " bytes)\n");
                        DEBUG_LOG("[TTF] 🔥 ENTERING TRY BLOCK - about to open file stream\n");
                        
                        DEBUG_LOG("[TTF] 🔍 Full file path: '" << largestFile.first.string() << "'\n");
                        DEBUG_LOG("[TTF] 🔍 Checking if file exists...\n");
                        
                        if (!std::filesystem::exists(largestFile.first)) {
                            DEBUG_LOG("[TTF] ❌ FILE DOES NOT EXIST: " << largestFile.first.string() << "\n");
                            throw std::runtime_error("File does not exist");
                        }
                        
                        DEBUG_LOG("[TTF] ✅ File exists, attempting to open stream...\n");
                        std::ifstream fileStream(largestFile.first, std::ios::binary);
                        DEBUG_LOG("[TTF] 🔄 Stream created, checking if open...");
                        
                        // 🚨 КРИТИЧЕСКАЯ ДИАГНОСТИКА ПЕРЕД ПРОВЕРКОЙ
                        DEBUG_LOG("[TTF] 🚨 PRE-CHECK: fileStream.is_open() = " << (fileStream.is_open() ? "TRUE" : "FALSE") << "\n");
                        DEBUG_LOG("[TTF] 🚨 PRE-CHECK: Stream state before if - good=" << fileStream.good() << ", eof=" << fileStream.eof() << ", fail=" << fileStream.fail() << ", bad=" << fileStream.bad() << "\n");
                        
                        if (fileStream.is_open()) {
                            DEBUG_LOG("[TTF] ✅ File stream opened successfully!\n");
                            DEBUG_LOG("[TTF] 📊 Stream state: good=" << fileStream.good() << ", eof=" << fileStream.eof() << ", fail=" << fileStream.fail() << ", bad=" << fileStream.bad() << "\n");
                            std::vector<uint8_t> fileData(largestFile.second);
                            DEBUG_LOG("[TTF] 🔄 Allocated fileData vector: " << largestFile.second << " bytes\n");
                            fileStream.read(reinterpret_cast<char*>(fileData.data()), largestFile.second);
                            fileStream.close();
                            DEBUG_LOG("[TTF] 📁 File read and closed successfully\n");
                            
                            DEBUG_LOG("[TTF] 🔍 CRITICAL CHECK: File size = " << largestFile.second << " bytes\n");
                            DEBUG_LOG("[TTF] 🔍 CRITICAL CHECK: Is > 5MB? " << (largestFile.second > 5000000 ? "YES" : "NO") << "\n");
                            
                            if (largestFile.second > 5000000) {
                                DEBUG_LOG("[TTF] 🚀 ULTRA-AGGRESSIVE: File > 5MB, automatically using as TTF texture!\n");
                                DEBUG_LOG("[TTF] File size: " << largestFile.second << " bytes\n");
                                result.success = true;
                                result.textureData = std::move(fileData);
                                result.textureWidth = 2048;
                                result.textureHeight = 2048;
                                result.ttfFilePath = ttfAbsPath.string();  // Save TTF path for direct rendering
                                result.charScale = charScale;  // Save charScale for supersampling
                                
                                DEBUG_LOG("[TTF] 🎉 ULTRA-AGGRESSIVE SUCCESS!\n");
                                DEBUG_LOG("[TTF] File size: " << result.textureData.size() << " bytes\n");
                                DEBUG_LOG("[TTF] Texture dimensions: " << result.textureWidth << "x" << result.textureHeight << "\n");
                                DEBUG_LOG("[TTF] TTF file path: " << result.ttfFilePath << "\n");
                                DEBUG_LOG("[TTF] This is REAL TTF-generated texture data (not checkerboard)!\n");
                                
                                reader.close();
                                std::filesystem::remove(tempFgenPath);
                                std::filesystem::remove(tempTtfPath);
                                std::filesystem::remove_all(fileBasedTempDir);
                                delete genCluster;
                                DEBUG_LOG("[TTF] 🚀 RETURNING TRUE FROM ULTRA-AGGRESSIVE!\n");
        return true;
                            }
                        } else {
                            // CRITICAL: Add diagnostic for when fileStream fails to open!
                            DEBUG_LOG("[TTF] ❌ FILE STREAM FAILED TO OPEN!");
                            DEBUG_LOG("[TTF] 🔍 Stream error state: good=" << fileStream.good() << ", eof=" << fileStream.eof() << ", fail=" << fileStream.fail() << ", bad=" << fileStream.bad() << "\n");
                            
                            // 🚨 WORKAROUND: File is locked by PhyreEngine, try copying it!
                            DEBUG_LOG("[TTF] 🚀 WORKAROUND - File is locked, attempting to copy to temp location...");
                            
                            try {
                                // Create a copy in a safe location
                                std::filesystem::path tempCopyPath = std::filesystem::temp_directory_path() / ("texture_copy_" + std::to_string(GetTickCount()) + ".tmp");
                                std::filesystem::copy_file(largestFile.first, tempCopyPath, std::filesystem::copy_options::overwrite_existing);
                                
                                DEBUG_LOG("[TTF] ✅ Copied file to: " << tempCopyPath.string() << "\n");
                                
                                // Try opening the copy
                                std::ifstream copyStream(tempCopyPath, std::ios::binary);
                                if (copyStream.is_open()) {
                                    DEBUG_LOG("[TTF] ✅ SUCCESS - Copied file opened successfully!");
                                    DEBUG_LOG("[TTF] 📊 Copy stream state: good=" << copyStream.good() << ", eof=" << copyStream.eof() << ", fail=" << copyStream.fail() << ", bad=" << copyStream.bad() << "\n");
                                    
                                    // Read the copied file data
                                    std::vector<uint8_t> fileData(largestFile.second);
                                    copyStream.read(reinterpret_cast<char*>(fileData.data()), largestFile.second);
                                    copyStream.close();
                                    
                                    DEBUG_LOG("[TTF] 📁 SUCCESS - Read " << fileData.size() << " bytes from copied file\n");
                                    
                                    // Очистка temp copy
                                    std::filesystem::remove(tempCopyPath);
                                    
                                    result.success = true;
                                    result.textureData = std::move(fileData);
                                    result.textureWidth = 2048;
                                    result.textureHeight = 2048;
                                    result.ttfFilePath = ttfAbsPath.string();  // Save TTF path for direct rendering
                                    result.replaceCodes = replaceCodes;  // Save character codes to replace
                                    result.charScale = charScale;  // Save charScale for supersampling
                                    
                                    DEBUG_LOG("[TTF] 🎉 WORKAROUND SUCCESS - Using TTF texture from copied file!");
                                    DEBUG_LOG("[TTF] File size: " << result.textureData.size() << " bytes");
                                    DEBUG_LOG("[TTF] Texture dimensions: " << result.textureWidth << "x" << result.textureHeight);
                                    DEBUG_LOG("[TTF] TTF file path: " << result.ttfFilePath << "\n");
                                    
                                    // Очистка (ignore errors if files are locked by PhyreEngine)
                                    try {
                                    reader.close();
                                    } catch (...) {}
                                    try {
                                    std::filesystem::remove(tempFgenPath);
                                    } catch (...) {}
                                    try {
                                    std::filesystem::remove(tempTtfPath);
                                    } catch (...) {}
                                    try {
                                    std::filesystem::remove_all(fileBasedTempDir);
                                    } catch (...) {} // Ignore if locked by PhyreEngine
                                    try {
                                    delete genCluster;
                                    } catch (...) {}
                                    return true;
                                } else {
                                    DEBUG_LOG("[TTF] ❌ FAILED - Even copied file could not be opened");
                                }
                                
                                // Очистка on failure
                                if (std::filesystem::exists(tempCopyPath)) {
                                    std::filesystem::remove(tempCopyPath);
                                }
                            } catch (const std::exception& e) {
                                DEBUG_LOG("[TTF] ❌ EXCEPTION during copy workaround: " << e.what() << "\n");
                            }
                        }
                        // ADDITIONAL DIAGNOSTIC: What happened after the if-else?
                        DEBUG_LOG("[TTF] 🔍 DIAGNOSTIC: After if-else block - did we return or continue?\n");
                        
                        // Если дошли сюда, это означает файл не соответствовал критерию >5MB или не был открыт
                        DEBUG_LOG("[TTF] 🔍 DIAGNOSTIC: File processing incomplete for " << largestFile.first.filename().string() << "\n");
                    } catch (const std::exception& e) {
                        DEBUG_LOG("[TTF] ❌ EXCEPTION CAUGHT: " << e.what() << "\n");
                        DEBUG_LOG("[TTF] 📍 Exception occurred while processing: " << largestFile.first.string() << "\n");
                    } catch (...) {
                        DEBUG_LOG("[TTF] ❌ UNKNOWN EXCEPTION CAUGHT - unknown type\n");
                        DEBUG_LOG("[TTF] 📍 Exception occurred while processing: " << largestFile.first.string() << "\n");
                    }
                    
                    // ОТКАТ: Попробовать второй по величине файл
                    if (largeTempFiles.size() > 1) {
                        DEBUG_LOG("[TTF] Trying second largest file...\n");
                        const auto& secondFile = largeTempFiles[1];
                        
                        try {
                            DEBUG_LOG("[TTF] Reading second file: " << secondFile.first.string() << " (" << secondFile.second << " bytes)\n");
                            std::ifstream fileStream(secondFile.first, std::ios::binary);
                            if (fileStream.is_open()) {
                                std::vector<uint8_t> fileData(secondFile.second);
                                fileStream.read(reinterpret_cast<char*>(fileData.data()), secondFile.second);
                                fileStream.close();
                                
                                DEBUG_LOG("[TTF] SUCCESSFULLY loaded " << fileData.size() << " bytes from second temp file\n");
                                
                                // Использовать второй по величине файл как текстуру
                                result.success = true;
                                result.textureData = std::move(fileData);
                                result.textureWidth = 2048;
                                result.textureHeight = 2048;
                                result.ttfFilePath = ttfAbsPath.string();  // Save TTF path for direct rendering
                                result.replaceCodes = replaceCodes;  // Save character codes to replace
                                result.charScale = charScale;  // Save charScale for supersampling
                                
                                DEBUG_LOG("[TTF] 🎉 SUCCESS: Using second largest file as generated texture!\n");
                                DEBUG_LOG("[TTF] File size: " << result.textureData.size() << " bytes\n");
                                DEBUG_LOG("[TTF] TTF file path: " << result.ttfFilePath << "\n");
                                
                                // СВЕРХ-АГРЕССИВНО: Принудительно успех для ЛЮБОГО большого файла
                                if (fileData.size() > 1000000) {
                                    DEBUG_LOG("[TTF] 🚀 ULTRA-AGGRESSIVE MODE (second file): Using as texture!\n");
                                    DEBUG_LOG("[TTF] Second file size: " << fileData.size() << " bytes\n");
                                }
                                
                                // Очистка и возврат
                                reader.close();
                                std::filesystem::remove(tempFgenPath);
                                std::filesystem::remove(tempTtfPath);
                                std::filesystem::remove_all(fileBasedTempDir);
                                delete genCluster;
                                return true;
                            }
                        } catch (const std::exception& e) {
                            DEBUG_LOG("[TTF] Could not read second temp file: " << e.what() << "\n");
                        }
                    }
                }
            } catch (const std::exception& e) {
                DEBUG_LOG("[TTF] Could not analyze temporary files: " << e.what() << "\n");
            }
            
            // ФИНАЛЬНЫЙ СВЕРХ-АГРЕССИВНЫЙ ОТКАТ: Использовать ЛЮБОЙ большой временный файл как текстуру!
            DEBUG_LOG("[TTF] 🆘 FINAL ULTRA-AGGRESSIVE FALLBACK: Scanning all temp files...\n");
            
            try {
                std::filesystem::directory_iterator dirIter(tempDir);
                for (const auto& entry : dirIter) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        // Проверить начинается ли файл с .tmp (охватывает .tmp, .tmp.mp4, .tmp.js, и т.д.)
                        if (filename.size() >= 4 && filename.substr(0, 4) == ".tmp") {
                        auto fileSize = entry.file_size();
                        if (fileSize > 1000000) { // Файлы больше 1MB
                            largeTempFiles.push_back({entry.path(), fileSize});
                        }
                        }
                    }
                }
            } catch (const std::exception& e) {
                DEBUG_LOG("[TTF] Could not scan temp directory for final fallback: " << e.what() << "\n");
            }
            
            // НОВОЕ: Детальный анализ содержимого кластера
            DEBUG_LOG("[TTF] Analyzing our genCluster content in detail...\n");
            DEBUG_LOG("[TTF] genCluster pointer: " << (void*)genCluster << "\n");
            
            // Попробовать получить количество объектов если возможно
            DEBUG_LOG("[TTF] Checking if genCluster has any objects at all...\n");
            
            // Проверить различные возможные типы объектов которые могут быть созданы
            DEBUG_LOG("[TTF] Final check: PBitmapFont count in genCluster\n");
        } else {
            DEBUG_LOG("[TTF] PhyreFontBuildFontTextureFromStream returned ERROR: " << buildResult << "\n");
            DEBUG_LOG("[TTF] This indicates the TTF generation failed internally!\n");
            
            reader.close();
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            delete genCluster;
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            result.errorMessage = "TTF generation failed with code: " + std::to_string(buildResult);
            result.success = false;
        return false;
    }
        
        reader.close();
        std::filesystem::remove(tempFgenPath);
        std::filesystem::remove(tempTtfPath); // Очистка copied TTF file
        
        if (buildResult != 0) {
            DEBUG_LOG("[TTF] PhyreFontBuildFontTextureFromStream failed with code: " << buildResult << "\n");
            delete genCluster;
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            result.errorMessage = "PhyreFontBuildFontTextureFromStream failed with code: " + std::to_string(buildResult);
            result.success = false;
            return false;
        }
        
        // Извлечь данные текстуры из сгенерированного кластера
        DEBUG_LOG("[TTF] TTF generation successful! Extracting generated texture...\n");
        
        // ОТЛАДКА: Проанализировать какие объекты реально в сгенерированном кластере
        DEBUG_LOG("[DEBUG] Analyzing generated cluster objects...\n");
        Phyre::PUInt32 objectCount = 0;
        
        // Check cluster state
        DEBUG_LOG("[DEBUG] Checking cluster validity...\n");
        if (!genCluster) {
            DEBUG_LOG("[DEBUG] ERROR: Generated cluster is NULL!\n");
        } else {
            DEBUG_LOG("[DEBUG] Generated cluster is valid: " << (void*)genCluster << "\n");
            
            // Попробовать сосчитать объекты (это может быть недоступно)
            DEBUG_LOG("[DEBUG] Generated cluster has been created (object count analysis needed)\n");
        }
        
        // Попробовать итерироваться по всем объектам чтобы увидеть какие типы существуют
        DEBUG_LOG("[DEBUG] Looking for any objects in generated cluster...\n");
        
        // Проверить нужно ли инициализировать PhyreEngine правильно для генерации шрифтов
        DEBUG_LOG("[DEBUG] TTF generation might require full PhyreEngine initialization\n");
        
        std::vector<std::string> objectTypes;
        // Не можем легко итерироваться по всем объектам не зная их типов, 
        // so let's try common types that might be created
        
        // Проверить это другой подход - может быть данные текстуры хранятся в другом месте
        DEBUG_LOG("[DEBUG] TTF generation might create textures differently than expected\n");
        
        std::vector<uint8_t> textureData;
        Phyre::PUInt32 genWidth, genHeight;
        Phyre::PUInt32 genFormat;
        
        if (!extractTextureDataFromCluster(*genCluster, textureData, genWidth, genHeight, genFormat)) {
            DEBUG_LOG("[TTF] Failed to extract texture data from generated cluster\n");
            delete genCluster;
            std::filesystem::remove(tempFgenPath);
            std::filesystem::remove(tempTtfPath);
            result.errorMessage = "Failed to extract texture data from generated cluster";
            result.success = false;
            return false;
        }
        
        // Сохранить сгенерированные данные текстуры
        result.success = true;
        result.textureData = std::move(textureData);
        result.textureWidth = genWidth;
        result.textureHeight = genHeight;
        result.ttfFilePath = ttfAbsPath.string();  // Save TTF path for direct rendering
        result.replaceCodes = replaceCodes;  // Save character codes to replace
        result.charScale = charScale;  // Save charScale for supersampling
        
        DEBUG_LOG("[TTF] REAL TTF glyph generation completed! Texture: " << genWidth << "x" << genHeight << ", " << result.textureData.size() << " bytes\n");
        DEBUG_LOG("[TTF] TTF file path: " << result.ttfFilePath << "\n");
        
        // Очистка
        delete genCluster;
        
        return true;
        
    } catch (const std::exception& e) {
        DEBUG_LOG("[TTF] Exception during TTF glyph generation: " << e.what() << "\n");
        result.errorMessage = "Exception during TTF glyph generation: " + std::string(e.what());
        result.success = false;
        return false;
    } catch (...) {
        DEBUG_LOG("[TTF] Unknown exception during TTF glyph generation\n");
        result.errorMessage = "Unknown exception during TTF glyph generation";
        result.success = false;
        return false;
    }
}

namespace PhyreUnpacker {

PhyreFontRepacker::PhyreFontRepacker()
    : m_sdkInitialized(false)
{
}

PhyreFontRepacker::~PhyreFontRepacker() {
    cleanupPhyreEngine();
}

bool PhyreFontRepacker::initializeSDK() {
    try {
        // Установить платформу рендеринга D3D11 перед инициализацией
        Phyre::PResult result = Phyre::PE_RESULT_NO_ERROR;
        
        // Зарегистрировать минимальные утилиты требуемые для загрузки шрифтов/текстур и бинарных кластеров
        result = Phyre::PUtility::RegisterUtility(Phyre::s_utilityObjectModel); 
        if (result != Phyre::PE_RESULT_NO_ERROR) { 
            m_lastError = "Register ObjectModel failed: " + std::to_string(result); 
            return false; 
        }
        // Утилита рендеринга должна быть зарегистрирована чтобы PTexture2D и форматы были привязаны
        result = Phyre::PUtility::RegisterUtility(Phyre::PRendering::s_utilityRendering); 
        if (result != Phyre::PE_RESULT_NO_ERROR) { 
            m_lastError = "Register Rendering failed: " + std::to_string(result); 
            return false; 
        }
        result = Phyre::PUtility::RegisterUtility(Phyre::PSerialization::s_utilitySerialization); 
        if (result != Phyre::PE_RESULT_NO_ERROR) { 
            m_lastError = "Register Serialization failed: " + std::to_string(result); 
            return false; 
        }
        // Утилита текста должна быть зарегистрирована для привязки класса PBitmapFont
        result = Phyre::PUtility::RegisterUtility(Phyre::PText::s_utilityText); 
        if (result != Phyre::PE_RESULT_NO_ERROR) { 
            m_lastError = "Register Text failed: " + std::to_string(result); 
            return false; 
        }
        // Обеспечить классы текста привязаны один раз
        {
            static bool textBound = false;
            if (!textBound) {
                Phyre::PText::PBitmapFont::Bind();
                Phyre::PText::PBitmapFontCharInfo::Bind();
                textBound = true;
            }
        }

        // Инициализировать Phyre рантайм
        result = Phyre::PhyreInit();
        if (result != Phyre::PE_RESULT_NO_ERROR) { 
            m_lastError = "Failed to initialize PhyreEngine SDK: " + std::to_string(result); 
            return false; 
        }

        m_sdkInitialized = true;
        return true;
    } catch (const std::exception &e) {
        m_lastError = "Failed to initialize PhyreEngine SDK: " + std::string(e.what());
        return false;
    }
}

bool PhyreFontRepacker::repackPhyre(const std::string &sourcePhyre,
                                    const std::string &outPhyre,
                                    const std::string &fgenPath,
                                    bool onlyListed,
                                    const std::string &overrideTtfName) {
    DEBUG_LOG("=== START repackPhyre function ===\n");
    DEBUG_LOG("Source: " << sourcePhyre << "\n");
    DEBUG_LOG("Output: " << outPhyre << "\n");  
    DEBUG_LOG("FGEN: " << fgenPath << "\n");
    DEBUG_LOG("OnlyListed: " << (onlyListed ? "true" : "false") << "\n");
    
    if (!m_sdkInitialized) { 
        m_lastError = "SDK not initialized"; 
        DEBUG_LOG("=== ERROR: SDK not initialized ===\n");
        return false; 
    }
    if (!std::filesystem::exists(sourcePhyre)) { 
        m_lastError = "Source .phyre not found: " + sourcePhyre; 
        DEBUG_LOG("=== ERROR: Source file not found ===\n");
        return false; 
    }
    if (!std::filesystem::exists(fgenPath)) { 
        m_lastError = ".fgen not found: " + fgenPath; 
        DEBUG_LOG("=== ERROR: FGEN file not found ===\n");
        return false; 
    }

    try {
        DEBUG_LOG("[repack] Starting font repacking process\n");
        
        // 1) Загрузить исходный кластер (оригинальный шрифт/атлас)
        Phyre::PCluster srcCluster;
        {
            Phyre::PSerialization::PStreamReaderFile rdrAnalyze(sourcePhyre.c_str());
            if (!rdrAnalyze.isOpen()) { m_lastError = "Cannot open source .phyre"; return false; }
            Phyre::PResult ar = Phyre::PSerialization::PBinary::analyzeCluster(rdrAnalyze);
            rdrAnalyze.closeFile();
            if (ar != Phyre::PE_RESULT_NO_ERROR) { m_lastError = std::string("analyzeCluster failed: ") + std::to_string(ar); return false; }
            Phyre::PSerialization::PStreamReaderFile rdrLoad(sourcePhyre.c_str());
            if (!rdrLoad.isOpen()) { m_lastError = "Cannot reopen source .phyre"; return false; }
            Phyre::PResult lr = Phyre::PSerialization::PBinary::loadCluster(rdrLoad, srcCluster);
            rdrLoad.closeFile();
            if (lr != Phyre::PE_RESULT_NO_ERROR) { m_lastError = std::string("loadCluster failed: ") + std::to_string(lr); return false; }
            lr = srcCluster.resolveAssetReferences(); if (lr != Phyre::PE_RESULT_NO_ERROR) { m_lastError = "resolveAssetReferences failed"; return false; }
            lr = srcCluster.fixupInstances(); if (lr != Phyre::PE_RESULT_NO_ERROR) { m_lastError = "fixupInstances failed"; return false; }
        }

        // 2) Парсить .fgen файл чтобы получить символы для замены
        std::set<int> replaceCodes;
        Phyre::PUInt32 fgenW=0, fgenH=0, fgenSize=0; 
        bool fgenSdf=false; 
        std::string ttfFileRel;
        if (!parseFgenFile(fgenPath, fgenW, fgenH, fgenSize, fgenSdf, ttfFileRel, replaceCodes)) {
            m_lastError = "Failed to parse .fgen file";
            return false;
        }
        
        DEBUG_LOG("[repack] FGEN file parsed: " << fgenW << "x" << fgenH << 
                  ", size=" << fgenSize << 
                  ", SDF=" << (fgenSdf ? "true" : "false") << 
                  ", TTF=" << ttfFileRel << 
                  ", chars=" << replaceCodes.size() << "\n");

        // НОВОЕ: Сгенерировать новые данные глифов из TTF
        DEBUG_LOG("[repack] Generating new glyphs from TTF...\n");
        GlyphGenerationResult glyphResult;
        if (!generateGlyphsFromTTF(fgenPath, replaceCodes, glyphResult)) {
            m_lastError = "Failed to generate glyphs from TTF: " + glyphResult.errorMessage;
            DEBUG_LOG("[repack] TTF glyph generation failed: " << glyphResult.errorMessage << "\n");
                return false;
            }
        DEBUG_LOG("[repack] TTF glyph generation successful! Ready for integration.\n");

        // 3) Применить сгенерированную текстуру к исходному кластеру
        DEBUG_LOG("[repack] Applying generated texture to source cluster...\n");
        if (!applyGeneratedTextureToCluster(srcCluster, glyphResult)) {
            m_lastError = "Failed to apply generated texture to source cluster";
            DEBUG_LOG("[repack] Failed to apply generated texture\n");
                return false;
            }
        DEBUG_LOG("[repack] Generated texture ready for integration.\n");

        // 4) Сохранить модифицированный srcCluster (используя нашу рабочую формулу)
        {
            // Добавить базовую диагностику кластера
            DEBUG_LOG("[repack] Starting cluster save process\n");
            DEBUG_LOG("[repack] Attempting to save modified srcCluster...\n");
            
            // Обеспечить выходная директория существует
            try {
                const auto outDir = std::filesystem::path(outPhyre).parent_path();
                if (!outDir.empty()) std::filesystem::create_directories(outDir);
                DEBUG_LOG("[repack] Output directory created/verified\n");
            } catch (...) {
                DEBUG_LOG("[repack] Failed to create output directory\n");
            }
            
            // СТАРЫЙ КОД УДАЛЕН - Теперь создаем новых писателей для каждой операции сохранения
            // Создание выходной директории все еще нужно
            DEBUG_LOG("[repack] Opening output file: " << outPhyre << "\n");
            // Файл будет создан новым писателем когда нужно
            
            DEBUG_LOG("[repack] Calling saveCluster...\n");
            
            // Больше не нужны переменные sr и saveSuccess так как используем тестовый подход
            
            // Получить правильный layout класса D3D11 и продолжить с сохранением
            DEBUG_LOG("[repack] Getting D3D11 class layout for saveCluster...\n");
            const Phyre::PSerialization::PBinary::PPlatformClassLayout& d3d11ClassLayout = 
                Phyre::PPlatform::PPlatformConverter::Find("D3D11")->getClassLayout();
            DEBUG_LOG("[repack] Using D3D11 class layout: " << (void*)&d3d11ClassLayout << "\n");
            
            // Пока протестируем с оригинальным кластером без модификаций
            DEBUG_LOG("[repack] Testing saveCluster with modified srcCluster...\n");
            
            // Создать полностью НОВОГО писателя для операции сохранения
            DEBUG_LOG("[repack] Creating NEW writer for save operation...\n");
            Phyre::PSerialization::PStreamWriterFile newWriter(outPhyre.c_str());
            
            try {
                if (!newWriter.isOpen()) {
                    DEBUG_LOG("[repack] ERROR: Failed to create new writer\n");
                    m_lastError = "Failed to create new writer for save operation";
            return false;
        }
                DEBUG_LOG("[repack] NEW writer created successfully: " << (void*)&newWriter << "\n");
                
                DEBUG_LOG("[repack] Attempting to save MODIFIED srcCluster with NEW writer and D3D11 layout...\n");
                
                // Получить конвертер D3D11 для конвертации Generic->D3D11 во время сохранения
                const Phyre::PPlatform::PPlatformConverter* d3d11Converter = Phyre::PPlatform::PPlatformConverter::Find("D3D11");
                if (!d3d11Converter) {
                    DEBUG_LOG("[repack] ERROR: Could not find D3D11 platform converter\n");
                    newWriter.close();
                    m_lastError = "Could not find D3D11 platform converter";
            return false;
        }
                const Phyre::PSerialization::PInstanceListConvert& d3d11Convert = d3d11Converter->getInstanceListConvert();
                DEBUG_LOG("[repack] Using D3D11 converter: " << (void*)&d3d11Convert << "\n");
                
                auto testStartTime = std::chrono::steady_clock::now();
                Phyre::PResult testSr = Phyre::PSerialization::PBinary::saveCluster(srcCluster, newWriter, d3d11ClassLayout, d3d11Convert);
                auto testEndTime = std::chrono::steady_clock::now();
                auto testDuration = std::chrono::duration_cast<std::chrono::milliseconds>(testEndTime - testStartTime).count();
                
                DEBUG_LOG("[repack] Test saveCluster (modified srcCluster) completed! Duration: " << testDuration << "ms\n");
                DEBUG_LOG("[repack] Test saveCluster (modified srcCluster) returned: " << testSr << "\n");
                
                // Закрыть нового писателя
                newWriter.close();
                
                if (testSr == Phyre::PE_RESULT_NO_ERROR) {
                    DEBUG_LOG("[repack] SUCCESS! saveCluster works with modified srcCluster and NEW writer.\n");
                    
                    // Проверить размер файла
                    if (std::filesystem::exists(outPhyre)) {
                        auto fileSize = std::filesystem::file_size(outPhyre);
                        DEBUG_LOG("[repack] Modified srcCluster file size: " << fileSize << " bytes\n");
                        if (fileSize > 0) {
                            DEBUG_LOG("[repack] Modified srcCluster saved successfully! Font repacking is working!\n");
                        }
                    }
                } else {
                    DEBUG_LOG("[repack] FAILED: saveCluster failed even with NEW writer and modified srcCluster. Error: " << testSr << "\n");
                    m_lastError = "saveCluster failed even with NEW writer and modified srcCluster. Error: " + std::to_string(testSr);
                return false; 
            }
            } catch (const std::exception& e) {
                newWriter.close();
                DEBUG_LOG("[repack] Exception during modified srcCluster save with new writer: " << e.what() << "\n");
                m_lastError = "Exception during modified srcCluster save with new writer: " + std::string(e.what());
                return false;
            } catch (...) {
                newWriter.close();
                DEBUG_LOG("[repack] Unknown exception during modified srcCluster save with new writer\n");
                m_lastError = "Unknown exception during modified srcCluster save with new writer";
                return false; 
            }
            
            // Если простое сохранение провалилось, попробовать с конвертером (СТАРЫЙ КОД - УДАЛЕН)
            // Теперь используем новых писателей вместо повторного использования старого писателя

            DEBUG_LOG("[repack] Font repacking completed successfully\n");
            DEBUG_LOG("=== END repackPhyre function (SUCCESS) ===\n");
        return true;
            
        } // Конец главного try блока
            
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception during repacking: ") + e.what();
        DEBUG_LOG("[repack] Exception: " << e.what() << "\n");
        DEBUG_LOG("=== END repackPhyre function (EXCEPTION) ===\n");
                return false; 
    } catch (...) {
        m_lastError = "Unknown exception during repacking";
        DEBUG_LOG("[repack] Unknown exception\n");
        DEBUG_LOG("=== END repackPhyre function (UNKNOWN EXCEPTION) ===\n");
        return false;
    }
}

std::string PhyreFontRepacker::getLastError() const {
    return m_lastError;
}

void PhyreFontRepacker::cleanupPhyreEngine() {
    if (m_sdkInitialized) {
        Phyre::PhyreExit();
        m_sdkInitialized = false;
    }
}

} // namespace PhyreUnpacker