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

    private:
        struct Arguments {
            std::string inputFile;      ///< Input .phyre file
            std::string inputDir;       ///< Input directory (batch mode)
            std::string outputDir;      ///< Output directory
            bool batchMode;             ///< Batch processing mode
            bool verbose;               ///< Verbose output
            bool help;                  ///< Show help
            bool version;               ///< Show version
        };

        Arguments m_args;               ///< Parsed arguments
        bool m_argsParsed;              ///< Arguments parsed flag
    };

} // namespace PhyreUnpacker
