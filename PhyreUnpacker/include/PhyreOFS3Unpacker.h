#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>

namespace PhyreUnpacker {

    /**
     * @brief Information about a file entry in OFS3 archive
     */
    struct OFS3FileEntry {
        std::string fileName;       ///< Name of the file
        uint32_t offset;            ///< Offset in archive
        uint32_t size;              ///< Size of file
        uint32_t compressedSize;    ///< Compressed size (if compressed)
        bool isCompressed;          ///< Whether file is compressed
    };

    /**
     * @brief Information about OFS3 archive structure
     */
    struct OFS3ArchiveInfo {
        std::string magic;          ///< Archive magic ("OFS3")
        uint32_t headerSize;        ///< Header size (usually 0x10)
        uint16_t type;              ///< Archive type (0x00, 0x02, etc.)
        uint8_t padding;            ///< Padding value
        uint8_t subType;            ///< Subtype (0x00 or 0x01)
        uint32_t fileCount;         ///< Number of files in archive
        std::vector<OFS3FileEntry> files; ///< List of file entries
    };

    /**
     * @brief Unpacker for OFS3 (Object File System 3) archives
     */
    class PhyreOFS3Unpacker {
    public:
        /**
         * @brief Constructor
         */
        PhyreOFS3Unpacker();

        /**
         * @brief Destructor
         */
        ~PhyreOFS3Unpacker();

        /**
         * @brief Open and analyze OFS3 archive
         * @param archivePath Path to .l2b archive file
         * @param info Output structure with archive information
         * @return true if archive opened successfully, false otherwise
         */
        bool openArchive(const std::string& archivePath, OFS3ArchiveInfo& info);

        /**
         * @brief Extract all files from archive to output directory
         * @param archivePath Path to .l2b archive file
         * @param outputDirectory Output directory for extracted files
         * @param verbose Enable verbose output
         * @return true if extraction successful, false otherwise
         */
        bool extractAll(const std::string& archivePath, const std::string& outputDirectory, bool verbose = false);

        /**
         * @brief Extract specific file by index
         * @param archivePath Path to .l2b archive file
         * @param fileIndex Index of file to extract
         * @param outputPath Output path for extracted file
         * @return true if extraction successful, false otherwise
         */
        bool extractFile(const std::string& archivePath, uint32_t fileIndex, const std::string& outputPath);

        /**
         * @brief List all files in archive
         * @param archivePath Path to .l2b archive file
         * @param fileList Output vector with file names
         * @return true if listing successful, false otherwise
         */
        bool listFiles(const std::string& archivePath, std::vector<std::string>& fileList);

        /**
         * @brief Get last error message
         * @return Error message string
         */
        std::string getLastError() const;

    private:
        /**
         * @brief Read OFS3 header from file stream
         * @param file Input file stream
         * @param info Output structure with archive information
         * @return true if header read successfully, false otherwise
         */
        bool readHeader(std::ifstream& file, OFS3ArchiveInfo& info);

        /**
         * @brief Read file table from archive
         * @param file Input file stream
         * @param info Archive info structure (updated with file entries)
         * @return true if table read successfully, false otherwise
         */
        bool readFileTable(std::ifstream& file, OFS3ArchiveInfo& info);

        /**
         * @brief Internal extract file with archive info
         */
        bool extractFile(const std::string& archivePath, uint32_t fileIndex, const std::string& outputPath,
                        const OFS3ArchiveInfo& info, bool verbose);

        /**
         * @brief Decompress data if needed
         * @param compressedData Compressed data buffer
         * @param compressedSize Size of compressed data
         * @param decompressedData Output buffer for decompressed data
         * @param decompressedSize Size of decompressed data
         * @return true if decompression successful, false otherwise
         */
        bool decompressData(const uint8_t* compressedData, uint32_t compressedSize,
                           std::vector<uint8_t>& decompressedData, uint32_t decompressedSize);

        std::string m_lastError;    ///< Last error message
    };

} // namespace PhyreUnpacker

