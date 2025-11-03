#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace PhyreUnpacker {

    /**
     * @brief Information about raw data analysis
     */
    struct RawDataInfo {
        enum DataType {
            TYPE_UNKNOWN,
            TYPE_TEXTURE,
            TYPE_GEOMETRY,
            TYPE_METADATA,
            TYPE_AUDIO,
            TYPE_INDEX_TABLE
        };

        DataType dataType;              ///< Detected data type
        std::string dataSubType;        ///< More specific type (e.g., "DXT1", "RGBA8", "VertexBuffer")
        
        // Texture information
        uint32_t textureWidth;          ///< Texture width (if texture)
        uint32_t textureHeight;         ///< Texture height (if texture)
        uint32_t textureFormat;         ///< Texture format enum value
        uint32_t mipmapCount;           ///< Number of mipmaps
        std::string textureFormatName;  ///< Human-readable format name
        
        // Geometry information
        uint32_t vertexCount;           ///< Number of vertices (if geometry)
        uint32_t indexCount;            ///< Number of indices (if geometry)
        uint32_t vertexStride;          ///< Vertex stride in bytes
        bool hasNormals;                ///< Whether geometry has normals
        bool hasUVs;                    ///< Whether geometry has UV coordinates
        
        // General information
        uint64_t dataOffset;            ///< Offset where actual data starts (after header/metadata)
        uint64_t dataSize;              ///< Size of actual data
        bool isLittleEndian;            ///< Endianness of data
        
        std::vector<uint8_t> sampleData; ///< Sample of data for pattern analysis
    };

    /**
     * @brief Analyzer for raw binary data extracted from OFS3 archives
     */
    class PhyreRawDataAnalyzer {
    public:
        /**
         * @brief Analyze raw binary data to determine its type and structure
         * @param filePath Path to the file to analyze
         * @param info Output structure with analysis results
         * @return true if analysis was successful, false otherwise
         */
        static bool analyzeRawData(const std::string& filePath, RawDataInfo& info);

        /**
         * @brief Detect if data might be a texture
         * @param data Raw data bytes
         * @param dataSize Size of data
         * @param info Output structure with texture information
         * @return true if texture format detected
         */
        static bool detectTexture(const uint8_t* data, size_t dataSize, RawDataInfo& info);

        /**
         * @brief Detect if data might be geometry
         * @param data Raw data bytes
         * @param dataSize Size of data
         * @param info Output structure with geometry information
         * @return true if geometry format detected
         */
        static bool detectGeometry(const uint8_t* data, size_t dataSize, RawDataInfo& info);

        /**
         * @brief Detect if data might be an index table or metadata
         * @param data Raw data bytes
         * @param dataSize Size of data
         * @param info Output structure with metadata information
         * @return true if metadata/index table format detected
         */
        static bool detectMetadata(const uint8_t* data, size_t dataSize, RawDataInfo& info);

        /**
         * @brief Print detailed analysis report
         * @param info Raw data information
         */
        static void printAnalysisReport(const RawDataInfo& info);
    };

} // namespace PhyreUnpacker

