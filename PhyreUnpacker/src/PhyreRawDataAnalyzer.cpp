#include "PhyreRawDataAnalyzer.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace PhyreUnpacker {

    bool PhyreRawDataAnalyzer::analyzeRawData(const std::string& filePath, RawDataInfo& info) {
        // Initialize info
        info.dataType = RawDataInfo::TYPE_UNKNOWN;
        info.dataSubType.clear();
        info.textureWidth = 0;
        info.textureHeight = 0;
        info.textureFormat = 0;
        info.mipmapCount = 0;
        info.textureFormatName.clear();
        info.vertexCount = 0;
        info.indexCount = 0;
        info.vertexStride = 0;
        info.hasNormals = false;
        info.hasUVs = false;
        info.dataOffset = 0;
        info.dataSize = 0;
        info.isLittleEndian = true;
        info.sampleData.clear();

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize == 0) {
            return false;
        }

        info.dataSize = fileSize;

        // Read sample data (first 4KB or whole file if smaller)
        size_t sampleSize = std::min<size_t>(4096, static_cast<size_t>(fileSize));
        info.sampleData.resize(sampleSize);
        file.read(reinterpret_cast<char*>(info.sampleData.data()), sampleSize);
        file.seekg(0, std::ios::beg);

        // Try to detect data type
        if (detectTexture(info.sampleData.data(), sampleSize, info)) {
            info.dataType = RawDataInfo::TYPE_TEXTURE;
            return true;
        }

        if (detectGeometry(info.sampleData.data(), sampleSize, info)) {
            info.dataType = RawDataInfo::TYPE_GEOMETRY;
            return true;
        }

        if (detectMetadata(info.sampleData.data(), sampleSize, info)) {
            info.dataType = RawDataInfo::TYPE_METADATA;
            return true;
        }

        // If all detection fails, it's unknown
        return true;
    }

    bool PhyreRawDataAnalyzer::detectTexture(const uint8_t* data, size_t dataSize, RawDataInfo& info) {
        if (dataSize < 16) {
            return false;
        }

        // Check for DDS header
        if (dataSize >= 128 && memcmp(data, "DDS ", 4) == 0) {
            info.dataSubType = "DDS";
            info.textureFormatName = "DDS Texture";
            // Parse DDS header
            uint32_t height = *reinterpret_cast<const uint32_t*>(&data[12]);
            uint32_t width = *reinterpret_cast<const uint32_t*>(&data[16]);
            info.textureWidth = width;
            info.textureHeight = height;
            info.dataOffset = 128; // DDS header size
            return true;
        }

        // Try to detect texture dimensions from patterns
        // Look for power-of-2 dimensions that might indicate a texture
        // Check if size matches common texture formats
        
        // Calculate possible texture dimensions from file size
        // Common formats: RGBA8 = 4 bytes per pixel, DXT1 = 0.5 bytes per pixel, DXT5 = 1 byte per pixel
        
        for (uint32_t width = 64; width <= 4096; width *= 2) {
            for (uint32_t height = 64; height <= 4096; height *= 2) {
                uint64_t rgba8Size = width * height * 4;
                uint64_t dxt1Size = ((width + 3) / 4) * ((height + 3) / 4) * 8;
                uint64_t dxt5Size = ((width + 3) / 4) * ((height + 3) / 4) * 16;
                
                // Allow some variance for mipmaps or headers
                int64_t diffRGBA = static_cast<int64_t>(info.dataSize) - static_cast<int64_t>(rgba8Size);
                int64_t diffDXT1 = static_cast<int64_t>(info.dataSize) - static_cast<int64_t>(dxt1Size);
                int64_t diffDXT5 = static_cast<int64_t>(info.dataSize) - static_cast<int64_t>(dxt5Size);
                
                if ((diffRGBA >= 0 ? diffRGBA : -diffRGBA) < static_cast<int64_t>(rgba8Size * 0.1) ||
                    (diffDXT1 >= 0 ? diffDXT1 : -diffDXT1) < static_cast<int64_t>(dxt1Size * 0.1) ||
                    (diffDXT5 >= 0 ? diffDXT5 : -diffDXT5) < static_cast<int64_t>(dxt5Size * 0.1)) {
                    info.textureWidth = width;
                    info.textureHeight = height;
                    info.dataSubType = "RawTexture";
                    info.textureFormatName = "Possible raw texture data";
                    
                    // Try to detect format from size
                    if ((diffDXT1 >= 0 ? diffDXT1 : -diffDXT1) < static_cast<int64_t>(dxt1Size * 0.1)) {
                        info.textureFormatName = "Possible DXT1/BC1";
                    } else if ((diffDXT5 >= 0 ? diffDXT5 : -diffDXT5) < static_cast<int64_t>(dxt5Size * 0.1)) {
                        info.textureFormatName = "Possible DXT5/BC3";
                    } else {
                        info.textureFormatName = "Possible RGBA8";
                    }
                    
                    return true;
                }
            }
        }

        // Check for patterns that look like compressed texture data (DXT/BC formats)
        // DXT blocks are 4x4 pixels, so data should be organized in blocks
        // DXT1: 8 bytes per block (4x4 pixels) = 0.5 bytes per pixel
        // DXT5: 16 bytes per block (4x4 pixels) = 1 byte per pixel
        if (info.dataSize > 1024) {
            // Try different header sizes (0, 16, 32, 64, 128 bytes)
            uint32_t possibleHeaders[] = {0, 16, 32, 64, 128};
            
            for (uint32_t headerSize : possibleHeaders) {
                if (headerSize >= info.dataSize) continue;
                
                uint64_t textureDataSize = info.dataSize - headerSize;
                uint32_t blockCount8 = static_cast<uint32_t>(textureDataSize / 8);   // DXT1 blocks
                uint32_t blockCount16 = static_cast<uint32_t>(textureDataSize / 16); // DXT5 blocks
                
                // Try DXT1 dimensions (allow for small rounding errors)
                if (textureDataSize % 8 <= 8 && blockCount8 > 0) {
                    // First, try to estimate dimensions from block count
                    // For square textures: blocks = (w/4)^2, so w = sqrt(blocks) * 4
                    uint32_t estimatedSquareSize = static_cast<uint32_t>(std::sqrt(static_cast<double>(blockCount8))) * 4;
                    
                    // Round to nearest multiple of 4
                    estimatedSquareSize = ((estimatedSquareSize + 2) / 4) * 4;
                    
                    // Check if estimated square size gives correct block count
                    if (estimatedSquareSize >= 64 && estimatedSquareSize <= 4096) {
                        uint32_t blocksW = (estimatedSquareSize + 3) / 4;
                        uint32_t blocksH = blocksW;
                        uint32_t totalBlocks = blocksW * blocksH;
                        
                        uint32_t diff = (totalBlocks > blockCount8) ? (totalBlocks - blockCount8) : (blockCount8 - totalBlocks);
                        // Allow up to 5% difference for non-square textures or rounding
                        if (diff <= blockCount8 * 0.05 || diff <= 10) {
                            info.textureWidth = estimatedSquareSize;
                            info.textureHeight = estimatedSquareSize;
                            info.dataOffset = headerSize;
                            info.dataSubType = "CompressedTexture";
                            if (headerSize > 0) {
                                info.textureFormatName = "Possible DXT1/BC1 compressed texture (estimated " + 
                                                          std::to_string(estimatedSquareSize) + "x" + std::to_string(estimatedSquareSize) +
                                                          ", header: " + std::to_string(headerSize) + " bytes)";
                            } else {
                                info.textureFormatName = "Possible DXT1/BC1 compressed texture (estimated " +
                                                          std::to_string(estimatedSquareSize) + "x" + std::to_string(estimatedSquareSize) + ")";
                            }
                            return true;
                        }
                    }
                    
                    // Find width and height that match: (w/4) * (h/4) = blockCount
                    // w and h must be multiples of 4 and powers of 2 (usually)
                    for (uint32_t width = 64; width <= 4096; width *= 2) {
                        for (uint32_t height = 64; height <= 4096; height *= 2) {
                            uint32_t blocksW = (width + 3) / 4;
                            uint32_t blocksH = (height + 3) / 4;
                            uint32_t totalBlocks = blocksW * blocksH;
                            
                            // Allow small difference for non-power-of-2 textures
                            uint32_t diff = (totalBlocks > blockCount8) ? (totalBlocks - blockCount8) : (blockCount8 - totalBlocks);
                            if (diff <= 2) { // Allow up to 2 block difference
                                info.textureWidth = width;
                                info.textureHeight = height;
                                info.dataOffset = headerSize;
                                info.dataSubType = "CompressedTexture";
                                if (headerSize > 0) {
                                    info.textureFormatName = "Possible DXT1/BC1 compressed texture (header: " + 
                                                              std::to_string(headerSize) + " bytes)";
                                } else {
                                    info.textureFormatName = "Possible DXT1/BC1 compressed texture";
                                }
                                return true;
                            }
                        }
                    }
                
                // Try square textures and common aspect ratios
                for (uint32_t width = 64; width <= 4096; width *= 2) {
                    // Square
                    uint32_t height = width;
                    uint32_t blocksW = (width + 3) / 4;
                    uint32_t blocksH = (height + 3) / 4;
                    uint32_t totalBlocks = blocksW * blocksH;
                    
                    uint32_t diff = (totalBlocks > blockCount8) ? (totalBlocks - blockCount8) : (blockCount8 - totalBlocks);
                    if (diff <= 3) {
                        info.textureWidth = width;
                        info.textureHeight = height;
                        info.dataOffset = headerSize;
                        info.dataSubType = "CompressedTexture";
                        if (headerSize > 0) {
                            info.textureFormatName = "Possible DXT1/BC1 compressed texture (estimated, header: " + 
                                                      std::to_string(headerSize) + " bytes)";
                        } else {
                            info.textureFormatName = "Possible DXT1/BC1 compressed texture (estimated)";
                        }
                        return true;
                    }
                    
                    // 2:1 aspect ratio
                    height = width / 2;
                    if (height >= 64) {
                        blocksH = (height + 3) / 4;
                        totalBlocks = blocksW * blocksH;
                        diff = (totalBlocks > blockCount8) ? (totalBlocks - blockCount8) : (blockCount8 - totalBlocks);
                        if (diff <= 3) {
                            info.textureWidth = width;
                            info.textureHeight = height;
                            info.dataOffset = headerSize;
                            info.dataSubType = "CompressedTexture";
                            if (headerSize > 0) {
                                info.textureFormatName = "Possible DXT1/BC1 compressed texture (estimated, header: " + 
                                                          std::to_string(headerSize) + " bytes)";
                            } else {
                                info.textureFormatName = "Possible DXT1/BC1 compressed texture (estimated)";
                            }
                            return true;
                        }
                    }
                }
                } // End if (textureDataSize % 8...)
            } // End for (headerSize) for DXT1
            
            // Try DXT5 dimensions
            for (uint32_t headerSize : possibleHeaders) {
                if (headerSize >= info.dataSize) continue;
                
                uint64_t textureDataSize = info.dataSize - headerSize;
                uint32_t blockCount16 = static_cast<uint32_t>(textureDataSize / 16);
                
                if (textureDataSize % 16 <= 16 && blockCount16 > 0) {
                    // First, try to estimate dimensions from block count for DXT5
                    uint32_t estimatedSquareSize = static_cast<uint32_t>(std::sqrt(static_cast<double>(blockCount16))) * 4;
                    estimatedSquareSize = ((estimatedSquareSize + 2) / 4) * 4;
                    
                    // Check if estimated square size gives correct block count
                    if (estimatedSquareSize >= 64 && estimatedSquareSize <= 4096) {
                        uint32_t blocksW = (estimatedSquareSize + 3) / 4;
                        uint32_t blocksH = blocksW;
                        uint32_t totalBlocks = blocksW * blocksH;
                        
                        uint32_t diff = (totalBlocks > blockCount16) ? (totalBlocks - blockCount16) : (blockCount16 - totalBlocks);
                        // Allow up to 5% difference
                        if (diff <= blockCount16 * 0.05 || diff <= 10) {
                            info.textureWidth = estimatedSquareSize;
                            info.textureHeight = estimatedSquareSize;
                            info.dataOffset = headerSize;
                            info.dataSubType = "CompressedTexture";
                            if (headerSize > 0) {
                                info.textureFormatName = "Possible DXT5/BC3 compressed texture (estimated " + 
                                                          std::to_string(estimatedSquareSize) + "x" + std::to_string(estimatedSquareSize) +
                                                          ", header: " + std::to_string(headerSize) + " bytes)";
                            } else {
                                info.textureFormatName = "Possible DXT5/BC3 compressed texture (estimated " +
                                                          std::to_string(estimatedSquareSize) + "x" + std::to_string(estimatedSquareSize) + ")";
                            }
                            return true;
                        }
                    }
                    
                    for (uint32_t width = 64; width <= 4096; width *= 2) {
                        for (uint32_t height = 64; height <= 4096; height *= 2) {
                            uint32_t blocksW = (width + 3) / 4;
                            uint32_t blocksH = (height + 3) / 4;
                            uint32_t totalBlocks = blocksW * blocksH;
                            
                            uint32_t diff = (totalBlocks > blockCount16) ? (totalBlocks - blockCount16) : (blockCount16 - totalBlocks);
                            if (diff <= 2) {
                                info.textureWidth = width;
                                info.textureHeight = height;
                                info.dataOffset = headerSize;
                                info.dataSubType = "CompressedTexture";
                                if (headerSize > 0) {
                                    info.textureFormatName = "Possible DXT5/BC3 compressed texture (header: " + 
                                                              std::to_string(headerSize) + " bytes)";
                                } else {
                                    info.textureFormatName = "Possible DXT5/BC3 compressed texture";
                                }
                                return true;
                            }
                        }
                    }
                
                // Try square textures for DXT5
                for (uint32_t width = 64; width <= 4096; width *= 2) {
                    uint32_t height = width;
                    uint32_t blocksW = (width + 3) / 4;
                    uint32_t blocksH = (height + 3) / 4;
                    uint32_t totalBlocks = blocksW * blocksH;
                    
                    uint32_t diff = (totalBlocks > blockCount16) ? (totalBlocks - blockCount16) : (blockCount16 - totalBlocks);
                    if (diff <= 3) {
                        info.textureWidth = width;
                        info.textureHeight = height;
                        info.dataOffset = headerSize;
                        info.dataSubType = "CompressedTexture";
                        if (headerSize > 0) {
                            info.textureFormatName = "Possible DXT5/BC3 compressed texture (estimated, header: " + 
                                                      std::to_string(headerSize) + " bytes)";
                        } else {
                            info.textureFormatName = "Possible DXT5/BC3 compressed texture (estimated)";
                        }
                        return true;
                    }
                }
                } // End if (textureDataSize % 16...)
            } // End for (headerSize) for DXT5
            
            // Fallback: just indicate it's compressed texture without dimensions
            // Check if size suggests DXT format even if dimensions can't be determined
            uint64_t remainder8 = info.dataSize % 8;
            uint64_t remainder16 = info.dataSize % 16;
            if (remainder8 <= 8 || remainder16 <= 16) {
                info.dataSubType = "CompressedTexture";
                info.textureFormatName = "Possible DXT/BC compressed texture (dimensions unknown)";
                return true;
            }
        }

        return false;
    }

    bool PhyreRawDataAnalyzer::detectGeometry(const uint8_t* data, size_t dataSize, RawDataInfo& info) {
        if (dataSize < 32) {
            return false;
        }

        // Look for patterns that suggest vertex/index data
        // Common vertex formats: 12 bytes (3 floats for position), 32 bytes (position + normal + UV), etc.
        
        // Check if data size is divisible by common vertex stride sizes
        std::vector<uint32_t> commonStrides = {12, 20, 24, 32, 36, 40, 44, 48, 56, 64};
        
        for (uint32_t stride : commonStrides) {
            if (info.dataSize % stride == 0) {
                uint32_t vertexCount = static_cast<uint32_t>(info.dataSize / stride);
                
                // Check if vertex count is reasonable (not too small, not too large)
                if (vertexCount >= 3 && vertexCount <= 1000000) {
                    // Analyze sample data to see if values look like vertex coordinates
                    // Vertex coordinates are usually in reasonable ranges (e.g., -100 to 100)
                    bool looksLikeVertices = true;
                    size_t samplesToCheck = std::min<size_t>(100, vertexCount);
                    
                    for (size_t i = 0; i < samplesToCheck && i * stride + 12 <= dataSize; ++i) {
                        const float* pos = reinterpret_cast<const float*>(&data[i * stride]);
                        float x = pos[0], y = pos[1], z = pos[2];
                        
                        // Check if values are reasonable for vertex coordinates
                        // (not NaN, not Inf, in reasonable range)
                        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                            looksLikeVertices = false;
                            break;
                        }
                        
                        // Very large values might indicate indices or other data
                        if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) {
                            looksLikeVertices = false;
                            break;
                        }
                    }
                    
                    if (looksLikeVertices) {
                        info.vertexCount = vertexCount;
                        info.vertexStride = stride;
                        info.dataSubType = "VertexBuffer";
                        info.hasNormals = (stride >= 24); // Assume normals if stride >= 24
                        info.hasUVs = (stride >= 32); // Assume UVs if stride >= 32
                        return true;
                    }
                }
            }
        }

        // Check for index buffer (usually uint16 or uint32 arrays)
        if (info.dataSize % 2 == 0) {
            uint32_t indexCount16 = static_cast<uint32_t>(info.dataSize / 2);
            if (indexCount16 >= 3 && indexCount16 <= 10000000) {
                // Check if values look like indices (all less than some reasonable max)
                bool looksLikeIndices16 = true;
                size_t samplesToCheck = std::min<size_t>(1000, indexCount16);
                uint32_t maxIndex = 0;
                
                for (size_t i = 0; i < samplesToCheck && i * 2 + 2 <= dataSize; ++i) {
                    uint16_t idx = *reinterpret_cast<const uint16_t*>(&data[i * 2]);
                    maxIndex = std::max(maxIndex, static_cast<uint32_t>(idx));
                }
                
                if (maxIndex < indexCount16 && maxIndex > 0) {
                    info.indexCount = indexCount16;
                    info.dataSubType = "IndexBuffer16";
                    return true;
                }
            }
        }

        if (info.dataSize % 4 == 0) {
            uint32_t indexCount32 = static_cast<uint32_t>(info.dataSize / 4);
            if (indexCount32 >= 3 && indexCount32 <= 10000000) {
                bool looksLikeIndices32 = true;
                size_t samplesToCheck = std::min<size_t>(1000, indexCount32);
                uint32_t maxIndex = 0;
                
                for (size_t i = 0; i < samplesToCheck && i * 4 + 4 <= dataSize; ++i) {
                    uint32_t idx = *reinterpret_cast<const uint32_t*>(&data[i * 4]);
                    maxIndex = std::max(maxIndex, idx);
                }
                
                if (maxIndex < indexCount32 && maxIndex > 0) {
                    info.indexCount = indexCount32;
                    info.dataSubType = "IndexBuffer32";
                    return true;
                }
            }
        }

        return false;
    }

    bool PhyreRawDataAnalyzer::detectMetadata(const uint8_t* data, size_t dataSize, RawDataInfo& info) {
        if (dataSize < 16) {
            return false;
        }

        // Check for patterns that suggest metadata or index tables
        // Often metadata contains:
        // - Arrays of offsets (uint32 or uint64)
        // - Small structures with repeating patterns
        // - Low entropy (structured data)
        
        // Count zero bytes - metadata often has padding
        uint32_t zeroBytes = 0;
        size_t checkSize = std::min<size_t>(256, dataSize);
        for (size_t i = 0; i < checkSize; ++i) {
            if (data[i] == 0) {
                zeroBytes++;
            }
        }
        
        float zeroRatio = static_cast<float>(zeroBytes) / static_cast<float>(checkSize);
        
        // If >40% zeros, might be metadata with padding
        if (zeroRatio > 0.4f && dataSize < 100000) { // Small files with many zeros
            info.dataSubType = "Metadata";
            
            // Try to detect if it's an array of offsets
            if (dataSize % 4 == 0 || dataSize % 8 == 0) {
                uint32_t entryCount = static_cast<uint32_t>(dataSize / 4);
                if (entryCount >= 2 && entryCount <= 10000) {
                    // Check if values look like offsets (monotonically increasing or reasonable ranges)
                    bool looksLikeOffsets = true;
                    uint32_t prevValue = 0;
                    size_t samplesToCheck = std::min<size_t>(100, entryCount);
                    
                    for (size_t i = 0; i < samplesToCheck && i * 4 + 4 <= dataSize; ++i) {
                        uint32_t value = *reinterpret_cast<const uint32_t*>(&data[i * 4]);
                        if (value < prevValue && value != 0) {
                            // Not strictly increasing, but might still be offsets
                            if (value > 1000000) {
                                looksLikeOffsets = false;
                                break;
                            }
                        }
                        prevValue = value;
                    }
                    
                    if (looksLikeOffsets) {
                        info.dataSubType = "OffsetTable";
                        return true;
                    }
                }
            }
            
            return true;
        }

        return false;
    }

    void PhyreRawDataAnalyzer::printAnalysisReport(const RawDataInfo& info) {
        std::cout << "=== Raw Data Analysis ===" << std::endl;
        
        std::cout << "Data Type: ";
        switch (info.dataType) {
            case RawDataInfo::TYPE_TEXTURE:
                std::cout << "TEXTURE";
                break;
            case RawDataInfo::TYPE_GEOMETRY:
                std::cout << "GEOMETRY";
                break;
            case RawDataInfo::TYPE_METADATA:
                std::cout << "METADATA";
                break;
            case RawDataInfo::TYPE_AUDIO:
                std::cout << "AUDIO";
                break;
            case RawDataInfo::TYPE_INDEX_TABLE:
                std::cout << "INDEX_TABLE";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }
        std::cout << std::endl;
        
        if (!info.dataSubType.empty()) {
            std::cout << "Sub-Type: " << info.dataSubType << std::endl;
        }
        
        std::cout << "Data Size: " << info.dataSize << " bytes (" 
                  << std::fixed << std::setprecision(2) << (info.dataSize / 1024.0) << " KB)" << std::endl;
        
        if (info.dataType == RawDataInfo::TYPE_TEXTURE) {
            std::cout << "Texture Information:" << std::endl;
            if (info.textureWidth > 0 && info.textureHeight > 0) {
                std::cout << "  Dimensions: " << info.textureWidth << "x" << info.textureHeight << std::endl;
            }
            if (!info.textureFormatName.empty()) {
                std::cout << "  Format: " << info.textureFormatName << std::endl;
            }
            if (info.mipmapCount > 0) {
                std::cout << "  Mipmaps: " << info.mipmapCount << std::endl;
            }
        } else if (info.dataType == RawDataInfo::TYPE_GEOMETRY) {
            std::cout << "Geometry Information:" << std::endl;
            if (info.vertexCount > 0) {
                std::cout << "  Vertex Count: " << info.vertexCount << std::endl;
                std::cout << "  Vertex Stride: " << info.vertexStride << " bytes" << std::endl;
            }
            if (info.indexCount > 0) {
                std::cout << "  Index Count: " << info.indexCount << std::endl;
            }
            std::cout << "  Has Normals: " << (info.hasNormals ? "Yes" : "No") << std::endl;
            std::cout << "  Has UVs: " << (info.hasUVs ? "Yes" : "No") << std::endl;
        } else if (info.dataType == RawDataInfo::TYPE_METADATA) {
            std::cout << "Metadata Information:" << std::endl;
            std::cout << "  Type: " << info.dataSubType << std::endl;
        }
        
        std::cout << std::endl;
    }

} // namespace PhyreUnpacker

