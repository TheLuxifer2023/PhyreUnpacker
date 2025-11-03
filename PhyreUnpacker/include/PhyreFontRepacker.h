#pragma once

#include <string>
#include <vector>
#include <set>

// Forward declarations
namespace Phyre {
    typedef unsigned int PUInt32;
}

namespace PhyreUnpacker {

class PhyreFontRepacker {
public:
    PhyreFontRepacker();
    ~PhyreFontRepacker();

    // Initialize PhyreEngine SDK
    bool initializeSDK();
    
    // Repack font: modify existing font by replacing characters with those from a new .fgen file
    bool repackPhyre(const std::string &sourcePhyre,
                     const std::string &outPhyre,
                     const std::string &fgenPath,
                     bool onlyListed = false,
                     const std::string &overrideTtfName = "");

    // Get last error message
    std::string getLastError() const;

private:
    void cleanupPhyreEngine();
    
    bool m_sdkInitialized;
    std::string m_lastError;
};

} // namespace PhyreUnpacker
