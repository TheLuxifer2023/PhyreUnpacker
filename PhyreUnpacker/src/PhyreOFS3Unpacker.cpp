#include "PhyreOFS3Unpacker.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sstream>

namespace PhyreUnpacker {

    PhyreOFS3Unpacker::PhyreOFS3Unpacker()
        : m_lastError()
    {
    }

    PhyreOFS3Unpacker::~PhyreOFS3Unpacker()
    {
    }

    bool PhyreOFS3Unpacker::openArchive(const std::string& archivePath, OFS3ArchiveInfo& info) {
        std::ifstream file(archivePath, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Cannot open archive file: " + archivePath;
            return false;
        }

        if (!readHeader(file, info)) {
            return false;
        }

        if (!readFileTable(file, info)) {
            return false;
        }

        return true;
    }

    bool PhyreOFS3Unpacker::readHeader(std::ifstream& file, OFS3ArchiveInfo& info) {
        // Read magic number (4 bytes) - "OFS3" = 0x3353464F
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
        if (file.gcount() != sizeof(uint32_t)) {
            m_lastError = "Failed to read magic number";
            return false;
        }
        
        if (magic != 0x3353464F) {
            m_lastError = "Invalid OFS3 archive: magic number mismatch (expected 0x3353464F, got 0x" 
                         + std::to_string(magic) + ")";
            return false;
        }
        info.magic = "OFS3";

        // Read header size (4 bytes) - usually 0x10
        file.read(reinterpret_cast<char*>(&info.headerSize), sizeof(uint32_t));
        if (file.gcount() != sizeof(uint32_t)) {
            m_lastError = "Failed to read header size";
            return false;
        }

        // Read type (2 bytes) - 0x00, 0x02, etc.
        file.read(reinterpret_cast<char*>(&info.type), sizeof(uint16_t));
        if (file.gcount() != sizeof(uint16_t)) {
            m_lastError = "Failed to read type";
            return false;
        }

        // Read padding (1 byte)
        file.read(reinterpret_cast<char*>(&info.padding), sizeof(uint8_t));
        if (file.gcount() != sizeof(uint8_t)) {
            m_lastError = "Failed to read padding";
            return false;
        }

        // Read subtype (1 byte) - 0x00 or 0x01
        file.read(reinterpret_cast<char*>(&info.subType), sizeof(uint8_t));
        if (file.gcount() != sizeof(uint8_t)) {
            m_lastError = "Failed to read subtype";
            return false;
        }

        // Read size (4 bytes) - total archive size
        uint32_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        if (file.gcount() != sizeof(uint32_t)) {
            m_lastError = "Failed to read size";
            return false;
        }

        // Read number of files (4 bytes)
        file.read(reinterpret_cast<char*>(&info.fileCount), sizeof(uint32_t));
        if (file.gcount() != sizeof(uint32_t)) {
            m_lastError = "Failed to read file count";
            return false;
        }

        return true;
    }

    bool PhyreOFS3Unpacker::readFileTable(std::ifstream& file, OFS3ArchiveInfo& info) {
        info.files.clear();

        // Seek to after header (headerSize bytes), which is where table starts
        // After headerSize is NFiles, then the table starts 4 bytes later
        file.seekg(info.headerSize + 4, std::ios::beg);

        // Get file size for SubType 1 calculation
        uint64_t savePos = file.tellg();
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        file.seekg(savePos, std::ios::beg);

        // Allocate arrays for file info
        std::vector<uint32_t> fileOffsets(info.fileCount);
        std::vector<uint32_t> fileSizes(info.fileCount);
        std::vector<uint32_t> fileNameOffsets(info.fileCount);

        // Read offset, size, and nameoffset for each file
        for (uint32_t i = 0; i < info.fileCount; ++i) {
            uint32_t temp;
            
            // Read file offset (4 bytes)
            file.read(reinterpret_cast<char*>(&temp), sizeof(uint32_t));
            fileOffsets[i] = (file.gcount() == sizeof(uint32_t)) ? temp + 0x10 : 0;
            
            // Read file size (4 bytes)
            file.read(reinterpret_cast<char*>(&temp), sizeof(uint32_t));
            fileSizes[i] = (file.gcount() == sizeof(uint32_t)) ? temp : 0;
            
            // Read filename offset only if Type == 0x02
            if (info.type == 0x02) {
                file.read(reinterpret_cast<char*>(&temp), sizeof(uint32_t));
                fileNameOffsets[i] = (file.gcount() == sizeof(uint32_t)) ? temp + 0x10 : 0;
            } else {
                fileNameOffsets[i] = 0;
            }
        }

        // For SubType 1, calculate file sizes (not specified in table)
        if (info.subType == 1) {
            for (uint32_t i = 0; i < info.fileCount; ++i) {
                if (fileOffsets[i] != 0) {
                    if (i == info.fileCount - 1) {
                        // Last file - size is to end of archive
                        fileSizes[i] = static_cast<uint32_t>(fileSize) - fileOffsets[i];
                    } else if (i < info.fileCount - 1) {
                        // Size is distance to next file
                        fileSizes[i] = fileOffsets[i + 1] - fileOffsets[i];
                    }
                }
            }
        }

        // Create file entries
        for (uint32_t i = 0; i < info.fileCount; ++i) {
            OFS3FileEntry entry;
            entry.offset = fileOffsets[i];
            entry.size = fileSizes[i];
            entry.compressedSize = 0;
            entry.isCompressed = false;

            // Read filename if Type == 0x02
            if (info.type == 0x02 && fileNameOffsets[i] != 0) {
                uint32_t savePos = static_cast<uint32_t>(file.tellg());
                file.seekg(fileNameOffsets[i], std::ios::beg);
                
                std::string name;
                char c;
                uint32_t printableChars = 0;
                while (static_cast<uint64_t>(file.tellg()) < fileSize && name.length() < 256) {
                    file.read(&c, 1);
                    if (file.gcount() != 1) break;
                    
                    if (c != 0) {
                    // Only add printable ASCII characters
                    if (c >= 32 && c <= 126) {
                        name += c;
                            printableChars++;
                        } else {
                            // Non-printable character found, likely not a filename
                            break;
                        }
                    } else {
                        break;
                    }
                }
                
                // Format filename: (0000)_filename, but only if we found printable chars
                if (printableChars > 0 && !name.empty()) {
                    // Extract just filename from full path
                    size_t lastSlash = name.find_last_of("/\\");
                    if (lastSlash != std::string::npos) {
                        name = name.substr(lastSlash + 1);
                    }
                    std::stringstream ss;
                    ss << "(" << std::setfill('0') << std::setw(4) << i << ")_" << name;
                    entry.fileName = ss.str();
                } else {
                    std::stringstream ss;
                    ss << "(" << std::setfill('0') << std::setw(4) << i << ")";
                    entry.fileName = ss.str();
                }
                
                file.seekg(savePos, std::ios::beg);
            } else {
                // Type != 0x02: no filenames, just numbered
                std::stringstream ss;
                ss << "(" << std::setfill('0') << std::setw(4) << i << ")";
                entry.fileName = ss.str();
            }

            info.files.push_back(entry);
        }

        return true;
    }

    bool PhyreOFS3Unpacker::extractAll(const std::string& archivePath, const std::string& outputDirectory, bool verbose) {
        OFS3ArchiveInfo info;
        if (!openArchive(archivePath, info)) {
            if (verbose) {
                std::cerr << "Failed to open archive: " << m_lastError << std::endl;
            }
            return false;
        }

        // Create output directory
        std::filesystem::create_directories(outputDirectory);

        if (verbose) {
            std::cout << "Archive Info:" << std::endl;
            std::cout << "  Magic: " << info.magic << std::endl;
            std::cout << "  Header Size: 0x" << std::hex << info.headerSize << std::dec << std::endl;
            std::cout << "  Type: 0x" << std::hex << info.type << std::dec << std::endl;
            std::cout << "  Padding: 0x" << std::hex << (int)info.padding << std::dec << std::endl;
            std::cout << "  SubType: 0x" << std::hex << (int)info.subType << std::dec << std::endl;
            std::cout << "  File Count: " << info.fileCount << std::endl;
            std::cout << "  Found " << info.files.size() << " file entries" << std::endl;
        }

        // Extract each file
        for (size_t i = 0; i < info.files.size(); ++i) {
            const auto& entry = info.files[i];
            
            // Construct output path
            std::filesystem::path outputPath = std::filesystem::path(outputDirectory) / entry.fileName;
            
            // Create subdirectories if needed
            std::filesystem::create_directories(outputPath.parent_path());

            if (verbose) {
                std::cout << "Extracting [" << std::setw(4) << (i+1) << "/" << info.files.size() 
                          << "] " << entry.fileName 
                          << " (offset=0x" << std::hex << entry.offset << std::dec
                          << ", size=" << entry.size << " bytes)" << std::endl;
            }

            if (!extractFile(archivePath, static_cast<uint32_t>(i), outputPath.string(), info, verbose)) {
                if (verbose) {
                    std::cerr << "  Warning: Failed to extract " << entry.fileName << ": " << m_lastError << std::endl;
                }
            }
        }

        return true;
    }

    bool PhyreOFS3Unpacker::extractFile(const std::string& archivePath, uint32_t fileIndex, const std::string& outputPath) {
        OFS3ArchiveInfo info;
        if (!openArchive(archivePath, info)) {
            return false;
        }
        return extractFile(archivePath, fileIndex, outputPath, info, false);
        }

    bool PhyreOFS3Unpacker::extractFile(const std::string& archivePath, uint32_t fileIndex, 
                                       const std::string& outputPath, const OFS3ArchiveInfo& info, bool verbose) {
        if (fileIndex >= info.files.size()) {
            m_lastError = "File index out of range";
            return false;
        }

        const auto& entry = info.files[fileIndex];
        
        std::ifstream inFile(archivePath, std::ios::binary);
        if (!inFile.is_open()) {
            m_lastError = "Cannot open archive file";
            return false;
        }

        inFile.seekg(entry.offset, std::ios::beg);
        if (!inFile.good()) {
            m_lastError = "Cannot seek to file offset";
            return false;
        }

        // Read file data
        std::vector<uint8_t> fileData(entry.size);
        inFile.read(reinterpret_cast<char*>(fileData.data()), entry.size);
        
        if (inFile.gcount() != entry.size) {
            m_lastError = "Failed to read file data completely";
            return false;
        }

        // Auto-detect and add extension based on file content
        std::string finalOutputPath = outputPath;
        if (fileData.size() > 8) {
            // Check for OFS3 archive
            if (fileData[0] == 0x4F && fileData[1] == 0x46 && fileData[2] == 0x53 && fileData[3] == 0x33) {
                if (outputPath.find(".ofs3") == std::string::npos) {
                    finalOutputPath += ".ofs3";
                }
            }
            // Check for GZip compressed
            else if (info.type == 0 && fileData[0] == 0x1F && fileData[1] == 0x8B && fileData[2] == 0x08) {
                finalOutputPath += ".gz";
            }
            // Check for GMO (Game Model)
            else if (info.type == 0 && fileData[0] == 0x4F && fileData[1] == 0x4D && fileData[2] == 0x47 && 
                     fileData[3] == 0x2E && fileData[4] == 0x30 && fileData[5] == 0x30) {
                finalOutputPath += ".gmo";
            }
            // Check for GIM (Game Image)
            else if (info.type == 0 && fileData[0] == 0x4D && fileData[1] == 0x49 && fileData[2] == 0x47 && 
                     fileData[3] == 0x2E && fileData[4] == 0x30 && fileData[5] == 0x30) {
                finalOutputPath += ".gim";
            }
            // Check for TM2 (Texture 2)
            else if (info.type == 0 && fileData[0] == 0x54 && fileData[1] == 0x49 && fileData[2] == 0x4D && fileData[3] == 0x32) {
                finalOutputPath += ".tm2";
            }
            // Check for PM2 (Particle 2)
            else if (info.type == 0 && fileData[0] == 0x50 && fileData[1] == 0x49 && fileData[2] == 0x4D && fileData[3] == 0x32) {
                finalOutputPath += ".pm2";
            }
            // Check for PHYR (PhyreEngine cluster)
            else if (fileData[0] == 0x52 && fileData[1] == 0x59 && fileData[2] == 0x48 && fileData[3] == 0x50) {
                if (outputPath.find(".phyre") == std::string::npos) {
                    finalOutputPath += ".phyre";
                }
            }
        }

        // Check if it needs decompression
        if (entry.isCompressed && entry.compressedSize > 0) {
            std::vector<uint8_t> decompressedData;
            if (!decompressData(fileData.data(), entry.compressedSize, decompressedData, entry.size)) {
                m_lastError = "Failed to decompress file data";
                return false;
            }
            fileData = decompressedData;
        }

        // Write extracted file
        std::ofstream outFile(finalOutputPath, std::ios::binary);
        if (!outFile.is_open()) {
            m_lastError = "Cannot create output file: " + finalOutputPath;
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        if (!outFile.good()) {
            m_lastError = "Failed to write output file";
            return false;
        }

        if (verbose) {
            std::cout << "  Saved to: " << finalOutputPath << std::endl;
        }

        return true;
    }

    bool PhyreOFS3Unpacker::listFiles(const std::string& archivePath, std::vector<std::string>& fileList) {
        OFS3ArchiveInfo info;
        if (!openArchive(archivePath, info)) {
            return false;
        }

        fileList.clear();
        for (const auto& entry : info.files) {
            fileList.push_back(entry.fileName);
        }

        return true;
    }

    bool PhyreOFS3Unpacker::decompressData(const uint8_t* compressedData, uint32_t compressedSize,
                                           std::vector<uint8_t>& decompressedData, uint32_t decompressedSize) {
        // TODO: Implement decompression if needed
        // Common compression formats: zlib, lz4, lzo
        // For now, just copy if sizes match (no compression)
        if (compressedSize == decompressedSize) {
            decompressedData.assign(compressedData, compressedData + compressedSize);
            return true;
        }

        // Compression not implemented yet
        m_lastError = "Decompression not implemented";
        return false;
    }

    std::string PhyreOFS3Unpacker::getLastError() const {
        return m_lastError;
    }

} // namespace PhyreUnpacker
