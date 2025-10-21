#include "FontExtractorCLI.h"
#include "PhyreFontExtractor.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <exception>

namespace PhyreUnpacker {

    FontExtractorCLI::FontExtractorCLI()
        : m_argsParsed(false)
    {
        // Initialize default arguments
        m_args.batchMode = false;
        m_args.verbose = false;
        m_args.help = false;
        m_args.version = false;
    }

    FontExtractorCLI::~FontExtractorCLI()
    {
    }

    bool FontExtractorCLI::parseArguments(int argc, char* argv[])
    {
        if (argc < 2)
        {
            std::cerr << "Error: No arguments provided. Use --help for usage information." << std::endl;
            return false;
        }

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                m_args.help = true;
            }
            else if (arg == "--version" || arg == "-v")
            {
                m_args.version = true;
            }
            else if (arg == "--verbose")
            {
                m_args.verbose = true;
            }
            else if (arg == "--batch" || arg == "-b")
            {
                m_args.batchMode = true;
            }
            else if (arg == "--input" || arg == "-i")
            {
                if (i + 1 < argc)
                {
                    m_args.inputFile = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --input requires a file path" << std::endl;
                    return false;
                }
            }
            else if (arg == "--input-dir" || arg == "-d")
            {
                if (i + 1 < argc)
                {
                    m_args.inputDir = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --input-dir requires a directory path" << std::endl;
                    return false;
                }
            }
            else if (arg == "--output" || arg == "-o")
            {
                if (i + 1 < argc)
                {
                    m_args.outputDir = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --output requires a directory path" << std::endl;
                    return false;
                }
            }
            else
            {
                // If no flag is specified, treat as input file
                if (m_args.inputFile.empty() && !m_args.batchMode)
                {
                    m_args.inputFile = arg;
                }
                else
                {
                    std::cerr << "Error: Unknown argument: " << arg << std::endl;
                    return false;
                }
            }
        }

        // Validate arguments
        if (m_args.help || m_args.version)
        {
            m_argsParsed = true;
            return true;
        }

        if (m_args.batchMode)
        {
            if (m_args.inputDir.empty())
            {
                std::cerr << "Error: Batch mode requires --input-dir" << std::endl;
                return false;
            }
        }
        else
        {
            if (m_args.inputFile.empty())
            {
                std::cerr << "Error: Input file required. Use --input or specify file path." << std::endl;
                return false;
            }
        }

        if (m_args.outputDir.empty())
        {
            std::cerr << "Error: Output directory required. Use --output to specify." << std::endl;
            return false;
        }

        m_argsParsed = true;
        return true;
    }

    bool FontExtractorCLI::run()
    {
        if (!m_argsParsed)
        {
            std::cerr << "Error: Arguments not parsed. Call parseArguments() first." << std::endl;
            return false;
        }

        if (m_args.help)
        {
            showHelp();
            return true;
        }

        if (m_args.version)
        {
            showVersion();
            return true;
        }

        // Initialize PhyreEngine SDK
        PhyreFontExtractor extractor;
        if (!extractor.initializeSDK())
        {
            std::cerr << "Failed to initialize PhyreEngine SDK: " 
                      << extractor.getLastError() << std::endl;
            return false;
        }

        std::cout << "PhyreFontExtractor v1.0.0" << std::endl;
        std::cout << "=========================" << std::endl;

        bool success = false;

        if (m_args.batchMode)
        {
            std::cout << "Batch mode: Processing directory " << m_args.inputDir << std::endl;
            success = processBatch(m_args.inputDir, m_args.outputDir);
        }
        else
        {
            std::cout << "Single file mode: Processing " << m_args.inputFile << std::endl;
            success = processFile(m_args.inputFile, m_args.outputDir);
        }

        if (success)
        {
            std::cout << "Extraction completed successfully!" << std::endl;
        }
        else
        {
            std::cerr << "Extraction failed!" << std::endl;
        }

        return success;
    }

    void FontExtractorCLI::showHelp() const
    {
        std::cout << "PhyreFontExtractor - Extract fonts from .phyre files" << std::endl;
        std::cout << "====================================================" << std::endl;
        std::cout << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "  PhyreUnpacker [options] <input_file>" << std::endl;
        std::cout << "  PhyreUnpacker --batch [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -h, --help              Show this help message" << std::endl;
        std::cout << "  -v, --version           Show version information" << std::endl;
        std::cout << "  -i, --input <file>      Input .phyre file" << std::endl;
        std::cout << "  -d, --input-dir <dir>   Input directory (batch mode)" << std::endl;
        std::cout << "  -o, --output <dir>      Output directory" << std::endl;
        std::cout << "  -b, --batch             Enable batch processing mode" << std::endl;
        std::cout << "      --verbose           Enable verbose output" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  PhyreUnpacker font00.fgen.phyre -o ./output" << std::endl;
        std::cout << "  PhyreUnpacker --batch -d ./fonts -o ./extracted" << std::endl;
        std::cout << "  PhyreUnpacker -i font00.fgen.phyre -o ./output --verbose" << std::endl;
        std::cout << std::endl;
        std::cout << "Output files:" << std::endl;
        std::cout << "  - font_<size>.png       Font texture (PPM format)" << std::endl;
        std::cout << "  - font_<size>_metadata.xml  Font metadata" << std::endl;
        std::cout << "  - font_<size>_charmap.json   Character map" << std::endl;
    }

    void FontExtractorCLI::showVersion() const
    {
        std::cout << "PhyreFontExtractor v1.0.0" << std::endl;
        std::cout << "Built with PhyreEngine SDK" << std::endl;
        std::cout << "Copyright (c) 2024 PhyreUnpacker Project" << std::endl;
    }

    bool FontExtractorCLI::processFile(const std::string& inputFile, const std::string& outputDir)
    {
        try
        {
            std::cout << "Processing: " << inputFile << std::endl;
            
            // Проверяем существование файла
            if (!std::filesystem::exists(inputFile))
            {
                std::cerr << "Error: Input file does not exist: " << inputFile << std::endl;
                return false;
            }
            
            // Create output directory
            std::filesystem::create_directories(outputDir);
            
            // Initialize extractor
            PhyreFontExtractor extractor;
            if (!extractor.initializeSDK())
            {
                std::cerr << "Failed to initialize extractor: " 
                          << extractor.getLastError() << std::endl;
                return false;
            }
            
            // Load file
            if (!extractor.loadPhyreFile(inputFile))
            {
                std::cerr << "Failed to load file: " << extractor.getLastError() << std::endl;
                return false;
            }
            
            // Extract fonts
            if (!extractor.extractFonts(outputDir))
            {
                std::cerr << "Failed to extract fonts from " << inputFile 
                          << ": " << extractor.getLastError() << std::endl;
                return false;
            }
            
            std::cout << "Successfully extracted font data" << std::endl;
            
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool FontExtractorCLI::processBatch(const std::string& inputDir, const std::string& outputDir)
    {
        std::vector<std::string> phyreFiles = findPhyreFiles(inputDir);
        
        if (phyreFiles.empty())
        {
            std::cout << "No .phyre files found in directory: " << inputDir << std::endl;
            return true; // Not an error
        }

        std::cout << "Found " << phyreFiles.size() << " .phyre file(s)" << std::endl;

        size_t successCount = 0;
        size_t failCount = 0;

        for (size_t i = 0; i < phyreFiles.size(); ++i)
        {
            const std::string& file = phyreFiles[i];
            printProgress(i + 1, phyreFiles.size(), file);

            // Create subdirectory for each file
            std::filesystem::path filePath(file);
            std::string fileName = filePath.stem().string();
            std::string fileOutputDir = outputDir + "/" + fileName;

            // Initialize extractor for each file
            PhyreFontExtractor extractor;
            if (!extractor.initializeSDK())
            {
                failCount++;
                std::cerr << "  ✗ Failed to initialize extractor" << std::endl;
                continue;
            }

            if (!extractor.loadPhyreFile(file))
            {
                failCount++;
                std::cerr << "  ✗ Failed to load file" << std::endl;
                continue;
            }

            if (extractor.extractFonts(fileOutputDir))
            {
                successCount++;
                if (m_args.verbose)
                {
                    std::cout << "  ✓ Success" << std::endl;
                }
            }
            else
            {
                failCount++;
                std::cerr << "  ✗ Failed to extract fonts" << std::endl;
            }
        }

        std::cout << std::endl;
        std::cout << "Batch processing completed:" << std::endl;
        std::cout << "  Successful: " << successCount << std::endl;
        std::cout << "  Failed: " << failCount << std::endl;

        return failCount == 0;
    }

    std::vector<std::string> FontExtractorCLI::findPhyreFiles(const std::string& directory)
    {
        std::vector<std::string> phyreFiles;

        try
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
            {
                if (entry.is_regular_file())
                {
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    
                    if (extension == ".phyre")
                    {
                        phyreFiles.push_back(entry.path().string());
                    }
                }
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error scanning directory: " << ex.what() << std::endl;
        }

        return phyreFiles;
    }

    void FontExtractorCLI::printProgress(size_t current, size_t total, const std::string& itemName) const
    {
        double percentage = static_cast<double>(current) / static_cast<double>(total) * 100.0;
        
        std::cout << "[" << std::setw(3) << std::fixed << std::setprecision(0) << percentage << "%] ";
        std::cout << "[" << current << "/" << total << "] ";
        std::cout << itemName;
        
        if (!m_args.verbose)
        {
            std::cout << "\r" << std::flush;
        }
        else
        {
            std::cout << std::endl;
        }
    }

} // namespace PhyreUnpacker
