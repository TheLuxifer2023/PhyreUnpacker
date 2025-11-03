#include "PhyreFileAnalyzer.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cmath>

namespace PhyreUnpacker {

    bool PhyreFileAnalyzer::analyzeFileHeader(const std::string& filePath, FileHeaderInfo& info) {
        // Initialize info structure
        info.isValidPhyreFile = false;
        info.hasPhyreMarker = false;
        info.fileSignature.clear();
        info.magicNumber.clear();
        info.phyreMarker = 0;
        info.headerSize = 0;
        info.platformID = 0;
        info.fileSize = 0;
        info.isLittleEndian = true;
        info.headerBytes.clear();
        info.isGzipCompressed = false;
        info.isZlibCompressed = false;
        info.mightBeCompressed = false;
        info.detectedFormat.clear();
        info.formatDescription.clear();
        info.entropy = 0.0;
        info.zeroBytes = 0;

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        info.fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (info.fileSize == 0) {
            return false;
        }

        // Read first 256 bytes for analysis
        size_t bytesToRead = (info.fileSize < 256) ? static_cast<size_t>(info.fileSize) : 256;
        info.headerBytes.resize(bytesToRead);
        file.read(reinterpret_cast<char*>(info.headerBytes.data()), bytesToRead);

        if (file.gcount() == 0) {
            return false;
        }

        // Create hex signature string
        std::ostringstream sigStream;
        sigStream << std::hex << std::uppercase << std::setfill('0');
        for (size_t i = 0; i < std::min<size_t>(16, info.headerBytes.size()); ++i) {
            sigStream << std::setw(2) << static_cast<unsigned>(info.headerBytes[i]);
            if (i < 15) sigStream << " ";
        }
        info.fileSignature = sigStream.str();

        // Check for magic numbers
        if (info.headerBytes.size() >= 4) {
            // Try to read as ASCII
            bool isAscii = true;
            for (size_t i = 0; i < 4; ++i) {
                if (info.headerBytes[i] < 32 || info.headerBytes[i] > 126) {
                    isAscii = false;
                    break;
                }
            }
            if (isAscii) {
                info.magicNumber = std::string(reinterpret_cast<const char*>(info.headerBytes.data()), 4);
            } else {
                std::ostringstream magicStream;
                magicStream << std::hex << std::uppercase << std::setfill('0');
                magicStream << std::setw(2) << static_cast<unsigned>(info.headerBytes[0]);
                magicStream << std::setw(2) << static_cast<unsigned>(info.headerBytes[1]);
                magicStream << std::setw(2) << static_cast<unsigned>(info.headerBytes[2]);
                magicStream << std::setw(2) << static_cast<unsigned>(info.headerBytes[3]);
                info.magicNumber = magicStream.str();
            }
        }

        // Check for Phyre marker 'PHYR' (0x50585952)
        const uint32_t PHYR_MARKER_LE = 0x52595850; // 'PHYR' in little-endian
        const uint32_t PHYR_MARKER_BE = 0x50585952; // 'PHYR' in big-endian

        // Check for RYHPT variant (5-byte magic: 'RYHP' + 'T')
        bool isRYHPT = false;
        if (info.headerBytes.size() >= 5) {
            if (memcmp(info.headerBytes.data(), "RYHPT", 5) == 0) {
                isRYHPT = true;
                info.hasPhyreMarker = true;
                info.isValidPhyreFile = true;
                info.isLittleEndian = true;
                info.phyreMarker = PHYR_MARKER_LE;
            }
        }

        if (!isRYHPT && info.headerBytes.size() >= sizeof(uint32_t)) {
            // Read as little-endian
            uint32_t markerLE = *reinterpret_cast<const uint32_t*>(info.headerBytes.data());
            // Read as big-endian (swap bytes)
            uint32_t markerBE = ((markerLE & 0xFF000000) >> 24) |
                               ((markerLE & 0x00FF0000) >> 8) |
                               ((markerLE & 0x0000FF00) << 8) |
                               ((markerLE & 0x000000FF) << 24);

            info.phyreMarker = markerLE;

            if (markerLE == PHYR_MARKER_LE) {
                info.hasPhyreMarker = true;
                info.isValidPhyreFile = true;
                info.isLittleEndian = true;
            } else if (markerBE == PHYR_MARKER_BE) {
                info.hasPhyreMarker = true;
                info.isValidPhyreFile = true;
                info.isLittleEndian = false;
                // Store swapped marker
                info.phyreMarker = markerBE;
            } else if (markerLE == PHYR_MARKER_BE) {
                // Big-endian file on little-endian system
                info.hasPhyreMarker = true;
                info.isValidPhyreFile = true;
                info.isLittleEndian = false;
                info.phyreMarker = markerLE;
            } else if (markerBE == PHYR_MARKER_LE) {
                // Little-endian file on big-endian system
                info.hasPhyreMarker = true;
                info.isValidPhyreFile = true;
                info.isLittleEndian = true;
                info.phyreMarker = markerBE;
            }
        }

        // If we have a valid Phyre marker, try to read header fields
        if (info.hasPhyreMarker && info.headerBytes.size() >= 20) {
            if (info.isLittleEndian) {
                size_t offset = isRYHPT ? 8 : 4; // RYHPT has extra 'T' + 3 padding bytes
                info.headerSize = *reinterpret_cast<const uint32_t*>(&info.headerBytes[offset]);
                // Skip packed namespace size
                size_t platformOffset = isRYHPT ? 12 : 12; // Platform ID at same offset for both
                if (info.headerBytes.size() >= 16) {
                    info.platformID = *reinterpret_cast<const uint32_t*>(&info.headerBytes[platformOffset]);
                }
            } else {
                // Big-endian - need to swap
                size_t offset = 4;
                uint32_t sizeBE = *reinterpret_cast<const uint32_t*>(&info.headerBytes[offset]);
                info.headerSize = ((sizeBE & 0xFF000000) >> 24) |
                                 ((sizeBE & 0x00FF0000) >> 8) |
                                 ((sizeBE & 0x0000FF00) << 8) |
                                 ((sizeBE & 0x000000FF) << 24);
                if (info.headerBytes.size() >= 16) {
                    uint32_t platformBE = *reinterpret_cast<const uint32_t*>(&info.headerBytes[12]);
                    info.platformID = ((platformBE & 0xFF000000) >> 24) |
                                     ((platformBE & 0x00FF0000) >> 8) |
                                     ((platformBE & 0x0000FF00) << 8) |
                                     ((platformBE & 0x000000FF) << 24);
                }
            }
        }

        // Check for compression formats
        if (info.headerBytes.size() >= 2) {
            // Check for GZIP (0x1f8b in big-endian, stored as 0x8b1f in little-endian)
            if (info.headerBytes[0] == 0x1f && info.headerBytes[1] == 0x8b) {
                info.isGzipCompressed = true;
                info.mightBeCompressed = true;
                info.detectedFormat = "GZIP";
                info.formatDescription = "GZIP compressed data";
            }
            
            // Check for ZLIB (0x789c or 0x78da - deflate)
            // ZLIB header: first byte is 0x78, second byte is method+flags
            if (info.headerBytes[0] == 0x78) {
                uint8_t methodFlags = info.headerBytes[1];
                uint8_t method = methodFlags & 0x0F;
                if (method == 8) { // deflate method (most common)
                    info.isZlibCompressed = true;
                    info.mightBeCompressed = true;
                    if (info.detectedFormat.empty()) {
                        info.detectedFormat = "ZLIB";
                        info.formatDescription = "ZLIB compressed data (deflate)";
                    }
                }
            }
        }
        
        // Check for other known formats
        if (info.headerBytes.size() >= 4 && info.detectedFormat.empty()) {
            // DDS texture (DirectDraw Surface)
            if (memcmp(info.headerBytes.data(), "DDS ", 4) == 0) {
                info.detectedFormat = "DDS";
                info.formatDescription = "DirectDraw Surface texture";
            }
            // WAV audio
            else if (memcmp(info.headerBytes.data(), "RIFF", 4) == 0 && 
                     info.headerBytes.size() >= 8 &&
                     memcmp(&info.headerBytes[8], "WAVE", 4) == 0) {
                info.detectedFormat = "WAV";
                info.formatDescription = "WAV audio file";
            }
            // PNG image
            else if (info.headerBytes.size() >= 8 &&
                     memcmp(info.headerBytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0) {
                info.detectedFormat = "PNG";
                info.formatDescription = "PNG image";
            }
            // JPEG
            else if (info.headerBytes.size() >= 3 &&
                     info.headerBytes[0] == 0xFF && 
                     info.headerBytes[1] == 0xD8 && 
                     info.headerBytes[2] == 0xFF) {
                info.detectedFormat = "JPEG";
                info.formatDescription = "JPEG image";
            }
            // TGA
            else if (info.headerBytes.size() >= 18) {
                // TGA files often start with variable length fields, but check common pattern
                uint8_t idLength = info.headerBytes[0];
                uint8_t colorMapType = info.headerBytes[1];
                uint8_t imageType = info.headerBytes[2];
                if (idLength <= 255 && colorMapType <= 1 && imageType <= 11) {
                    info.detectedFormat = "TGA";
                    info.formatDescription = "Targa image (possible)";
                }
            }
        }
        
        // Calculate entropy and zero bytes for compression detection
        if (info.headerBytes.size() > 0) {
            uint32_t byteFreq[256] = {0};
            info.zeroBytes = 0;
            
            for (size_t i = 0; i < info.headerBytes.size(); ++i) {
                byteFreq[info.headerBytes[i]]++;
                if (info.headerBytes[i] == 0) {
                    info.zeroBytes++;
                }
            }
            
            // Calculate Shannon entropy
            double entropy = 0.0;
            double size = static_cast<double>(info.headerBytes.size());
            for (int i = 0; i < 256; ++i) {
                if (byteFreq[i] > 0) {
                    double prob = static_cast<double>(byteFreq[i]) / size;
                    entropy -= prob * log2(prob);
                }
            }
            info.entropy = entropy;
            
            // High entropy (>7.5 bits) suggests compressed or encrypted data
            if (!info.mightBeCompressed && entropy > 7.5 && info.detectedFormat.empty()) {
                info.mightBeCompressed = true;
                info.detectedFormat = "UNKNOWN_COMPRESSED";
                info.formatDescription = "Possible compressed/encrypted data (high entropy)";
            }
        }

        return true;
    }

    bool PhyreFileAnalyzer::canLoadWithPhyreEngine(const std::string& filePath) {
        FileHeaderInfo info;
        if (!analyzeFileHeader(filePath, info)) {
            return false;
        }
        return info.isValidPhyreFile;
    }

    void PhyreFileAnalyzer::printAnalysisReport(const std::string& filePath, const FileHeaderInfo& info) {
        std::cout << "=== File Header Analysis ===" << std::endl;
        std::cout << "File Size: " << info.fileSize << " bytes (" 
                  << std::fixed << std::setprecision(2) << (info.fileSize / 1024.0) << " KB)" << std::endl;
        std::cout << "File Signature (first 16 bytes): " << info.fileSignature << std::endl;
        std::cout << "Magic Number: " << info.magicNumber;
        if (!info.magicNumber.empty() && info.magicNumber.length() == 4 && 
            info.magicNumber[0] >= 32 && info.magicNumber[0] <= 126) {
            std::cout << " ('" << info.magicNumber << "')";
            
            // Check for known formats
            if (info.magicNumber == "OFS3") {
                std::cout << " [OFS3 Archive Format]";
            } else if (info.magicNumber == "PK\x03\x04") {
                std::cout << " [ZIP Archive]";
            } else if (info.magicNumber == "RAR") {
                std::cout << " [RAR Archive]";
            } else if (info.magicNumber == "7z\xBC\xAF") {
                std::cout << " [7-Zip Archive]";
            }
        }
        std::cout << std::endl;

        std::cout << "Phyre Marker: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(8) << info.phyreMarker << std::dec << std::endl;
        std::cout << "Has Phyre Marker: " << (info.hasPhyreMarker ? "YES" : "NO") << std::endl;
        std::cout << "Is Valid Phyre File: " << (info.isValidPhyreFile ? "YES" : "NO") << std::endl;
        std::cout << "Endianness: " << (info.isLittleEndian ? "Little-Endian" : "Big-Endian") << std::endl;

        // Analyze OFS3 format if detected
        if (info.magicNumber == "OFS3" && info.headerBytes.size() >= 16) {
            std::cout << std::endl;
            std::cout << "=== OFS3 Archive Analysis ===" << std::endl;
            
            // Parse OFS3 header structure
            // Based on signature: 4F 46 53 33 10 00 00 00 02 00 40 00 30 83 1B 00
            // OFS3 + version(?) + flags(?) + entries(?) + data_offset(?)
            
            uint32_t version = *reinterpret_cast<const uint32_t*>(&info.headerBytes[4]);
            uint32_t flags = *reinterpret_cast<const uint32_t*>(&info.headerBytes[8]);
            uint32_t entryCount = *reinterpret_cast<const uint32_t*>(&info.headerBytes[12]);
            
            std::cout << "OFS3 Version/Flags: 0x" << std::hex << std::uppercase << std::setfill('0') 
                      << std::setw(8) << version << std::dec << std::endl;
            std::cout << "Flags/Data: 0x" << std::hex << std::uppercase << std::setfill('0') 
                      << std::setw(8) << flags << std::dec << std::endl;
            std::cout << "Possible Entry Count/Offset: 0x" << std::hex << std::uppercase << std::setfill('0') 
                      << std::setw(8) << entryCount << " (" << std::dec << entryCount << ")" << std::endl;
            
            std::cout << std::endl;
            std::cout << ">>> This appears to be an OFS3 (Object File System 3) archive." << std::endl;
            std::cout << ">>> Files need to be extracted from the archive before PhyreEngine can load them." << std::endl;
            std::cout << ">>> The .l2b extension likely means 'Level 2 Binary' - a packed level file." << std::endl;
        }

        // Search for embedded PHYR markers inside the file
        std::vector<uint64_t> phyreMarkerOffsets;
        size_t markerCount = searchForPhyreMarkers(filePath, phyreMarkerOffsets);
        
        if (markerCount > 0) {
            std::cout << std::endl;
            std::cout << "=== Embedded Phyre Files Detected ===" << std::endl;
            std::cout << "Found " << markerCount << " PHYR marker(s) inside the file:" << std::endl;
            for (size_t i = 0; i < phyreMarkerOffsets.size() && i < 10; ++i) { // Limit to first 10
                std::cout << "  Marker " << (i+1) << " at offset 0x" << std::hex << std::uppercase 
                          << std::setfill('0') << std::setw(8) << phyreMarkerOffsets[i] 
                          << " (" << std::dec << phyreMarkerOffsets[i] << ")" << std::endl;
            }
            if (phyreMarkerOffsets.size() > 10) {
                std::cout << "  ... and " << (phyreMarkerOffsets.size() - 10) << " more marker(s)" << std::endl;
            }
            std::cout << ">>> This file may contain embedded PhyreEngine assets." << std::endl;
            std::cout << ">>> You may need to extract data starting from these offsets." << std::endl;
        } else if (!info.isValidPhyreFile && info.magicNumber != "OFS3") {
            // No PHYR markers found - this might be raw data
            std::cout << std::endl;
            std::cout << "=== Data Type Analysis ===" << std::endl;
            std::cout << "No PHYR markers found inside the file." << std::endl;
            std::cout << "This file likely contains raw binary data, not PhyreEngine cluster files." << std::endl;
            
            // Analyze what kind of raw data this might be
            if (info.zeroBytes > 100) {
                std::cout << "High number of zero bytes (" << info.zeroBytes << "/" << info.headerBytes.size() 
                          << ") suggests sparse data or padding." << std::endl;
            }
            if (info.entropy < 3.0) {
                std::cout << "Low entropy (" << std::fixed << std::setprecision(2) << info.entropy 
                          << " bits) suggests structured/repeating data (e.g., geometry, textures, or indices)." << std::endl;
            } else if (info.entropy > 7.5) {
                std::cout << "High entropy (" << std::fixed << std::setprecision(2) << info.entropy 
                          << " bits) suggests compressed or encrypted data." << std::endl;
            }
            
            std::cout << std::endl;
            std::cout << "Possible data types:" << std::endl;
            std::cout << "  - Raw texture data (DXT, BC formats, uncompressed)" << std::endl;
            std::cout << "  - Geometry data (vertex/index buffers)" << std::endl;
            std::cout << "  - Audio samples (WAV, compressed audio)" << std::endl;
            std::cout << "  - Metadata or index tables" << std::endl;
            std::cout << "  - Shader bytecode or compiled resources" << std::endl;
        }

        // Compression and format detection
        if (info.isGzipCompressed || info.isZlibCompressed || info.mightBeCompressed || !info.detectedFormat.empty()) {
            std::cout << std::endl;
            std::cout << "=== Compression/Format Detection ===" << std::endl;
            if (info.isGzipCompressed) {
                std::cout << "Compression: GZIP detected (0x1f8b header)" << std::endl;
            }
            if (info.isZlibCompressed) {
                std::cout << "Compression: ZLIB detected (deflate method)" << std::endl;
            }
            if (!info.detectedFormat.empty()) {
                std::cout << "Detected Format: " << info.detectedFormat;
                if (!info.formatDescription.empty()) {
                    std::cout << " - " << info.formatDescription;
                }
                std::cout << std::endl;
            }
            if (info.mightBeCompressed && !info.isGzipCompressed && !info.isZlibCompressed) {
                std::cout << "Compression: Possibly compressed (high entropy: " 
                          << std::fixed << std::setprecision(2) << info.entropy << " bits)" << std::endl;
            }
            std::cout << "Entropy: " << std::fixed << std::setprecision(2) << info.entropy 
                      << " bits (8.0 = maximum randomness)" << std::endl;
            std::cout << "Zero bytes in header: " << info.zeroBytes << " / " 
                      << info.headerBytes.size() << std::endl;
        }

        if (info.isValidPhyreFile) {
            std::cout << std::endl;
            std::cout << "Header Size: " << info.headerSize << " bytes" << std::endl;
            std::cout << "Platform ID: 0x" << std::hex << std::uppercase << std::setfill('0') 
                      << std::setw(8) << info.platformID;
            
            // Decode platform ID
            if (info.platformID == 0x44313158) { // 'X11D' = D3D11
                std::cout << " (D3D11)";
            } else if (info.platformID == 0x4C472020) { // '  GL' = OpenGL
                std::cout << " (OpenGL)";
            } else if (info.platformID == 0x4D4E4720) { // ' GNM' = GNM (PS4)
                std::cout << " (GNM/PS4)";
            }
            std::cout << std::dec << std::endl;
            std::cout << ">>> File CAN be loaded by PhyreEngine" << std::endl;
        } else {
            if (info.magicNumber != "OFS3") {
                std::cout << std::endl;
                std::cout << ">>> File CANNOT be loaded directly by PhyreEngine" << std::endl;
                
                // Provide more helpful information based on detection
                if (info.isGzipCompressed || info.isZlibCompressed) {
                    std::cout << "   This file is compressed (GZIP/ZLIB)." << std::endl;
                    std::cout << "   Decompress it first before analyzing with PhyreEngine." << std::endl;
                } else if (!info.detectedFormat.empty() && 
                          (info.detectedFormat == "DDS" || info.detectedFormat == "WAV" || 
                           info.detectedFormat == "PNG" || info.detectedFormat == "JPEG")) {
                    std::cout << "   This appears to be a " << info.detectedFormat 
                              << " file, not a PhyreEngine asset." << std::endl;
                } else {
                    std::cout << "   This might be an archive or compressed format that needs extraction first." << std::endl;
                }
            }
        }

        std::cout << std::endl;
    }

    size_t PhyreFileAnalyzer::searchForPhyreMarkers(const std::string& filePath, std::vector<uint64_t>& offsets) {
        offsets.clear();
        
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return 0;
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (fileSize < 4) {
            return 0;
        }
        
        // Search for PHYR marker (both endianness)
        const uint32_t PHYR_LE = 0x52595850; // 'PHYR' in little-endian ('RYHP')
        const uint32_t PHYR_BE = 0x50585952; // 'PHYR' in big-endian
        
        // Read file in chunks to avoid loading entire file into memory
        const size_t chunkSize = 64 * 1024; // 64KB chunks
        std::vector<uint8_t> buffer(chunkSize + 3); // +3 for overlapping search at boundaries
        
        uint64_t fileOffset = 0;
        uint8_t overlap[3] = {0}; // Store last 3 bytes from previous chunk
        bool hasOverlap = false;
        
        while (fileOffset < fileSize) {
            size_t bytesToRead = static_cast<size_t>(std::min<uint64_t>(chunkSize, fileSize - fileOffset));
            
            // Copy overlap to start of buffer
            if (hasOverlap) {
                std::memcpy(buffer.data(), overlap, 3);
            }
            
            // Read new chunk
            size_t readOffset = hasOverlap ? 3 : 0;
            file.read(reinterpret_cast<char*>(buffer.data() + readOffset), bytesToRead);
            size_t bytesRead = file.gcount();
            
            if (bytesRead == 0) {
                break;
            }
            
            // Save last 3 bytes for next iteration
            if (bytesRead >= 3) {
                std::memcpy(overlap, buffer.data() + readOffset + bytesRead - 3, 3);
                hasOverlap = true;
            }
            
            size_t totalSize = readOffset + bytesRead;
            size_t searchStart = 0;
            
            // Search for PHYR markers in current buffer
            for (size_t i = searchStart; i + 4 <= totalSize; ++i) {
                uint32_t value = *reinterpret_cast<const uint32_t*>(&buffer[i]);
                
                if (value == PHYR_LE || value == PHYR_BE) {
                    uint64_t absoluteOffset = fileOffset + i - readOffset;
                    offsets.push_back(absoluteOffset);
                }
            }
            
            fileOffset += bytesRead;
        }
        
        return offsets.size();
    }

} // namespace PhyreUnpacker
