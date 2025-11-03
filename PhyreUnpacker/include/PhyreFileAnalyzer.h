#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace PhyreUnpacker {

    /**
     * @brief Information about file header analysis
     */
    struct FileHeaderInfo {
        bool isValidPhyreFile;           ///< Whether file starts with 'PHYR' marker
        bool hasPhyreMarker;             ///< Whether 'PHYR' marker found (possibly swapped endianness)
        std::string fileSignature;       ///< First 16 bytes as hex string
        std::string magicNumber;         ///< First 4 bytes as ASCII/hex
        uint32_t phyreMarker;            ///< Phyre marker value (little/big endian)
        uint32_t headerSize;             ///< Size from header (if valid Phyre)
        uint32_t platformID;             ///< Platform ID from header (if valid Phyre)
        uint64_t fileSize;               ///< Total file size
        bool isLittleEndian;             ///< Detected endianness
        std::vector<uint8_t> headerBytes; ///< First 256 bytes of file
        
        // Compression detection
        bool isGzipCompressed;           ///< Whether file appears to be gzip compressed (0x1f8b)
        bool isZlibCompressed;           ///< Whether file appears to be zlib compressed
        bool mightBeCompressed;          ///< General compression indicator
        
        // Format detection
        std::string detectedFormat;      ///< Detected file format (e.g., "GZIP", "ZLIB", "DDS", "WAV", etc.)
        std::string formatDescription;   ///< Description of detected format
        
        // Statistics
        double entropy;                  ///< File entropy (high = likely compressed/encrypted)
        uint32_t zeroBytes;              ///< Count of zero bytes in first 256 bytes
    };

    /**
     * @brief Analyzer for binary file headers (especially .l2b and .phyre files)
     */
    class PhyreFileAnalyzer {
    public:
        /**
         * @brief Analyze a binary file header to determine if it's a Phyre file or archive
         * @param filePath Path to the file to analyze
         * @param info Output structure with analysis results
         * @return true if file was read successfully, false otherwise
         */
        static bool analyzeFileHeader(const std::string& filePath, FileHeaderInfo& info);

        /**
         * @brief Check if file can be loaded directly by PhyreEngine
         * @param filePath Path to the file to check
         * @return true if file appears to be a valid Phyre binary file
         */
        static bool canLoadWithPhyreEngine(const std::string& filePath);

        /**
         * @brief Print detailed analysis report
         * @param filePath Path to the file being analyzed (needed for marker search)
         * @param info File header information
         */
        static void printAnalysisReport(const std::string& filePath, const FileHeaderInfo& info);
        
        /**
         * @brief Search for PHYR markers inside a file
         * @param filePath Path to the file to search
         * @param offsets Output vector with offsets where PHYR markers were found
         * @return Number of markers found
         */
        static size_t searchForPhyreMarkers(const std::string& filePath, std::vector<uint64_t>& offsets);
    };

} // namespace PhyreUnpacker

