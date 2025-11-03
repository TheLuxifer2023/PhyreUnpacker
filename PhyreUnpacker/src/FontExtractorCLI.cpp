#include "FontExtractorCLI.h"
#include "PhyreFontExtractor.h"
#include "PhyreFontRepacker.h"
#include "PhyreFileAnalyzer.h"
#include "PhyreOFS3Unpacker.h"
#include "PhyreRawDataAnalyzer.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <exception>
#include <set>
#include <fstream>

namespace PhyreUnpacker {

    FontExtractorCLI::FontExtractorCLI()
        : m_argsParsed(false)
    {
        // Initialize default arguments
        m_args.batchMode = false;
        m_args.verbose = false;
        m_args.help = false;
        m_args.version = false;
        m_args.compareFgen = false;
        m_args.repackPhyre = false;
        m_args.repackKeepAtlas = true;
        m_args.repackOnlyListed = false;
            m_args.compareBinaryDumps = false;
            m_args.analyzeFile = false;
            m_args.extractOFS3 = false;
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
            else if (arg == "--compare-fgen")
            {
                m_args.compareFgen = true;
                if (i + 2 < argc)
                {
                    m_args.compareFgenOriginal = argv[++i];
                    m_args.compareFgenModified = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --compare-fgen requires two file paths: <original.fgen> <modified.fgen>" << std::endl;
                    return false;
                }
            }
            else if (arg == "--repack-phyre")
            {
                m_args.repackPhyre = true;
            }
            else if (arg == "--repack-source")
            {
                if (i + 1 < argc) m_args.repackSourcePhyre = argv[++i]; else { std::cerr << "Error: --repack-source <file.phyre>" << std::endl; return false; }
            }
            else if (arg == "--repack-out")
            {
                if (i + 1 < argc) m_args.repackOutPhyre = argv[++i]; else { std::cerr << "Error: --repack-out <file.phyre>" << std::endl; return false; }
            }
            else if (arg == "--repack-fgen")
            {
                if (i + 1 < argc) m_args.repackFgenPath = argv[++i]; else { std::cerr << "Error: --repack-fgen <file.fgen>" << std::endl; return false; }
            }
            else if (arg == "--repack-keep-atlas")
            {
                m_args.repackKeepAtlas = true;
            }
            else if (arg == "--repack-only-listed")
            {
                m_args.repackOnlyListed = true;
            }
            else if (arg == "--repack-ttf-name")
            {
                if (i + 1 < argc) m_args.repackTtfName = argv[++i]; else { std::cerr << "Error: --repack-ttf-name <TTFName.ttf>" << std::endl; return false; }
            }
            else if (arg == "--compare-binary-dumps")
            {
                m_args.compareBinaryDumps = true;
                if (i + 2 < argc)
                {
                    m_args.binaryDumpFile1 = argv[++i];
                    m_args.binaryDumpFile2 = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --compare-binary-dumps requires two file paths: <file1> <file2>" << std::endl;
                    return false;
                }
            }
            else if (arg == "--analyze" || arg == "-a")
            {
                m_args.analyzeFile = true;
                if (i + 1 < argc)
                {
                    m_args.analyzeFilePath = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --analyze requires a file path" << std::endl;
                    return false;
                }
            }
            else if (arg == "--extract-ofs3" || arg == "--extract-l2b")
            {
                m_args.extractOFS3 = true;
                if (i + 1 < argc)
                {
                    m_args.extractOFS3Archive = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --extract-ofs3 requires an archive path" << std::endl;
                    return false;
                }
            }
            else
            {
                // If no flag is specified, treat as input file
                if (m_args.inputFile.empty() && !m_args.batchMode && !m_args.compareFgen && !m_args.repackPhyre && !m_args.analyzeFile && !m_args.extractOFS3)
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

        if (m_args.compareFgen)
        {
            if (m_args.compareFgenOriginal.empty() || m_args.compareFgenModified.empty())
            {
                std::cerr << "Error: --compare-fgen requires <original.fgen> and <modified.fgen>" << std::endl;
                return false;
            }
            // outputDir still required to write report
            if (m_args.outputDir.empty())
            {
                std::cerr << "Error: Output directory required. Use --output to specify." << std::endl;
                return false;
            }
            m_argsParsed = true;
            return true;
        }

        if (m_args.repackPhyre)
        {
            if (m_args.repackSourcePhyre.empty() || m_args.repackOutPhyre.empty())
            {
                std::cerr << "Error: --repack-phyre requires --repack-source <src.phyre> and --repack-out <out.phyre>" << std::endl;
                return false;
            }
            m_argsParsed = true;
            return true;
        }

        if (m_args.compareBinaryDumps)
        {
            if (m_args.binaryDumpFile1.empty() || m_args.binaryDumpFile2.empty())
            {
                std::cerr << "Error: --compare-binary-dumps requires two file paths" << std::endl;
                return false;
            }
            if (m_args.outputDir.empty())
            {
                std::cerr << "Error: Output directory required for report. Use --output to specify." << std::endl;
                return false;
            }
            m_argsParsed = true;
            return true;
        }

        if (m_args.analyzeFile)
        {
            if (m_args.analyzeFilePath.empty())
            {
                std::cerr << "Error: --analyze requires a file path" << std::endl;
                return false;
            }
            m_argsParsed = true;
            return true;
        }

        if (m_args.extractOFS3)
        {
            if (m_args.extractOFS3Archive.empty())
            {
                std::cerr << "Error: --extract-ofs3 requires an archive path" << std::endl;
                return false;
            }
            if (m_args.outputDir.empty())
            {
                std::cerr << "Error: Output directory required. Use --output to specify." << std::endl;
                return false;
            }
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
        else if (!m_args.analyzeFile && !m_args.extractOFS3)
        {
            // Only require inputFile for regular font extraction mode
            if (m_args.inputFile.empty())
            {
                std::cerr << "Error: Input file required. Use --input or specify file path." << std::endl;
                return false;
            }
        }

        // Output directory required
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

        if (m_args.compareFgen)
        {
            std::filesystem::create_directories(m_args.outputDir);
            bool ok = compareFgenFiles(m_args.compareFgenOriginal, m_args.compareFgenModified, m_args.outputDir);
            if (ok) std::cout << "Compare completed successfully" << std::endl;
            else std::cerr << "Compare failed" << std::endl;
            return ok;
        }

        if (m_args.repackPhyre)
        {
            try {
                std::cout << "Starting repack operation..." << std::endl;
                PhyreFontRepacker repacker;
                std::cout << "Initializing SDK..." << std::endl;
                if (!repacker.initializeSDK()) { 
                    std::cerr << "Init failed: " << repacker.getLastError() << std::endl; 
                    return false; 
                }
                std::cout << "SDK initialized successfully." << std::endl;
                if (m_args.repackFgenPath.empty()) { 
                    std::cerr << "Error: --repack-fgen <file.fgen> is required" << std::endl; 
                    return false; 
                }
                std::cout << "Repacking from: " << m_args.repackSourcePhyre << " to: " << m_args.repackOutPhyre << " using: " << m_args.repackFgenPath << std::endl;
                bool ok = repacker.repackPhyre(m_args.repackSourcePhyre, m_args.repackOutPhyre, m_args.repackFgenPath, m_args.repackOnlyListed, m_args.repackTtfName);
                if (!ok) { 
                    std::cerr << "Repack failed: " << repacker.getLastError() << std::endl; 
                    return false;
                } else { 
                    std::cout << "Repack completed: " << m_args.repackOutPhyre << std::endl;
                    return true;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception during repack: " << e.what() << std::endl;
                return false;
            } catch (...) {
                std::cerr << "Unknown exception during repack" << std::endl;
                return false;
            }
        }

        if (m_args.compareBinaryDumps)
        {
            std::filesystem::create_directories(m_args.outputDir);
            bool ok = compareBinaryDumps(m_args.binaryDumpFile1, m_args.binaryDumpFile2, m_args.outputDir);
            if (ok) std::cout << "Binary comparison completed successfully" << std::endl;
            else std::cerr << "Binary comparison failed" << std::endl;
            return ok;
        }

        if (m_args.analyzeFile)
        {
            bool ok = analyzeFileHeader(m_args.analyzeFilePath);
            return ok;
        }

        if (m_args.extractOFS3)
        {
            bool ok = extractOFS3Archive(m_args.extractOFS3Archive, m_args.outputDir);
            if (ok) std::cout << "OFS3 extraction completed successfully" << std::endl;
            else std::cerr << "OFS3 extraction failed" << std::endl;
            return ok;
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
        std::cout << "      --compare-fgen <orig.fgen> <mod.fgen>  Compare two .fgen files and write report to --output" << std::endl;
        std::cout << "      --compare-binary-dumps <file1> <file2>  Compare two binary files byte-by-byte" << std::endl;
        std::cout << "  -a, --analyze <file>    Analyze file header to check if it can be loaded by PhyreEngine" << std::endl;
        std::cout << "      --extract-ofs3, --extract-l2b <archive>  Extract OFS3 archive (.l2b file)" << std::endl;
        std::cout << "      --convert-texture, --convert-to-dds <file> [output]  Convert raw texture to DDS format" << std::endl;
        std::cout << "      --repack-phyre      Repack to .phyre (stub; prints guidance)" << std::endl;
        std::cout << "          --repack-source <src.phyre>  Source .phyre to base on" << std::endl;
        std::cout << "          --repack-out <out.phyre>     Output .phyre path" << std::endl;
        std::cout << "          --repack-fgen <file.fgen>    Optional .fgen to use (TTF name/charset)" << std::endl;
        std::cout << "          --repack-keep-atlas          Keep original atlas/UV (default true)" << std::endl;
        std::cout << "          --repack-only-listed         Replace only glyphs listed in .fgen" << std::endl;
        std::cout << "          --repack-ttf-name <TTF.ttf>  Override TTF name in .fgen header" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  PhyreUnpacker font00.fgen.phyre -o ./output" << std::endl;
        std::cout << "  PhyreUnpacker --batch -d ./fonts -o ./extracted" << std::endl;
        std::cout << "  PhyreUnpacker -i font00.fgen.phyre -o ./output --verbose" << std::endl;
        std::cout << "  PhyreUnpacker --compare-fgen a.fgen b.fgen -o ./report" << std::endl;
        std::cout << "  PhyreUnpacker --analyze file.l2b  Analyze .l2b file header" << std::endl;
        std::cout << "  PhyreUnpacker --extract-ofs3 file.l2b -o ./extracted  Extract .l2b archive" << std::endl;
        std::cout << "  PhyreUnpacker --convert-texture extracted/file_0.phyre  Convert texture to DDS" << std::endl;
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

    static bool readAllLines(const std::string &path, std::vector<std::string> &lines)
    {
        try {
            std::ifstream f(path);
            if (!f.is_open()) return false;
            std::string s;
            while (std::getline(f, s)) { if (!s.empty() && s.back()=='\r') s.pop_back(); lines.push_back(s); }
            return true;
        } catch (...) { return false; }
    }

    bool FontExtractorCLI::compareFgenFiles(const std::string &originalFgen, const std::string &modifiedFgen, const std::string &outputDir)
    {
        std::vector<std::string> a, b;
        if (!readAllLines(originalFgen, a)) { std::cerr << "Cannot read original: " << originalFgen << std::endl; return false; }
        if (!readAllLines(modifiedFgen, b)) { std::cerr << "Cannot read modified: " << modifiedFgen << std::endl; return false; }

        auto norm = [](const std::vector<std::string> &src){
            struct F { static bool isHex(const std::string &s){ if (s.empty()) return false; for(char c: s){ if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return false; } return true; } };
            std::set<std::string> hex;
            std::vector<std::string> header;
            for (size_t i=0;i<src.size();++i) {
                if (i<11) header.push_back(src[i]);
                else if (F::isHex(src[i])) {
                    std::string s = src[i];
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    hex.insert(s);
                }
            }
            return std::pair<std::vector<std::string>, std::set<std::string>>(header, hex);
        };

        auto pa = norm(a);
        auto pb = norm(b);

        const std::filesystem::path outPath = std::filesystem::path(outputDir) / "fgen_compare_report.txt";
        std::ofstream rpt(outPath.string(), std::ios::out | std::ios::trunc);
        if (!rpt.is_open()) { std::cerr << "Cannot write report: " << outPath.string() << std::endl; return false; }

        auto safeGet = [](const std::vector<std::string> &v, size_t i){ return (i<v.size()? v[i]: std::string()); };

        rpt << "=== FGEN Header Diff ===\n";
        static const char* hdrNames[11] = {"magic","ttf","dummy","packing","width","height","size","sdf","charScale","charPad","mipPad"};
        for (size_t i=0;i<11;++i) {
            std::string av = safeGet(pa.first, i);
            std::string bv = safeGet(pb.first, i);
            rpt << hdrNames[i] << ": \"" << av << "\" vs \"" << bv << "\"" << (av==bv? " (OK)":" (DIFF)") << "\n";
        }

        // Char sets
        rpt << "\n=== Characters Diff (hex) ===\n";
        std::set<std::string> onlyA, onlyB;
        for (auto &s: pa.second) if (!pb.second.count(s)) onlyA.insert(s);
        for (auto &s: pb.second) if (!pa.second.count(s)) onlyB.insert(s);

        rpt << "only_in_original (" << onlyA.size() << "):\n";
        for (auto &s: onlyA) rpt << s << "\n";
        rpt << "\nonly_in_modified (" << onlyB.size() << "):\n";
        for (auto &s: onlyB) rpt << s << "\n";

        rpt.close();
        if (m_args.verbose) std::cout << "Report: " << outPath.string() << std::endl;
        return true;
    }

    bool FontExtractorCLI::repackPhyreFile()
    {
        std::cout << "[stub] Repack to .phyre is not implemented in this tool yet.\n";
        std::cout << "Planned steps:\n";
        std::cout << "  1) Use PhyreClusterWriterBinary to create a cluster for target platform.\n";
        std::cout << "  2) Construct PBitmapFont + PTexture2D inside a new PCluster from exported data.\n";
        std::cout << "  3) Write header/namespace/fixups matching target platform (DX11/GL/GNM).\n";
        std::cout << "Hints:\n";
        std::cout << "  --repack-source:  Source .phyre to clone structure from\n";
        std::cout << "  --repack-out:     Output .phyre path\n";
        std::cout << "  --repack-fgen:    Optional .fgen to inject header/charset (line2 TTF)\n";
        std::cout << "  --repack-ttf-name:Override TTF name used in .fgen header\n";
        std::cout << "  --repack-keep-atlas:Keep original atlas/UVs (no texture regeneration)\n";
        return false;
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

    bool FontExtractorCLI::compareBinaryDumps(const std::string& file1Path, const std::string& file2Path, const std::string& outputDir)
    {
        std::cout << "Comparing binary files:" << std::endl;
        std::cout << "  File 1: " << file1Path << std::endl;
        std::cout << "  File 2: " << file2Path << std::endl;

        // Open both files
        std::ifstream file1(file1Path, std::ios::binary);
        std::ifstream file2(file2Path, std::ios::binary);

        if (!file1.is_open())
        {
            std::cerr << "Error: Cannot open file 1: " << file1Path << std::endl;
            return false;
        }

        if (!file2.is_open())
        {
            std::cerr << "Error: Cannot open file 2: " << file2Path << std::endl;
            return false;
        }

        // Get file sizes
        file1.seekg(0, std::ios::end);
        file2.seekg(0, std::ios::end);
        std::streampos size1 = file1.tellg();
        std::streampos size2 = file2.tellg();
        file1.seekg(0, std::ios::beg);
        file2.seekg(0, std::ios::beg);

        std::cout << "  Size 1: " << size1 << " bytes" << std::endl;
        std::cout << "  Size 2: " << size2 << " bytes" << std::endl;

        // Prepare report
        std::filesystem::path outputDirPath(outputDir);
        std::filesystem::create_directories(outputDirPath);
        std::filesystem::path reportPath = outputDirPath / "binary_compare_report.txt";
        std::ofstream report(reportPath);

        if (!report.is_open())
        {
            std::cerr << "Error: Cannot create report file: " << reportPath << std::endl;
            return false;
        }

        report << "Binary File Comparison Report" << std::endl;
        report << "=============================" << std::endl;
        report << std::endl;
        report << "File 1: " << file1Path << " (" << size1 << " bytes)" << std::endl;
        report << "File 2: " << file2Path << " (" << size2 << " bytes)" << std::endl;
        report << std::endl;

        // Compare byte-by-byte
        size_t pos = 0;
        size_t diffCount = 0;
        const size_t maxDiffsToReport = 1000; // Limit report size
        size_t size = static_cast<size_t>(std::min(size1, size2));

        while (pos < size)
        {
            uint8_t byte1, byte2;
            file1.read(reinterpret_cast<char*>(&byte1), 1);
            file2.read(reinterpret_cast<char*>(&byte2), 1);

            if (byte1 != byte2)
            {
                diffCount++;
                if (diffCount <= maxDiffsToReport)
                {
                    report << std::hex << std::setfill('0') << std::setw(8) << pos
                           << ": " << std::setw(2) << static_cast<int>(byte1) 
                           << " -> " << std::setw(2) << static_cast<int>(byte2) << std::dec << std::endl;
                }
            }
            pos++;
        }

        report << std::endl;
        report << "Total differences: " << diffCount << std::endl;

        if (size1 != size2)
        {
            report << "File size mismatch: " << size1 << " vs " << size2 << std::endl;
        }

        report.close();

        std::cout << "  Report saved to: " << reportPath << std::endl;
        std::cout << "  Total differences: " << diffCount << std::endl;

        return true;
    }

    bool FontExtractorCLI::analyzeFileHeader(const std::string& filePath)
    {
        std::cout << "Analyzing file: " << filePath << std::endl;
        std::cout << std::endl;

        FileHeaderInfo info;
        if (!PhyreFileAnalyzer::analyzeFileHeader(filePath, info))
        {
            std::cerr << "Error: Cannot read file: " << filePath << std::endl;
            return false;
        }

        PhyreFileAnalyzer::printAnalysisReport(filePath, info);

        // Also perform raw data analysis if file is not a Phyre file
        if (!info.isValidPhyreFile && info.magicNumber != "OFS3") {
            std::cout << std::endl;
            std::cout << "=== Deep Raw Data Analysis ===" << std::endl;
            
            RawDataInfo rawInfo;
            if (PhyreRawDataAnalyzer::analyzeRawData(filePath, rawInfo)) {
                PhyreRawDataAnalyzer::printAnalysisReport(rawInfo);
            }
        }

        return true;
    }


    bool FontExtractorCLI::extractOFS3Archive(const std::string& archivePath, const std::string& outputDirectory) {
        std::cout << "Extracting OFS3 archive: " << archivePath << std::endl;
        std::cout << "Output directory: " << outputDirectory << std::endl;
        std::cout << std::endl;

        PhyreOFS3Unpacker unpacker;
        
        // First, try to open and analyze archive
        PhyreUnpacker::OFS3ArchiveInfo info;
        if (!unpacker.openArchive(archivePath, info)) {
            std::cerr << "Error opening archive: " << unpacker.getLastError() << std::endl;
            return false;
        }

        // Always show basic info, verbose shows more
        std::cout << "Archive Info:" << std::endl;
        std::cout << "  Magic: " << info.magic << std::endl;
        std::cout << "  Header Size: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << info.headerSize << std::dec << std::endl;
        std::cout << "  Type: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (uint32_t)info.type << std::dec << std::endl;
        std::cout << "  Padding: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (uint32_t)info.padding << std::dec << std::endl;
        std::cout << "  SubType: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (uint32_t)info.subType << std::dec << std::endl;
        std::cout << "  File Count: " << info.fileCount << std::endl;
        
        // Show raw header bytes for debugging
        if (m_args.verbose) {
            std::ifstream debugFile(archivePath, std::ios::binary);
            if (debugFile.is_open()) {
                std::vector<uint8_t> rawBytes(64);
                debugFile.read(reinterpret_cast<char*>(rawBytes.data()), 64);
                std::cout << "  Raw header (first 64 bytes):" << std::endl;
                for (size_t i = 0; i < 64; i += 16) {
                    std::cout << "    " << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
                    for (size_t j = 0; j < 16 && (i+j) < 64; ++j) {
                        std::cout << std::setw(2) << static_cast<unsigned>(rawBytes[i+j]) << " ";
                    }
                    std::cout << std::dec << std::endl;
                }
            }
        }
        
        std::cout << "  Found " << info.files.size() << " file entries" << std::endl;
        if (!info.files.empty()) {
            std::cout << "  File list:" << std::endl;
            for (size_t i = 0; i < std::min<size_t>(10, info.files.size()); ++i) {
                const auto& f = info.files[i];
                std::cout << "    [" << i << "] " << f.fileName 
                          << " offset=0x" << std::hex << f.offset << std::dec
                          << " size=" << f.size << " bytes";
                if (f.size == 0) {
                    std::cout << " (unknown)";
                }
                std::cout << std::endl;
            }
            if (info.files.size() > 10) {
                std::cout << "    ... and " << (info.files.size() - 10) << " more files" << std::endl;
            }
        }
        std::cout << std::endl;

        bool ok = unpacker.extractAll(archivePath, outputDirectory, m_args.verbose);

        if (!ok && !unpacker.getLastError().empty()) {
            std::cerr << "Error: " << unpacker.getLastError() << std::endl;
        }

        return ok;
    }

} // namespace PhyreUnpacker
