#pragma once

#include <string>
#include <vector>

namespace PhyreUnpacker {

    /**
     * @brief Command line interface for PhyreFontExtractor
     */
    class FontExtractorCLI {
    public:
        /**
         * @brief Constructor
         */
        FontExtractorCLI();

        /**
         * @brief Destructor
         */
        ~FontExtractorCLI();

        /**
         * @brief Parse command line arguments
         * @param argc Argument count
         * @param argv Argument vector
         * @return true if parsing successful, false otherwise
         */
        bool parseArguments(int argc, char* argv[]);

        /**
         * @brief Run the extraction process
         * @return true if successful, false otherwise
         */
        bool run();

        /**
         * @brief Show help message
         */
        void showHelp() const;

        /**
         * @brief Show version information
         */
        void showVersion() const;

    private:
        /**
         * @brief Process single file
         * @param inputFile Input .phyre file path
         * @param outputDir Output directory
         * @return true if successful, false otherwise
         */
        bool processFile(const std::string& inputFile, const std::string& outputDir);

        /**
         * @brief Process multiple files in batch mode
         * @param inputDir Input directory containing .phyre files
         * @param outputDir Output directory
         * @return true if successful, false otherwise
         */
        bool processBatch(const std::string& inputDir, const std::string& outputDir);

        /**
         * @brief Find all .phyre files in directory
         * @param directory Directory to search
         * @return Vector of .phyre file paths
         */
        std::vector<std::string> findPhyreFiles(const std::string& directory);

        /**
         * @brief Print progress information
         * @param current Current item number
         * @param total Total number of items
         * @param itemName Name of current item
         */
        void printProgress(size_t current, size_t total, const std::string& itemName) const;

        /**
         * @brief Compare two .fgen files and write a report into output directory
         * @param originalFgen Path to original .fgen
         * @param modifiedFgen Path to modified .fgen (after Phyre Font Editor)
         * @param outputDir Output directory for report
         * @return true if comparison was successful (files parsed), false otherwise
         */
        bool compareFgenFiles(const std::string &originalFgen, const std::string &modifiedFgen, const std::string &outputDir);

        /**
         * @brief Repack .phyre with font data (stub; not implemented yet)
         * @return false always (until implemented)
         */
        bool repackPhyreFile();

        /**
         * @brief Compare two binary files byte-by-byte and report differences
         * @param file1Path Path to first file
         * @param file2Path Path to second file
         * @param outputDir Output directory for report
         * @return true if comparison was successful, false otherwise
         */
        bool compareBinaryDumps(const std::string& file1Path, const std::string& file2Path, const std::string& outputDir);

        /**
         * @brief Analyze file header to check if it can be loaded by PhyreEngine
         * @param filePath Path to file to analyze
         * @return true if analysis was successful, false otherwise
         */
        bool analyzeFileHeader(const std::string& filePath);

        /**
         * @brief Extract OFS3 archive
         * @param archivePath Path to .l2b archive
         * @param outputDirectory Output directory for extracted files
         * @return true if extraction successful, false otherwise
         */
        bool extractOFS3Archive(const std::string& archivePath, const std::string& outputDirectory);

    private:
        struct Arguments {
            std::string inputFile;      ///< Input .phyre file
            std::string inputDir;       ///< Input directory (batch mode)
            std::string outputDir;      ///< Output directory
            bool batchMode;             ///< Batch processing mode
            bool verbose;               ///< Verbose output
            bool help;                  ///< Show help
            bool version;               ///< Show version
            bool compareFgen;           ///< Compare two .fgen files instead of extracting
            std::string compareFgenOriginal; ///< Path to original .fgen
            std::string compareFgenModified; ///< Path to modified .fgen
            bool repackPhyre;           ///< Repack to .phyre mode (stub)
            std::string repackSourcePhyre;   ///< Source .phyre to base on
            std::string repackOutPhyre;      ///< Output .phyre path
            std::string repackFgenPath;      ///< Optional .fgen to use
            bool repackKeepAtlas;            ///< Keep original atlas/UVs
            std::string repackTtfName;       ///< Optional TTF name override
            bool repackOnlyListed;           ///< Replace only glyphs listed in .fgen
            bool compareBinaryDumps;         ///< Compare two binary files byte-by-byte
            std::string binaryDumpFile1;     ///< First file for binary comparison
            std::string binaryDumpFile2;     ///< Second file for binary comparison
            bool analyzeFile;                ///< Analyze file header mode
            std::string analyzeFilePath;     ///< Path to file to analyze
            bool extractOFS3;                ///< Extract OFS3 archive mode
            std::string extractOFS3Archive;  ///< Path to OFS3 archive to extract
        };

        Arguments m_args;               ///< Parsed arguments
        bool m_argsParsed;              ///< Arguments parsed flag
    };

} // namespace PhyreUnpacker
