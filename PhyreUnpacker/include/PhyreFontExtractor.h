#pragma once

// Стандартные заголовки C++
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>

// Простые типы вместо PhyreEngine типов
typedef int32_t PInt32;
typedef uint32_t PUInt32;
typedef int16_t PInt16;
typedef uint16_t PUInt16;
typedef int8_t PInt8;
typedef uint8_t PUInt8;
typedef float PFloat;
typedef bool PBool;

namespace PhyreUnpacker {

    /**
     * @brief Information about a character in the font
     */
    struct CharacterInfo {
        PInt32 characterCode;        ///< Unicode code of the character
        float width;                       ///< Width of the glyph
        float height;                      ///< Height of the glyph
        float uvX;                         ///< UV X coordinate in texture
        float uvY;                         ///< UV Y coordinate in texture
        float offsetX;                     ///< X offset from baseline
        float offsetY;                     ///< Y offset from baseline
        float advanceX;                    ///< X advance to next character
        float advanceY;                    ///< Y advance to next character
        bool rotated;                      ///< Whether glyph is rotated
        PInt32 kernPairs;            ///< Number of kerning pairs
        PInt32 kernOffset;           ///< Offset in kerning table
    };

    /**
     * @brief Information about a font extracted from .phyre file
     */
    struct FontInfo {
        std::string fontName;              ///< Font name
        PUInt32 fontSize;                ///< Font size in pixels
        float lineSpacing;                ///< Distance between lines
        float baselineOffset;             ///< Baseline offset
        bool isSDF;                       ///< Whether font uses SDF rendering
        PUInt32 textureWidth;            ///< Width of font texture
        PUInt32 textureHeight;           ///< Height of font texture
        std::vector<PUInt8> pixelData;   ///< Pixel data (L8 format)
        std::vector<CharacterInfo> characters; ///< Character information
        std::vector<PInt32> kerningData; ///< Kerning data
        std::string originalFileName;      ///< Original .phyre file name
    };

    /**
     * @brief Main class for extracting fonts from .phyre files
     */
    class PhyreFontExtractor {
    public:
        /**
         * @brief Constructor
         */
        PhyreFontExtractor();

        /**
         * @brief Destructor
         */
        ~PhyreFontExtractor();

        /**
         * @brief Initialize the extractor
         * @return true if initialization successful, false otherwise
         */
        bool initializeSDK();

        /**
         * @brief Load a .phyre file
         * @param phyreFilePath Path to the .phyre file
         * @return true if loading successful, false otherwise
         */
        bool loadPhyreFile(const std::string& phyreFilePath);

        /**
         * @brief Extract fonts from loaded file
         * @param outputDirectory Directory to save extracted data
         * @return true if extraction successful, false otherwise
         */
        bool extractFonts(const std::string& outputDirectory);

        /**
         * @brief Get last error message
         * @return Error message string
         */
        std::string getLastError() const;

    private:
        bool m_sdkInitialized;                    ///< Whether extractor is initialized
        std::vector<uint8_t> m_fileData;          ///< Loaded file data
        std::string m_loadedFilePath;             ///< Path to loaded file
        std::string m_lastError;                  ///< Last error message

        /**
         * @brief Cleanup resources
         */
        void cleanupPhyreEngine();

        /**
         * @brief Save font texture as PPM file
         * @param fontInfo Font information
         * @param outputDirectory Output directory
         * @return true if successful, false otherwise
         */
        bool saveFontAsPPM(const FontInfo& fontInfo, const std::string& outputDirectory);

        /**
         * @brief Save font metadata as XML file
         * @param fontInfo Font information
         * @param outputDirectory Output directory
         * @return true if successful, false otherwise
         */
        bool saveFontAsXML(const FontInfo& fontInfo, const std::string& outputDirectory);

        /**
         * @brief Save character map as JSON file
         * @param fontInfo Font information
         * @param outputDirectory Output directory
         * @return true if successful, false otherwise
         */
        bool saveFontAsJSON(const FontInfo& fontInfo, const std::string& outputDirectory);
    };

} // namespace PhyreUnpacker